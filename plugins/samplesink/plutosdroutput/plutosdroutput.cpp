///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGPlutoSdrOutputReport.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "plutosdr/deviceplutosdrparams.h"
#include "plutosdr/deviceplutosdrbox.h"

#include "plutosdroutput.h"
#include "plutosdroutputthread.h"

#define PLUTOSDR_BLOCKSIZE_SAMPLES (16*1024) //complex samples per buffer (must be multiple of 64)

MESSAGE_CLASS_DEFINITION(PlutoSDROutput::MsgConfigurePlutoSDR, Message)
MESSAGE_CLASS_DEFINITION(PlutoSDROutput::MsgStartStop, Message)

PlutoSDROutput::PlutoSDROutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_deviceDescription("PlutoSDROutput"),
    m_settings(),
    m_running(false),
    m_plutoTxBuffer(0),
    m_plutoSDROutputThread(0)
{
    m_deviceAPI->setNbSinkStreams(1);
    m_deviceSampleRates.m_addaConnvRate = 0;
    m_deviceSampleRates.m_bbRateHz = 0;
    m_deviceSampleRates.m_firRate = 0;
    m_deviceSampleRates.m_hb1Rate = 0;
    m_deviceSampleRates.m_hb2Rate = 0;
    m_deviceSampleRates.m_hb3Rate = 0;

    suspendBuddies();
    m_open = openDevice();

    if (!m_open) {
        qCritical("PlutoSDRInput::PlutoSDRInput: cannot open device");
    }

    resumeBuddies();

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PlutoSDROutput::networkManagerFinished
    );
}

PlutoSDROutput::~PlutoSDROutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PlutoSDROutput::networkManagerFinished
    );
    delete m_networkManager;

    suspendBuddies();
    closeDevice();
    resumeBuddies();
}

void PlutoSDROutput::destroy()
{
    delete this;
}

void PlutoSDROutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool PlutoSDROutput::start()
{
    if (!m_deviceShared.m_deviceParams->getBox())
    {
        qCritical("PlutoSDROutput::start: device not open");
        return false;
    }

    if (m_running) {
         stop();
    }

    // start / stop streaming is done in the thread.

    m_plutoSDROutputThread = new PlutoSDROutputThread(PLUTOSDR_BLOCKSIZE_SAMPLES, m_deviceShared.m_deviceParams->getBox(), &m_sampleSourceFifo);
    qDebug("PlutoSDROutput::start: thread created");

    applySettings(m_settings, QList<QString>(), true);

    m_plutoSDROutputThread->setLog2Interpolation(m_settings.m_log2Interp);
    m_plutoSDROutputThread->startWork();

    m_deviceShared.m_thread = m_plutoSDROutputThread;
    m_running = true;

    return true;
}

void PlutoSDROutput::stop()
{
    if (m_plutoSDROutputThread != 0)
    {
        m_plutoSDROutputThread->stopWork();
        delete m_plutoSDROutputThread;
        m_plutoSDROutputThread = 0;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;
}

QByteArray PlutoSDROutput::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDROutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePlutoSDR* message = MsgConfigurePlutoSDR::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDR* messageToGUI = MsgConfigurePlutoSDR::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& PlutoSDROutput::getDeviceDescription() const
{
    return m_deviceDescription;
}
int PlutoSDROutput::getSampleRate() const
{
    return (m_settings.m_devSampleRate / (1<<m_settings.m_log2Interp));
}

quint64 PlutoSDROutput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PlutoSDROutput::setCenterFrequency(qint64 centerFrequency)
{
    PlutoSDROutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigurePlutoSDR* message = MsgConfigurePlutoSDR::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDR* messageToGUI = MsgConfigurePlutoSDR::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool PlutoSDROutput::handleMessage(const Message& message)
{
    if (MsgConfigurePlutoSDR::match(message))
    {
        MsgConfigurePlutoSDR& conf = (MsgConfigurePlutoSDR&) message;
        qDebug() << "PlutoSDROutput::handleMessage: MsgConfigurePlutoSDR";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
            qDebug("PlutoSDROutput::handleMessage config error");
        }

        return true;
    }
    else if (DevicePlutoSDRShared::MsgCrossReportToBuddy::match(message)) // message from buddy
    {
        DevicePlutoSDRShared::MsgCrossReportToBuddy& conf = (DevicePlutoSDRShared::MsgCrossReportToBuddy&) message;
        PlutoSDROutputSettings newSettings = m_settings;
        newSettings.m_devSampleRate = conf.getDevSampleRate();
        newSettings.m_lpfFIRlog2Interp = conf.getLpfFiRlog2IntDec();
        newSettings.m_lpfFIRBW = conf.getLpfFirbw();
        newSettings.m_LOppmTenths = conf.getLoPPMTenths();
        newSettings.m_lpfFIREnable = conf.isLpfFirEnable();

        m_settings.applySettings(QList<QString>{"devSampleRate", "lpfFIRlog2Interp", "lpfFIRBW", "LOppmTenths", "lpfFIREnable"}, newSettings);

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "PlutoSDROutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine()) {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool PlutoSDROutput::openDevice()
{
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));

    // look for Rx buddy and get reference to common parameters
    if (m_deviceAPI->getSourceBuddies().size() > 0) // then sink
    {
        qDebug("PlutoSDROutput::openDevice: look at Rx buddy");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DevicePlutoSDRShared* buddySharedPtr = (DevicePlutoSDRShared*) sourceBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = buddySharedPtr->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("PlutoSDROutput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("PlutoSDROutput::openDevice: getting device parameters from Rx buddy");
        }
    }
    // There is no buddy then create the first PlutoSDR common parameters
    // open the device this will also populate common fields
    else
    {
        qDebug("PlutoSDROutput::openDevice: open device here");
        m_deviceShared.m_deviceParams = new DevicePlutoSDRParams();

        if (m_deviceAPI->getHardwareUserArguments().size() != 0)
        {
            QStringList kv = m_deviceAPI->getHardwareUserArguments().split('='); // expecting "uri=xxx"

            if (kv.size() > 1)
            {
                if (kv.at(0) == "uri")
                {
                    if (!m_deviceShared.m_deviceParams->openURI(kv.at(1).toStdString()))
                    {
                        qCritical("PlutoSDROutput::openDevice: open network device uri=%s failed", qPrintable(kv.at(1)));
                        return false;
                    }
                }
                else
                {
                    qCritical("PlutoSDROutput::openDevice: unexpected user parameter key %s", qPrintable(kv.at(0)));
                    return false;
                }
            }
            else
            {
                qCritical("PlutoSDROutput::openDevice: unexpected user arguments %s", qPrintable(m_deviceAPI->getHardwareUserArguments()));
                return false;
            }
        }
        else
        {
            char serial[256];
            strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

            if (!m_deviceShared.m_deviceParams->open(serial))
            {
                qCritical("PlutoSDROutput::openDevice: open serial %s failed", serial);
                return false;
            }
        }
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    if (!plutoBox->openTx())
    {
        qCritical("PlutoSDROutput::openDevice: cannot open Tx channel");
        return false;
    }

    m_plutoTxBuffer = plutoBox->createTxBuffer(PLUTOSDR_BLOCKSIZE_SAMPLES, false); // PlutoSDR buffer size is counted in number of (I,Q) samples

    return true;
}

void PlutoSDROutput::closeDevice()
{
    if (!m_open) { // was never open
        return;
    }

    if (m_deviceAPI->getSourceBuddies().size() == 0)
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

void PlutoSDROutput::suspendBuddies()
{
    // suspend Rx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
    {
        DeviceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
        DevicePlutoSDRShared *buddyShared = (DevicePlutoSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->stopWork();
        }
    }
}

void PlutoSDROutput::resumeBuddies()
{
    // resume Rx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
    {
        DeviceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
        DevicePlutoSDRShared *buddyShared = (DevicePlutoSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->startWork();
        }
    }
}

bool PlutoSDROutput::applySettings(const PlutoSDROutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    if (!m_open)
    {
        qCritical("PlutoSDROutput::applySettings: device not open");
        return false;
    }

    qDebug().noquote() << "PlutoSDROutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);

    bool forwardChangeOwnDSP    = false;
    bool forwardChangeOtherDSP  = false;
    bool ownThreadWasRunning    = false;
    bool suspendAllOtherThreads = false; // All others means Rx in fact
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    QLocale loc;

    // determine if buddies threads or own thread need to be suspended

    // changes affecting all buddies can occur if
    //   - device to host sample rate is changed
    //   - FIR filter is enabled or disabled
    //   - FIR filter is changed
    //   - LO correction is changed
    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("lpfFIREnable") ||
        settingsKeys.contains("lpfFIRlog2Interp") ||
        settingsKeys.contains("lpfFIRBW") ||
        settingsKeys.contains("lpfFIRGain") ||
        settingsKeys.contains("LOppmTenths") || force)
    {
        suspendAllOtherThreads = true;
    }

    if (suspendAllOtherThreads)
    {
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sourceBuddies.begin();

        for (; itSink != sourceBuddies.end(); ++itSink)
        {
            DevicePlutoSDRShared *buddySharedPtr = (DevicePlutoSDRShared *) (*itSink)->getBuddySharedPtr();

            if (buddySharedPtr->m_thread) {
                buddySharedPtr->m_thread->stopWork();
                buddySharedPtr->m_threadWasRunning = true;
            }
            else
            {
                buddySharedPtr->m_threadWasRunning = false;
            }
        }
    }

    if (m_plutoSDROutputThread && m_plutoSDROutputThread->isRunning())
    {
        m_plutoSDROutputThread->stopWork();
        ownThreadWasRunning = true;
    }

    // apply settings

    // Change affecting device sample rate chain and other buddies
    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("lpfFIREnable") ||
        settingsKeys.contains("lpfFIRlog2Interp") ||
        settingsKeys.contains("lpfFIRBW") ||
        settingsKeys.contains("lpfFIRGain") || force)
    {
        plutoBox->setFIR(settings.m_devSampleRate, settings.m_lpfFIRlog2Interp, DevicePlutoSDRBox::USE_TX, settings.m_lpfFIRBW, settings.m_lpfFIRGain);
        plutoBox->setFIREnable(settings.m_lpfFIREnable);   // eventually enable/disable FIR
        plutoBox->setSampleRate(settings.m_devSampleRate); // and set end point sample rate

        plutoBox->getTxSampleRates(m_deviceSampleRates); // pick up possible new rates
        qDebug() << "PlutoSDRInput::applySettings: BBPLL(Hz): " << m_deviceSampleRates.m_bbRateHz
                 << " DAC: " << m_deviceSampleRates.m_addaConnvRate
                 << " <-HB3- " << m_deviceSampleRates.m_hb3Rate
                 << " <-HB2- " << m_deviceSampleRates.m_hb2Rate
                 << " <-HB1- " << m_deviceSampleRates.m_hb1Rate
                 << " <-FIR- " << m_deviceSampleRates.m_firRate;

        forwardChangeOtherDSP = true;
        forwardChangeOwnDSP = (m_settings.m_devSampleRate != settings.m_devSampleRate) || force;
    }

	if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Interp") || force)
	{
        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2Interp),
            DevicePlutoSDRShared::m_sampleFifoMinRate);
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
	}

    if (settingsKeys.contains("log2Interp") || force)
    {
        if (m_plutoSDROutputThread != 0)
        {
            m_plutoSDROutputThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "PlutoSDROutput::applySettings: set soft interpolation in thread to " << (1<<settings.m_log2Interp);
        }

        forwardChangeOwnDSP = true;
    }

    if (settingsKeys.contains("LOppmTenths") || force)
    {
        plutoBox->setLOPPMTenths(settings.m_LOppmTenths);
        forwardChangeOtherDSP = true;
    }

    std::vector<std::string> params;
    bool paramsToSet = false;

    if (force || settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency"))
    {
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;


        params.push_back(QString(tr("out_altvoltage1_TX_LO_frequency=%1").arg(deviceCenterFrequency)).toStdString());
        paramsToSet = true;
        forwardChangeOwnDSP = true;

        qDebug() << "PlutoSDROutput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
                << " device center freq: " << deviceCenterFrequency << " Hz";

    }

    if (settingsKeys.contains("lpfBW") || force)
    {
        params.push_back(QString(tr("out_voltage_rf_bandwidth=%1").arg(settings.m_lpfBW)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("antennaPath") || force)
    {
        QString rfPortStr;
        PlutoSDROutputSettings::translateRFPath(settings.m_antennaPath, rfPortStr);
        params.push_back(QString(tr("out_voltage0_rf_port_select=%1").arg(rfPortStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("att") || force)
    {
        float attF = settings.m_att * 0.25f;
        params.push_back(QString(tr("out_voltage0_hardwaregain=%1").arg(attF)).toStdString());
        paramsToSet = true;
    }

    if (paramsToSet) {
        plutoBox->set_params(DevicePlutoSDRBox::DEVICE_PHY, params);
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (suspendAllOtherThreads)
    {
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DevicePlutoSDRShared *buddySharedPtr = (DevicePlutoSDRShared *) (*itSource)->getBuddySharedPtr();

            if (buddySharedPtr->m_threadWasRunning) {
                buddySharedPtr->m_thread->startWork();
            }
        }
    }

    if (ownThreadWasRunning) {
        m_plutoSDROutputThread->startWork();
    }

    // TODO: forward changes to other (Rx) DSP
    if (forwardChangeOtherDSP)
    {
        qDebug("PlutoSDROutput::applySettings: forwardChangeOtherDSP");

        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DevicePlutoSDRShared::MsgCrossReportToBuddy *msg = DevicePlutoSDRShared::MsgCrossReportToBuddy::create(
                    settings.m_devSampleRate,
                    settings.m_lpfFIREnable,
                    settings.m_lpfFIRlog2Interp,
                    settings.m_lpfFIRBW,
                    settings.m_LOppmTenths);

            if ((*itSource)->getSamplingDeviceGUIMessageQueue())
            {
                DevicePlutoSDRShared::MsgCrossReportToBuddy *msgToGUI = new DevicePlutoSDRShared::MsgCrossReportToBuddy(*msg);
                (*itSource)->getSamplingDeviceGUIMessageQueue()->push(msgToGUI);
            }

            (*itSource)->getSamplingDeviceInputMessageQueue()->push(msg);
        }
    }

    if (forwardChangeOwnDSP)
    {
        qDebug("PlutoSDROutput::applySettings: forward change to self");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    return true;
}

void PlutoSDROutput::getRSSI(std::string& rssiStr)
{
    if (!m_open)
    {
        qDebug("PlutoSDROutput::getRSSI: device not open");
        return;
    }

    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    if (!plutoBox->getTxRSSI(rssiStr, 0)) {
        rssiStr = "xxx dB";
    }
}

void PlutoSDROutput::getLORange(qint64& minLimit, qint64& maxLimit)
{
    if (!m_open)
    {
        qDebug("PlutoSDROutput::getLORange: device not open");
        return;
    }

    uint64_t min, max;
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    plutoBox->getTxLORange(min, max);
    minLimit = min;
    maxLimit = max;
}

void PlutoSDROutput::getbbLPRange(quint32& minLimit, quint32& maxLimit)
{
    if (!m_open)
    {
        qDebug("PlutoSDROutput::getbbLPRange: device not open");
        return;
    }

    uint32_t min, max;
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    plutoBox->getbbLPTxRange(min, max);
    minLimit = min;
    maxLimit = max;
}

bool PlutoSDROutput::fetchTemperature()
{
    if (!m_open)
    {
        qDebug("PlutoSDRInput::fetchTemperature: device not open");
        return false;
    }

    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    return plutoBox->fetchTemp();
}

float PlutoSDROutput::getTemperature()
{
    if (!m_open)
    {
        qDebug("PlutoSDRInput::getTemperature: device not open");
        return 0.0;
    }

    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    return plutoBox->getTemp();
}

int PlutoSDROutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int PlutoSDROutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}

int PlutoSDROutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPlutoSdrOutputSettings(new SWGSDRangel::SWGPlutoSdrOutputSettings());
    response.getPlutoSdrOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int PlutoSDROutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    PlutoSDROutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigurePlutoSDR *msg = MsgConfigurePlutoSDR::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePlutoSDR *msgToGUI = MsgConfigurePlutoSDR::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void PlutoSDROutput::webapiUpdateDeviceSettings(
        PlutoSDROutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getPlutoSdrOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getPlutoSdrOutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getPlutoSdrOutputSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("lpfFIREnable")) {
        settings.m_lpfFIREnable = response.getPlutoSdrOutputSettings()->getLpfFirEnable() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBW")) {
        settings.m_lpfFIRBW = response.getPlutoSdrOutputSettings()->getLpfFirbw();
    }
    if (deviceSettingsKeys.contains("lpfFIRlog2Interp")) {
        settings.m_lpfFIRlog2Interp = response.getPlutoSdrOutputSettings()->getLpfFiRlog2Interp();
    }
    if (deviceSettingsKeys.contains("lpfFIRGain")) {
        settings.m_lpfFIRGain = response.getPlutoSdrOutputSettings()->getLpfFirGain();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getPlutoSdrOutputSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getPlutoSdrOutputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("att")) {
        settings.m_att = response.getPlutoSdrOutputSettings()->getAtt();
    }
    if (deviceSettingsKeys.contains("antennaPath")) {
        int antennaPath = response.getPlutoSdrOutputSettings()->getAntennaPath();
        antennaPath = antennaPath < 0 ? 0 : antennaPath >= PlutoSDROutputSettings::RFPATH_END ? PlutoSDROutputSettings::RFPATH_END-1 : antennaPath;
        settings.m_antennaPath = (PlutoSDROutputSettings::RFPath) antennaPath;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getPlutoSdrOutputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getPlutoSdrOutputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPlutoSdrOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPlutoSdrOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPlutoSdrOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPlutoSdrOutputSettings()->getReverseApiDeviceIndex();
    }
}

int PlutoSDROutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setPlutoSdrOutputReport(new SWGSDRangel::SWGPlutoSdrOutputReport());
    response.getPlutoSdrOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void PlutoSDROutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const PlutoSDROutputSettings& settings)
{
    response.getPlutoSdrOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getPlutoSdrOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getPlutoSdrOutputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getPlutoSdrOutputSettings()->setLpfFirEnable(settings.m_lpfFIREnable ? 1 : 0);
    response.getPlutoSdrOutputSettings()->setLpfFirbw(settings.m_lpfFIRBW);
    response.getPlutoSdrOutputSettings()->setLpfFiRlog2Interp(settings.m_lpfFIRlog2Interp);
    response.getPlutoSdrOutputSettings()->setLpfFirGain(settings.m_lpfFIRGain);
    response.getPlutoSdrOutputSettings()->setLog2Interp(settings.m_log2Interp);
    response.getPlutoSdrOutputSettings()->setLpfBw(settings.m_lpfBW);
    response.getPlutoSdrOutputSettings()->setAtt(settings.m_att);
    response.getPlutoSdrOutputSettings()->setAntennaPath((int) settings.m_antennaPath);
    response.getPlutoSdrOutputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getPlutoSdrOutputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getPlutoSdrOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPlutoSdrOutputSettings()->getReverseApiAddress()) {
        *response.getPlutoSdrOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPlutoSdrOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPlutoSdrOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPlutoSdrOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void PlutoSDROutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getPlutoSdrOutputReport()->setDacRate(getDACSampleRate());
    std::string rssiStr;
    getRSSI(rssiStr);
    response.getPlutoSdrOutputReport()->setRssi(new QString(rssiStr.c_str()));
    fetchTemperature();
    response.getPlutoSdrOutputReport()->setTemperature(getTemperature());
}

void PlutoSDROutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const PlutoSDROutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("PlutoSDR"));
    swgDeviceSettings->setPlutoSdrOutputSettings(new SWGSDRangel::SWGPlutoSdrOutputSettings());
    SWGSDRangel::SWGPlutoSdrOutputSettings *swgPlutoSdrOutputSettings = swgDeviceSettings->getPlutoSdrOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgPlutoSdrOutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgPlutoSdrOutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgPlutoSdrOutputSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("lpfFIREnable") || force) {
        swgPlutoSdrOutputSettings->setLpfFirEnable(settings.m_lpfFIREnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfFIRBW") || force) {
        swgPlutoSdrOutputSettings->setLpfFirbw(settings.m_lpfFIRBW);
    }
    if (deviceSettingsKeys.contains("lpfFIRlog2Interp") || force) {
        swgPlutoSdrOutputSettings->setLpfFiRlog2Interp(settings.m_lpfFIRlog2Interp);
    }
    if (deviceSettingsKeys.contains("lpfFIRGain") || force) {
        swgPlutoSdrOutputSettings->setLpfFirGain(settings.m_lpfFIRGain);
    }
    if (deviceSettingsKeys.contains("log2Interp") || force) {
        swgPlutoSdrOutputSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgPlutoSdrOutputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("att") || force) {
        swgPlutoSdrOutputSettings->setAtt(settings.m_att);
    }
    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgPlutoSdrOutputSettings->setAntennaPath((int) settings.m_antennaPath);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgPlutoSdrOutputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgPlutoSdrOutputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void PlutoSDROutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("PlutoSDR"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

void PlutoSDROutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PlutoSDROutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PlutoSDROutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
