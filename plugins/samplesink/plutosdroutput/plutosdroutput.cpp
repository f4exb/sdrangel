///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "plutosdr/deviceplutosdrparams.h"
#include "plutosdr/deviceplutosdrbox.h"

#include "plutosdroutput.h"
#include "plutosdroutputthread.h"

#define PLUTOSDR_BLOCKSIZE_SAMPLES (16*1024) //complex samples per buffer (must be multiple of 64)

MESSAGE_CLASS_DEFINITION(PlutoSDROutput::MsgConfigurePlutoSDR, Message)
MESSAGE_CLASS_DEFINITION(PlutoSDROutput::MsgStartStop, Message)

PlutoSDROutput::PlutoSDROutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_deviceDescription("PlutoSDROutput"),
    m_settings(),
    m_running(false),
    m_plutoTxBuffer(0),
    m_plutoSDROutputThread(0)
{
    suspendBuddies();
    openDevice();
    resumeBuddies();
}

PlutoSDROutput::~PlutoSDROutput()
{
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
    applySettings(m_settings, true);
}

bool PlutoSDROutput::start()
{
    if (!m_deviceShared.m_deviceParams->getBox()) {
        return false;
    }

    if (m_running) stop();

    applySettings(m_settings, true);

    // start / stop streaming is done in the thread.

    if ((m_plutoSDROutputThread = new PlutoSDROutputThread(PLUTOSDR_BLOCKSIZE_SAMPLES, m_deviceShared.m_deviceParams->getBox(), &m_sampleSourceFifo)) == 0)
    {
        qFatal("PlutoSDROutput::start: cannot create thread");
        stop();
        return false;
    }
    else
    {
        qDebug("PlutoSDROutput::start: thread created");
    }

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

    MsgConfigurePlutoSDR* message = MsgConfigurePlutoSDR::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDR* messageToGUI = MsgConfigurePlutoSDR::create(m_settings, true);
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

    MsgConfigurePlutoSDR* message = MsgConfigurePlutoSDR::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDR* messageToGUI = MsgConfigurePlutoSDR::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool PlutoSDROutput::handleMessage(const Message& message)
{
    if (MsgConfigurePlutoSDR::match(message))
    {
        MsgConfigurePlutoSDR& conf = (MsgConfigurePlutoSDR&) message;
        qDebug() << "PlutoSDROutput::handleMessage: MsgConfigurePlutoSDR";

        if (!applySettings(conf.getSettings(), conf.getForce()))
        {
            qDebug("PlutoSDROutput::handleMessage config error");
        }

        return true;
    }
    else if (DevicePlutoSDRShared::MsgCrossReportToBuddy::match(message)) // message from buddy
    {
        DevicePlutoSDRShared::MsgCrossReportToBuddy& conf = (DevicePlutoSDRShared::MsgCrossReportToBuddy&) message;
        m_settings.m_devSampleRate = conf.getDevSampleRate();
        m_settings.m_lpfFIRlog2Interp = conf.getLpfFiRlog2IntDec();
        m_settings.m_lpfFIRBW = conf.getLpfFirbw();
        m_settings.m_LOppmTenths = conf.getLoPPMTenths();
        PlutoSDROutputSettings newSettings = m_settings;
        newSettings.m_lpfFIREnable = conf.isLpfFirEnable();
        applySettings(newSettings);

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "PlutoSDROutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initGeneration())
            {
                m_deviceAPI->startGeneration();
                DSPEngine::instance()->startAudioInput();
            }
        }
        else
        {
            m_deviceAPI->stopGeneration();
            DSPEngine::instance()->stopAudioInput();
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
    m_sampleSourceFifo.resize(32*PLUTOSDR_BLOCKSIZE_SAMPLES);

    // look for Rx buddy and get reference to common parameters
    if (m_deviceAPI->getSourceBuddies().size() > 0) // then sink
    {
        qDebug("PlutoSDROutput::openDevice: look at Rx buddy");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
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
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSinkSerial()));
        m_deviceShared.m_deviceParams->open(serial);
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel
    suspendBuddies();
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    plutoBox->openTx();
    m_plutoTxBuffer = plutoBox->createTxBuffer(PLUTOSDR_BLOCKSIZE_SAMPLES, false); // PlutoSDR buffer size is counted in number of (I,Q) samples
    resumeBuddies();

    return true;
}

void PlutoSDROutput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getBox() == 0) { // was never open
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
        DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
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
        DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
        DevicePlutoSDRShared *buddyShared = (DevicePlutoSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->startWork();
        }
    }
}

bool PlutoSDROutput::applySettings(const PlutoSDROutputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP    = false;
    bool forwardChangeOtherDSP  = false;
    bool suspendOwnThread       = false;
    bool ownThreadWasRunning    = false;
    bool suspendAllOtherThreads = false; // All others means Rx in fact
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    // determine if buddies threads or own thread need to be suspended

    // changes affecting all buddies can occur if
    //   - device to host sample rate is changed
    //   - FIR filter is enabled or disabled
    //   - FIR filter is changed
    //   - LO correction is changed
    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) ||
        (m_settings.m_lpfFIRlog2Interp != settings.m_lpfFIRlog2Interp) ||
        (settings.m_lpfFIRBW != m_settings.m_lpfFIRBW) ||
        (settings.m_lpfFIRGain != m_settings.m_lpfFIRGain) ||
        (m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force)
    {
        suspendAllOtherThreads = true;
        suspendOwnThread = true;
    }
    else
    {
        suspendOwnThread = true;
    }

    if (suspendAllOtherThreads)
    {
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSink = sourceBuddies.begin();

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

    if (suspendOwnThread)
    {
        if (m_plutoSDROutputThread && m_plutoSDROutputThread->isRunning())
        {
            m_plutoSDROutputThread->stopWork();
            ownThreadWasRunning = true;
        }
    }

    // apply settings

    // Change affecting device sample rate chain and other buddies
    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) ||
        (m_settings.m_lpfFIRlog2Interp != settings.m_lpfFIRlog2Interp) ||
        (settings.m_lpfFIRBW != m_settings.m_lpfFIRBW) ||
        (settings.m_lpfFIRGain != m_settings.m_lpfFIRGain) || force)
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

    if ((m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        m_sampleSourceFifo.resize((32*PLUTOSDR_BLOCKSIZE_SAMPLES)/(1<<settings.m_log2Interp));

        if (m_plutoSDROutputThread != 0)
        {
            m_plutoSDROutputThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "PlutoSDROutput::applySettings: set soft interpolation in thread to " << (1<<settings.m_log2Interp);
        }

        forwardChangeOwnDSP = true;
    }

    if ((m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force)
    {
        plutoBox->setLOPPMTenths(settings.m_LOppmTenths);
        forwardChangeOtherDSP = true;
    }

    std::vector<std::string> params;
    bool paramsToSet = false;

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_transverterMode != settings.m_transverterMode)
        || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency))

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

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        params.push_back(QString(tr("out_voltage_rf_bandwidth=%1").arg(settings.m_lpfBW)).toStdString());
        paramsToSet = true;
    }

    if ((m_settings.m_antennaPath != settings.m_antennaPath) || force)
    {
        QString rfPortStr;
        PlutoSDROutputSettings::translateRFPath(settings.m_antennaPath, rfPortStr);
        params.push_back(QString(tr("out_voltage0_rf_port_select=%1").arg(rfPortStr)).toStdString());
        paramsToSet = true;
    }

    if ((m_settings.m_att != settings.m_att) || force)
    {
        float attF = settings.m_att * 0.25f;
        params.push_back(QString(tr("out_voltage0_hardwaregain=%1").arg(attF)).toStdString());
        paramsToSet = true;
    }

    if (paramsToSet)
    {
        plutoBox->set_params(DevicePlutoSDRBox::DEVICE_PHY, params);
    }

    m_settings = settings;

    if (suspendAllOtherThreads)
    {
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DevicePlutoSDRShared *buddySharedPtr = (DevicePlutoSDRShared *) (*itSource)->getBuddySharedPtr();

            if (buddySharedPtr->m_threadWasRunning) {
                buddySharedPtr->m_thread->startWork();
            }
        }
    }

    if (suspendOwnThread)
    {
        if (ownThreadWasRunning) {
            m_plutoSDROutputThread->startWork();
        }
    }

    // TODO: forward changes to other (Rx) DSP
    if (forwardChangeOtherDSP)
    {
        qDebug("PlutoSDROutput::applySettings: forwardChangeOtherDSP");

        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DevicePlutoSDRShared::MsgCrossReportToBuddy *msg = DevicePlutoSDRShared::MsgCrossReportToBuddy::create(
                    settings.m_devSampleRate,
                    settings.m_lpfFIREnable,
                    settings.m_lpfFIRlog2Interp,
                    settings.m_lpfFIRBW,
                    settings.m_LOppmTenths);

            if ((*itSource)->getSampleSourceGUIMessageQueue())
            {
                DevicePlutoSDRShared::MsgCrossReportToBuddy *msgToGUI = new DevicePlutoSDRShared::MsgCrossReportToBuddy(*msg);
                (*itSource)->getSampleSourceGUIMessageQueue()->push(msgToGUI);
            }

            (*itSource)->getSampleSourceInputMessageQueue()->push(msg);
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
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    if (!plutoBox->getTxRSSI(rssiStr, 0)) {
        rssiStr = "xxx dB";
    }
}

bool PlutoSDROutput::fetchTemperature()
{
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    return plutoBox->fetchTemp();
}

float PlutoSDROutput::getTemperature()
{
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    return plutoBox->getTemp();
}

int PlutoSDROutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int PlutoSDROutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
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

