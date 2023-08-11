///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Benjamin Menkuec, DJ4LF                                    //
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
#include "SWGPlutoSdrInputReport.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "jupitersdr/devicejupitersdrparams.h"
#include "jupitersdr/devicejupitersdrbox.h"

#include "jupitersdrinput.h"
#include "jupitersdrinputthread.h"

#define JUPITERSDR_BLOCKSIZE_SAMPLES (16*1024) //complex samples per buffer (must be multiple of 64)

MESSAGE_CLASS_DEFINITION(JupiterSDRInput::MsgConfigureJupiterSDR, Message)
MESSAGE_CLASS_DEFINITION(JupiterSDRInput::MsgStartStop, Message)

JupiterSDRInput::JupiterSDRInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_deviceDescription("JupiterSDRInput"),
    m_running(false),
    m_jupiterRxBuffer(0),
    m_jupiterSDRInputThread(nullptr)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_deviceSampleRates.m_addaConnvRate = 0;
    m_deviceSampleRates.m_bbRateHz = 0;
    m_deviceSampleRates.m_hb1Rate = 0;
    m_deviceSampleRates.m_hb2Rate = 0;
    m_deviceSampleRates.m_hb3Rate = 0;

    suspendBuddies();
    qCritical("open");
    m_open = openDevice();
    qCritical("open finished");

    if (!m_open) {
        qCritical("JupiterSDRInput::JupiterSDRInput: cannot open device");
    }

    qCritical("resume");
    resumeBuddies();

    qCritical("setNb");
    m_deviceAPI->setNbSourceStreams(1);
    qCritical("setNb finished");

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &JupiterSDRInput::networkManagerFinished
    );
}

JupiterSDRInput::~JupiterSDRInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &JupiterSDRInput::networkManagerFinished
    );
    delete m_networkManager;
    suspendBuddies();
    closeDevice();
    resumeBuddies();
}

void JupiterSDRInput::destroy()
{
    delete this;
}

void JupiterSDRInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
    qCritical("init end");
}

bool JupiterSDRInput::start()
{
    qCritical("start");
    if (!m_deviceShared.m_deviceParams->getBox())
    {
        qCritical("JupiterSDRInput::start: device not open");
        return false;
    }

    if (m_running) {
        stop();
    }

    // start / stop streaming is done in the thread.

    m_jupiterSDRInputThread = new JupiterSDRInputThread(JUPITERSDR_BLOCKSIZE_SAMPLES, m_deviceShared.m_deviceParams->getBox(), &m_sampleFifo);
    qDebug("JupiterSDRInput::start: thread created");

    qCritical("apply");
    applySettings(m_settings, QList<QString>(), true);

    qCritical("setLog2");
    m_jupiterSDRInputThread->setLog2Decimation(m_settings.m_log2Decim);
    qCritical("setIQOrder");
    m_jupiterSDRInputThread->setIQOrder(m_settings.m_iqOrder);
    qCritical("startWork");
    m_jupiterSDRInputThread->startWork();

    m_deviceShared.m_thread = m_jupiterSDRInputThread;
    m_running = true;

    return true;
}

void JupiterSDRInput::stop()
{
    if (m_jupiterSDRInputThread)
    {
        m_jupiterSDRInputThread->stopWork();
        delete m_jupiterSDRInputThread;
        m_jupiterSDRInputThread = nullptr;
    }

    m_deviceShared.m_thread = nullptr;
    m_running = false;
}

QByteArray JupiterSDRInput::serialize() const
{
    return m_settings.serialize();
}

bool JupiterSDRInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureJupiterSDR* message = MsgConfigureJupiterSDR::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureJupiterSDR* messageToGUI = MsgConfigureJupiterSDR::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& JupiterSDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}
int JupiterSDRInput::getSampleRate() const
{
    return (m_settings.m_devSampleRate / (1<<m_settings.m_log2Decim));
}

quint64 JupiterSDRInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void JupiterSDRInput::setCenterFrequency(qint64 centerFrequency)
{
    qCritical("setCenterFrequency"); 
    JupiterSDRInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureJupiterSDR* message = MsgConfigureJupiterSDR::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureJupiterSDR* messageToGUI = MsgConfigureJupiterSDR::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool JupiterSDRInput::handleMessage(const Message& message)
{
    qCritical("handleMessage"); 
    if (MsgConfigureJupiterSDR::match(message))
    {
        MsgConfigureJupiterSDR& conf = (MsgConfigureJupiterSDR&) message;
        qDebug() << "JupiterSDRInput::handleMessage: MsgConfigureJupiterSDR";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
            qDebug("JupiterSDRInput::handleMessage config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "JupiterSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
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
    else if (DeviceJupiterSDRShared::MsgCrossReportToBuddy::match(message)) // message from buddy
    {
        DeviceJupiterSDRShared::MsgCrossReportToBuddy& conf = (DeviceJupiterSDRShared::MsgCrossReportToBuddy&) message;
        JupiterSDRInputSettings newSettings = m_settings;
        newSettings.m_devSampleRate = conf.getDevSampleRate();
        newSettings.m_LOppmTenths = conf.getLoPPMTenths();

        m_settings.applySettings(QList<QString>{"devSampleRate", "LOppmTenths"}, newSettings);

        return true;
    }
    else
    {
        return false;
    }
}

bool JupiterSDRInput::openDevice()
{
    qCritical("openDevice"); 
    if (!m_sampleFifo.setSize(JUPITERSDR_BLOCKSIZE_SAMPLES))
    {
        qCritical("JupiterSDRInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("JupiterSDRInput::openDevice: allocated SampleFifo");
    }

    // look for Tx buddy and get reference to common parameters
    if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("JupiterSDRInput::openDevice: look at Tx buddy");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceJupiterSDRShared* buddySharedPtr = (DeviceJupiterSDRShared*) sinkBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = buddySharedPtr->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("JupiterSDRInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("JupiterSDRInput::openDevice: getting device parameters from Tx buddy");
        }
    }
    // There is no buddy then create the first JupiterSDR common parameters
    // open the device this will also populate common fields
    else
    {
        qDebug("JupiterSDRInput::openDevice: open device here");
        m_deviceShared.m_deviceParams = new DeviceJupiterSDRParams();

        if (m_deviceAPI->getHardwareUserArguments().size() != 0)
        {
            QStringList kv = m_deviceAPI->getHardwareUserArguments().split('='); // expecting "uri=xxx"

            if (kv.size() > 1)
            {
                if (kv.at(0) == "uri")
                {
                    if (!m_deviceShared.m_deviceParams->openURI(kv.at(1).toStdString()))
                    {
                        qCritical("JupiterSDRInput::openDevice: open network device uri=%s failed", qPrintable(kv.at(1)));
                        return false;
                    }
                }
                else
                {
                    qCritical("JupiterSDRInput::openDevice: unexpected user parameter key %s", qPrintable(kv.at(0)));
                    return false;
                }
            }
            else
            {
                qCritical("JupiterSDRInput::openDevice: unexpected user arguments %s", qPrintable(m_deviceAPI->getHardwareUserArguments()));
                return false;
            }
        }
        else
        {
            char serial[256];
            strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

            if (!m_deviceShared.m_deviceParams->open(serial))
            {
                qCritical("JupiterSDRInput::openDevice: open serial %s failed", serial);
                return false;
            }
        }
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel
    DeviceJupiterSDRBox *jupiterBox =  m_deviceShared.m_deviceParams->getBox();

    if (!jupiterBox->openRx())
    {
        qCritical("JupiterSDRInput::openDevice: cannot open Rx channel");
        return false;
    }

    m_jupiterRxBuffer = jupiterBox->createRxBuffer(JUPITERSDR_BLOCKSIZE_SAMPLES, false);

    qCritical("openDevice finished"); 
    return true;
}

void JupiterSDRInput::closeDevice()
{
    if (!m_open) { // was never open
        return;
    }

    if (m_deviceAPI->getSinkBuddies().size() == 0)
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

void JupiterSDRInput::suspendBuddies()
{
    // suspend Tx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DeviceJupiterSDRShared *buddyShared = (DeviceJupiterSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->stopWork();
        }
    }
}

void JupiterSDRInput::resumeBuddies()
{
    // resume Tx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DeviceJupiterSDRShared *buddyShared = (DeviceJupiterSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->startWork();
        }
    }
}

bool JupiterSDRInput::applySettings(const JupiterSDRInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qCritical("applySettings"); 
    if (!m_open)
    {
        qCritical("JupiterSDRInput::applySettings: device not open");
        return false;
    }

    bool forwardChangeOwnDSP    = false;
    bool forwardChangeOtherDSP  = false;
    bool ownThreadWasRunning    = false;
    bool suspendAllOtherThreads = false; // All others means Tx in fact
    DeviceJupiterSDRBox *jupiterBox =  m_deviceShared.m_deviceParams->getBox();
    QLocale loc;
    QList<QString> reverseAPIKeys;

    qDebug() << "JupiterSDRInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);

    // determine if buddies threads or own thread need to be suspended

    // changes affecting all buddies can occur if
    //   - device to host sample rate is changed
    //   - LO correction is changed
    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("LOppmTenths") || force)
    {
        suspendAllOtherThreads = true;
    }

    if (suspendAllOtherThreads)
    {
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceJupiterSDRShared *buddySharedPtr = (DeviceJupiterSDRShared *) (*itSink)->getBuddySharedPtr();

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

    if (m_jupiterSDRInputThread && m_jupiterSDRInputThread->isRunning())
    {
        m_jupiterSDRInputThread->stopWork();
        ownThreadWasRunning = true;
    }

    // apply settings

    if (settingsKeys.contains("dcBlock") ||
        settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    // Change affecting device sample rate chain and other buddies
    if (settingsKeys.contains("devSampleRate") || force)
    {
        jupiterBox->setSampleRate(settings.m_devSampleRate); // and set end point sample rate

        jupiterBox->getRxSampleRates(m_deviceSampleRates); // pick up possible new rates
        qDebug() << "JupiterSDRInput::applySettings: BBPLL(Hz): " << m_deviceSampleRates.m_bbRateHz
                 << " ADC: " << m_deviceSampleRates.m_addaConnvRate
                 << " -HB3-> " << m_deviceSampleRates.m_hb3Rate
                 << " -HB2-> " << m_deviceSampleRates.m_hb2Rate
                 << " -HB1-> " << m_deviceSampleRates.m_hb1Rate;

        forwardChangeOtherDSP = true;
        forwardChangeOwnDSP = (m_settings.m_devSampleRate != settings.m_devSampleRate) || force;
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        if (m_jupiterSDRInputThread)
        {
            m_jupiterSDRInputThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "JupiterSDRInput::applySettings: set soft decimation to " << (1<<settings.m_log2Decim);
        }

        forwardChangeOwnDSP = true;
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_jupiterSDRInputThread) {
            m_jupiterSDRInputThread->setIQOrder(settings.m_iqOrder);
        }
    }

    if (settingsKeys.contains("LOppmTenths") || force)
    {
        jupiterBox->setLOPPMTenths(settings.m_LOppmTenths);
        forwardChangeOtherDSP = true;
    }

    std::vector<std::string> params;
    bool paramsToSet = false;

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("fcPos")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("devSampleRate")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency") || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                settings.m_transverterDeltaFrequency,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                settings.m_transverterMode);

        params.push_back(QString(tr("out_altvoltage0_RX1_LO_frequency=%1").arg(deviceCenterFrequency)).toStdString());
        paramsToSet = true;
        forwardChangeOwnDSP = true;

        if (settingsKeys.contains("fcPos") || force)
        {
            if (m_jupiterSDRInputThread)
            {
                m_jupiterSDRInputThread->setFcPos(settings.m_fcPos);
                qDebug() << "JupiterSDRInput::applySettings: set fcPos to " << settings.m_fcPos;
            }
        }
    }

    if (settingsKeys.contains("lpfBW") || force)
    {
        params.push_back(QString(tr("in_voltage0_rf_bandwidth=%1").arg(settings.m_lpfBW)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("antennaPath") || force)
    {
        // QString rfPortStr;
        // JupiterSDRInputSettings::translateRFPath(settings.m_antennaPath, rfPortStr);
        // params.push_back(QString(tr("in_voltage0_rf_port_select=%1").arg(rfPortStr)).toStdString());
        // paramsToSet = true;
    }

    if (settingsKeys.contains("gainMode") || force)
    {
        QString gainModeStr;
        JupiterSDRInputSettings::translateGainMode(settings.m_gainMode, gainModeStr);
        params.push_back(QString(tr("in_voltage0_gain_control_mode=%1").arg(gainModeStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("gain") || force)
    {
        params.push_back(QString(tr("in_voltage0_hardwaregain=%1").arg(settings.m_gain)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("hwBBDCBlock") || force)
    {
        params.push_back(QString(tr("in_voltage0_bbdc_rejection_tracking_en=%1").arg(settings.m_hwBBDCBlock ? 1 : 0)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("hwRFDCBlock") || force)
    {
        params.push_back(QString(tr("in_voltage0_rfdc_tracking_en=%1").arg(settings.m_hwRFDCBlock ? 1 : 0)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("hwIQCorrection") || force)
    {
        params.push_back(QString(tr("in_voltage0_quadrature_fic_tracking_en=%1").arg(settings.m_hwIQCorrection ? 1 : 0)).toStdString());
        paramsToSet = true;
    }

    if (paramsToSet)
    {
        jupiterBox->set_params(DeviceJupiterSDRBox::DEVICE_PHY, params);
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
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceJupiterSDRShared *buddySharedPtr = (DeviceJupiterSDRShared *) (*itSink)->getBuddySharedPtr();

            if (buddySharedPtr->m_threadWasRunning) {
                buddySharedPtr->m_thread->startWork();
            }
        }
    }

    if (ownThreadWasRunning) {
        m_jupiterSDRInputThread->startWork();
    }

    // TODO: forward changes to other (Tx) DSP
    if (forwardChangeOtherDSP)
    {

        qDebug("JupiterSDRInput::applySettings: forwardChangeOtherDSP");

        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceJupiterSDRShared::MsgCrossReportToBuddy *msg = DeviceJupiterSDRShared::MsgCrossReportToBuddy::create(
                    settings.m_devSampleRate,
                    settings.m_LOppmTenths);

            if ((*itSink)->getSamplingDeviceGUIMessageQueue())
            {
                DeviceJupiterSDRShared::MsgCrossReportToBuddy *msgToGUI = new DeviceJupiterSDRShared::MsgCrossReportToBuddy(*msg);
                (*itSink)->getSamplingDeviceGUIMessageQueue()->push(msgToGUI);
            }

            (*itSink)->getSamplingDeviceInputMessageQueue()->push(msg);
        }
    }

    if (forwardChangeOwnDSP)
    {
        qDebug("JupiterSDRInput::applySettings: forward change to self");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    qCritical("applySettings finished"); 
    return true;
}

void JupiterSDRInput::getRSSI(std::string& rssiStr)
{
    if (!m_open)
    {
        qDebug("JupiterSDRInput::getRSSI: device not open");
        return;
    }

    DeviceJupiterSDRBox *jupiterBox =  m_deviceShared.m_deviceParams->getBox();

    if (!jupiterBox->getRxRSSI(rssiStr, 0)) {
        rssiStr = "xxx dB";
    }
}

void JupiterSDRInput::getLORange(qint64& minLimit, qint64& maxLimit)
{
    if (!m_open)
    {
        qDebug("JupiterSDRInput::getLORange: device not open");
        return;
    }

    uint64_t min, max;
    DeviceJupiterSDRBox *jupiterBox = m_deviceShared.m_deviceParams->getBox();

    jupiterBox->getRxLORange(min, max);
    minLimit = min;
    maxLimit = max;
}

void JupiterSDRInput::getbbLPRange(quint32& minLimit, quint32& maxLimit)
{
    if (!m_open)
    {
        qDebug("JupiterSDRInput::getbbLPRange: device not open");
        return;
    }

    uint32_t min, max;
    DeviceJupiterSDRBox *jupiterBox =  m_deviceShared.m_deviceParams->getBox();

    jupiterBox->getbbLPRxRange(min, max);
    minLimit = min;
    maxLimit = max;
}

void JupiterSDRInput::getGain(int& gaindB)
{
    if (!m_open)
    {
        qDebug("JupiterSDRInput::getGain: device not open");
        return;
    }

    DeviceJupiterSDRBox *jupiterBox =  m_deviceShared.m_deviceParams->getBox();

    if (!jupiterBox->getRxGain(gaindB, 0)) {
        gaindB = 0;
    }
}

bool JupiterSDRInput::fetchTemperature()
{
    if (!m_open)
    {
        qDebug("JupiterSDRInput::fetchTemperature: device not open");
        return false;
    }

    DeviceJupiterSDRBox *jupiterBox =  m_deviceShared.m_deviceParams->getBox();
    return jupiterBox->fetchTemp();
}

float JupiterSDRInput::getTemperature()
{
    if (!m_open)
    {
        qDebug("JupiterSDRInput::getTemperature: device not open");
        return 0.0;
    }

    DeviceJupiterSDRBox *jupiterBox =  m_deviceShared.m_deviceParams->getBox();
    return jupiterBox->getTemp();
}

int JupiterSDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int JupiterSDRInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

int JupiterSDRInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPlutoSdrInputSettings(new SWGSDRangel::SWGPlutoSdrInputSettings());
    response.getPlutoSdrInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int JupiterSDRInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    JupiterSDRInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureJupiterSDR *msg = MsgConfigureJupiterSDR::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureJupiterSDR *msgToGUI = MsgConfigureJupiterSDR::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void JupiterSDRInput::webapiUpdateDeviceSettings(
        JupiterSDRInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getPlutoSdrInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getPlutoSdrInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getPlutoSdrInputSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        int fcPos = response.getPlutoSdrInputSettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (JupiterSDRInputSettings::fcPos_t) fcPos;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getPlutoSdrInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getPlutoSdrInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("hwBBDCBlock")) {
        settings.m_hwBBDCBlock = response.getPlutoSdrInputSettings()->getHwBbdcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("hwRFDCBlock")) {
        settings.m_hwBBDCBlock = response.getPlutoSdrInputSettings()->getHwRfdcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("hwIQCorrection")) {
        settings.m_hwBBDCBlock = response.getPlutoSdrInputSettings()->getHwIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getPlutoSdrInputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getPlutoSdrInputSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getPlutoSdrInputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getPlutoSdrInputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("antennaPath")) {
        int antennaPath = response.getPlutoSdrInputSettings()->getAntennaPath();
        antennaPath = antennaPath < 0 ? 0 : antennaPath >= JupiterSDRInputSettings::RFPATH_END ? JupiterSDRInputSettings::RFPATH_END-1 : antennaPath;
        settings.m_antennaPath = (JupiterSDRInputSettings::RFPath) antennaPath;
    }
    if (deviceSettingsKeys.contains("gainMode")) {
        int gainMode = response.getPlutoSdrInputSettings()->getGainMode();
        gainMode = gainMode < 0 ? 0 : gainMode >= JupiterSDRInputSettings::GAIN_END ? JupiterSDRInputSettings::GAIN_END-1 : gainMode;
        settings.m_gainMode = (JupiterSDRInputSettings::GainMode) gainMode;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getPlutoSdrInputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getPlutoSdrInputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPlutoSdrInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPlutoSdrInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPlutoSdrInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPlutoSdrInputSettings()->getReverseApiDeviceIndex();
    }
}

int JupiterSDRInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setPlutoSdrInputReport(new SWGSDRangel::SWGPlutoSdrInputReport());
    response.getPlutoSdrInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void JupiterSDRInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const JupiterSDRInputSettings& settings)
{
    response.getPlutoSdrInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getPlutoSdrInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getPlutoSdrInputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getPlutoSdrInputSettings()->setFcPos((int) settings.m_fcPos);
    response.getPlutoSdrInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getPlutoSdrInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getPlutoSdrInputSettings()->setHwBbdcBlock(settings.m_hwBBDCBlock ? 1 : 0);
    response.getPlutoSdrInputSettings()->setHwRfdcBlock(settings.m_hwRFDCBlock ? 1 : 0);
    response.getPlutoSdrInputSettings()->setHwIqCorrection(settings.m_hwIQCorrection ? 1 : 0);
    response.getPlutoSdrInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getPlutoSdrInputSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getPlutoSdrInputSettings()->setLpfBw(settings.m_lpfBW);
    response.getPlutoSdrInputSettings()->setGain(settings.m_gain);
    response.getPlutoSdrInputSettings()->setAntennaPath((int) settings.m_antennaPath);
    response.getPlutoSdrInputSettings()->setGainMode((int) settings.m_gainMode);
    response.getPlutoSdrInputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getPlutoSdrInputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    response.getPlutoSdrInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPlutoSdrInputSettings()->getReverseApiAddress()) {
        *response.getPlutoSdrInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPlutoSdrInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPlutoSdrInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPlutoSdrInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void JupiterSDRInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getPlutoSdrInputReport()->setAdcRate(getADCSampleRate());
    std::string rssiStr;
    getRSSI(rssiStr);
    response.getPlutoSdrInputReport()->setRssi(new QString(rssiStr.c_str()));
    int gainDB;
    getGain(gainDB);
    response.getPlutoSdrInputReport()->setGainDb(gainDB);
    fetchTemperature();
    response.getPlutoSdrInputReport()->setTemperature(getTemperature());
}

void JupiterSDRInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const JupiterSDRInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("JupiterSDR"));
    swgDeviceSettings->setPlutoSdrInputSettings(new SWGSDRangel::SWGPlutoSdrInputSettings());
    SWGSDRangel::SWGPlutoSdrInputSettings *swgPlutoSdrInputSettings = swgDeviceSettings->getPlutoSdrInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgPlutoSdrInputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgPlutoSdrInputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgPlutoSdrInputSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgPlutoSdrInputSettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgPlutoSdrInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgPlutoSdrInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("hwBBDCBlock") || force) {
        swgPlutoSdrInputSettings->setHwBbdcBlock(settings.m_hwBBDCBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("hwRFDCBlock") || force) {
        swgPlutoSdrInputSettings->setHwRfdcBlock(settings.m_hwRFDCBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("hwIQCorrection") || force) {
        swgPlutoSdrInputSettings->setHwIqCorrection(settings.m_hwIQCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgPlutoSdrInputSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgPlutoSdrInputSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgPlutoSdrInputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgPlutoSdrInputSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgPlutoSdrInputSettings->setAntennaPath((int) settings.m_antennaPath);
    }
    if (deviceSettingsKeys.contains("gainMode") || force) {
        swgPlutoSdrInputSettings->setGainMode((int) settings.m_gainMode);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgPlutoSdrInputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgPlutoSdrInputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
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

void JupiterSDRInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("JupiterSDR"));

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

void JupiterSDRInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "JupiterSDRInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("JupiterSDRInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
