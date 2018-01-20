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

#include "dsp/filerecord.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "plutosdr/deviceplutosdrparams.h"
#include "plutosdr/deviceplutosdrbox.h"

#include "plutosdrinput.h"
#include "plutosdrinputthread.h"

#define PLUTOSDR_BLOCKSIZE_SAMPLES (16*1024) //complex samples per buffer (must be multiple of 64)

MESSAGE_CLASS_DEFINITION(PlutoSDRInput::MsgConfigurePlutoSDR, Message)
MESSAGE_CLASS_DEFINITION(PlutoSDRInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(PlutoSDRInput::MsgStartStop, Message)

PlutoSDRInput::PlutoSDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_fileSink(0),
    m_deviceDescription("PlutoSDRInput"),
    m_running(false),
    m_plutoRxBuffer(0),
    m_plutoSDRInputThread(0)
{
    suspendBuddies();
    openDevice();
    resumeBuddies();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}

PlutoSDRInput::~PlutoSDRInput()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    suspendBuddies();
    closeDevice();
    resumeBuddies();
}

void PlutoSDRInput::destroy()
{
    delete this;
}

void PlutoSDRInput::init()
{
    applySettings(m_settings, true);
}

bool PlutoSDRInput::start()
{
    if (!m_deviceShared.m_deviceParams->getBox()) {
        return false;
    }

    if (m_running) stop();

    applySettings(m_settings, true);

    // start / stop streaming is done in the thread.

    if ((m_plutoSDRInputThread = new PlutoSDRInputThread(PLUTOSDR_BLOCKSIZE_SAMPLES, m_deviceShared.m_deviceParams->getBox(), &m_sampleFifo)) == 0)
    {
        qFatal("PlutoSDRInput::start: cannot create thread");
        stop();
        return false;
    }
    else
    {
        qDebug("PlutoSDRInput::start: thread created");
    }

    m_plutoSDRInputThread->setLog2Decimation(m_settings.m_log2Decim);
    m_plutoSDRInputThread->startWork();

    m_deviceShared.m_thread = m_plutoSDRInputThread;
    m_running = true;

    return true;
}

void PlutoSDRInput::stop()
{
    if (m_plutoSDRInputThread != 0)
    {
        m_plutoSDRInputThread->stopWork();
        delete m_plutoSDRInputThread;
        m_plutoSDRInputThread = 0;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;
}

QByteArray PlutoSDRInput::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDRInput::deserialize(const QByteArray& data)
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

const QString& PlutoSDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}
int PlutoSDRInput::getSampleRate() const
{
    return (m_settings.m_devSampleRate / (1<<m_settings.m_log2Decim));
}

quint64 PlutoSDRInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PlutoSDRInput::setCenterFrequency(qint64 centerFrequency)
{
    PlutoSDRInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigurePlutoSDR* message = MsgConfigurePlutoSDR::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDR* messageToGUI = MsgConfigurePlutoSDR::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool PlutoSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigurePlutoSDR::match(message))
    {
        MsgConfigurePlutoSDR& conf = (MsgConfigurePlutoSDR&) message;
        qDebug() << "PlutoSDRInput::handleMessage: MsgConfigurePlutoSDR";

        if (!applySettings(conf.getSettings(), conf.getForce()))
        {
            qDebug("PlutoSDRInput::handleMessage config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "PlutoSDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "PlutoSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
                DSPEngine::instance()->startAudioOutput();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
            DSPEngine::instance()->stopAudioOutput();
        }

        return true;
    }
    else if (DevicePlutoSDRShared::MsgCrossReportToBuddy::match(message)) // message from buddy
    {
        DevicePlutoSDRShared::MsgCrossReportToBuddy& conf = (DevicePlutoSDRShared::MsgCrossReportToBuddy&) message;
        m_settings.m_devSampleRate = conf.getDevSampleRate();
        m_settings.m_lpfFIRlog2Decim = conf.getLpfFiRlog2IntDec();
        m_settings.m_lpfFIRBW = conf.getLpfFirbw();
        m_settings.m_LOppmTenths = conf.getLoPPMTenths();
        PlutoSDRInputSettings newSettings = m_settings;
        newSettings.m_lpfFIREnable = conf.isLpfFirEnable();
        applySettings(newSettings);

        return true;
    }
    else
    {
        return false;
    }
}

bool PlutoSDRInput::openDevice()
{
    if (!m_sampleFifo.setSize(PLUTOSDR_BLOCKSIZE_SAMPLES))
    {
        qCritical("PlutoSDRInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("PlutoSDRInput::openDevice: allocated SampleFifo");
    }

    // look for Tx buddy and get reference to common parameters
    if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("PlutoSDRInput::openDevice: look at Tx buddy");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DevicePlutoSDRShared* buddySharedPtr = (DevicePlutoSDRShared*) sinkBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = buddySharedPtr->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("PlutoSDRInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("PlutoSDRInput::openDevice: getting device parameters from Tx buddy");
        }
    }
    // There is no buddy then create the first PlutoSDR common parameters
    // open the device this will also populate common fields
    else
    {
        qDebug("PlutoSDRInput::openDevice: open device here");

        m_deviceShared.m_deviceParams = new DevicePlutoSDRParams();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));
        m_deviceShared.m_deviceParams->open(serial);
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel
    suspendBuddies();
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    plutoBox->openRx();
    m_plutoRxBuffer = plutoBox->createRxBuffer(PLUTOSDR_BLOCKSIZE_SAMPLES, false);
    resumeBuddies();

    return true;
}

void PlutoSDRInput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getBox() == 0) { // was never open
        return;
    }

    if (m_deviceAPI->getSinkBuddies().size() == 0)
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

void PlutoSDRInput::suspendBuddies()
{
    // suspend Tx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DevicePlutoSDRShared *buddyShared = (DevicePlutoSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->stopWork();
        }
    }
}

void PlutoSDRInput::resumeBuddies()
{
    // resume Tx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DevicePlutoSDRShared *buddyShared = (DevicePlutoSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->startWork();
        }
    }
}

bool PlutoSDRInput::applySettings(const PlutoSDRInputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP    = false;
    bool forwardChangeOtherDSP  = false;
    bool suspendOwnThread       = false;
    bool ownThreadWasRunning    = false;
    bool suspendAllOtherThreads = false; // All others means Tx in fact
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    // determine if buddies threads or own thread need to be suspended

    // changes affecting all buddies can occur if
    //   - device to host sample rate is changed
    //   - FIR filter is enabled or disabled
    //   - FIR filter is changed
    //   - LO correction is changed
    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) ||
        (m_settings.m_lpfFIRlog2Decim != settings.m_lpfFIRlog2Decim) ||
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
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
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
        if (m_plutoSDRInputThread && m_plutoSDRInputThread->isRunning())
        {
            m_plutoSDRInputThread->stopWork();
            ownThreadWasRunning = true;
        }
    }

    // apply settings

    if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
        (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    // Change affecting device sample rate chain and other buddies
    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) ||
        (m_settings.m_lpfFIRlog2Decim != settings.m_lpfFIRlog2Decim) ||
        (settings.m_lpfFIRBW != m_settings.m_lpfFIRBW) ||
        (settings.m_lpfFIRGain != m_settings.m_lpfFIRGain) || force)
    {
        plutoBox->setFIR(settings.m_devSampleRate, settings.m_lpfFIRlog2Decim, DevicePlutoSDRBox::USE_RX, settings.m_lpfFIRBW, settings.m_lpfFIRGain);
        plutoBox->setFIREnable(settings.m_lpfFIREnable);   // eventually enable/disable FIR
        plutoBox->setSampleRate(settings.m_devSampleRate); // and set end point sample rate

        plutoBox->getRxSampleRates(m_deviceSampleRates); // pick up possible new rates
        qDebug() << "PlutoSDRInput::applySettings: BBPLL(Hz): " << m_deviceSampleRates.m_bbRateHz
                 << " ADC: " << m_deviceSampleRates.m_addaConnvRate
                 << " -HB3-> " << m_deviceSampleRates.m_hb3Rate
                 << " -HB2-> " << m_deviceSampleRates.m_hb2Rate
                 << " -HB1-> " << m_deviceSampleRates.m_hb1Rate
                 << " -FIR-> " << m_deviceSampleRates.m_firRate;

        forwardChangeOtherDSP = true;
        forwardChangeOwnDSP = (m_settings.m_devSampleRate != settings.m_devSampleRate) || force;
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        if (m_plutoSDRInputThread != 0)
        {
            m_plutoSDRInputThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "PlutoSDRInput::applySettings: set soft decimation to " << (1<<settings.m_log2Decim);
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

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_log2Decim != settings.m_log2Decim)
        || (m_settings.m_transverterMode != settings.m_transverterMode)
        || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency) || force)
    {
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        qint64 f_img = deviceCenterFrequency;
        quint32 devSampleRate = settings.m_devSampleRate;

        if ((settings.m_log2Decim == 0) || (settings.m_fcPos == PlutoSDRInputSettings::FC_POS_CENTER))
        {
            f_img = deviceCenterFrequency;
        }
        else
        {
            if (settings.m_fcPos == PlutoSDRInputSettings::FC_POS_INFRA)
            {
                deviceCenterFrequency += (devSampleRate / 4);
                f_img = deviceCenterFrequency + devSampleRate/2;
            }
            else if (settings.m_fcPos == PlutoSDRInputSettings::FC_POS_SUPRA)
            {
                deviceCenterFrequency -= (devSampleRate / 4);
                f_img = deviceCenterFrequency - devSampleRate/2;
            }
        }

        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

        if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
                || (m_settings.m_transverterMode != settings.m_transverterMode)
                || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency))
        {
            params.push_back(QString(tr("out_altvoltage0_RX_LO_frequency=%1").arg(deviceCenterFrequency)).toStdString());
            paramsToSet = true;
            forwardChangeOwnDSP = true;
        }

        if ((m_settings.m_fcPos != settings.m_fcPos) || force)
        {
            if (m_plutoSDRInputThread != 0)
            {
                m_plutoSDRInputThread->setFcPos(settings.m_fcPos);
                qDebug() << "PlutoSDRInput::applySettings: set fcPos to " << settings.m_fcPos;
            }
        }

        qDebug() << "PlutoSDRInput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
                << " device center freq: " << deviceCenterFrequency << " Hz"
                << " device sample rate: " << devSampleRate << "S/s"
                << " Actual sample rate: " << devSampleRate/(1<<settings.m_log2Decim) << "S/s"
                << " img: " << f_img << "Hz";
    }

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        params.push_back(QString(tr("in_voltage_rf_bandwidth=%1").arg(settings.m_lpfBW)).toStdString());
        paramsToSet = true;
    }

    if ((m_settings.m_antennaPath != settings.m_antennaPath) || force)
    {
        QString rfPortStr;
        PlutoSDRInputSettings::translateRFPath(settings.m_antennaPath, rfPortStr);
        params.push_back(QString(tr("in_voltage0_rf_port_select=%1").arg(rfPortStr)).toStdString());
        paramsToSet = true;
    }

    if ((m_settings.m_gainMode != settings.m_gainMode) || force)
    {
        QString gainModeStr;
        PlutoSDRInputSettings::translateGainMode(settings.m_gainMode, gainModeStr);
        params.push_back(QString(tr("in_voltage0_gain_control_mode=%1").arg(gainModeStr)).toStdString());
        paramsToSet = true;
    }

    if ((m_settings.m_gain != settings.m_gain) || force)
    {
        params.push_back(QString(tr("in_voltage0_hardwaregain=%1").arg(settings.m_gain)).toStdString());
        paramsToSet = true;
    }

    if (paramsToSet)
    {
        plutoBox->set_params(DevicePlutoSDRBox::DEVICE_PHY, params);
    }

    m_settings = settings;

    if (suspendAllOtherThreads)
    {
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DevicePlutoSDRShared *buddySharedPtr = (DevicePlutoSDRShared *) (*itSink)->getBuddySharedPtr();

            if (buddySharedPtr->m_threadWasRunning) {
                buddySharedPtr->m_thread->startWork();
            }
        }
    }

    if (suspendOwnThread)
    {
        if (ownThreadWasRunning) {
            m_plutoSDRInputThread->startWork();
        }
    }

    // TODO: forward changes to other (Tx) DSP
    if (forwardChangeOtherDSP)
    {

        qDebug("PlutoSDRInput::applySettings: forwardChangeOtherDSP");

        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DevicePlutoSDRShared::MsgCrossReportToBuddy *msg = DevicePlutoSDRShared::MsgCrossReportToBuddy::create(
                    settings.m_devSampleRate,
                    settings.m_lpfFIREnable,
                    settings.m_lpfFIRlog2Decim,
                    settings.m_lpfFIRBW,
                    settings.m_LOppmTenths);

            if ((*itSink)->getSampleSinkGUIMessageQueue())
            {
                DevicePlutoSDRShared::MsgCrossReportToBuddy *msgToGUI = new DevicePlutoSDRShared::MsgCrossReportToBuddy(*msg);
                (*itSink)->getSampleSinkGUIMessageQueue()->push(msgToGUI);
            }

            (*itSink)->getSampleSinkInputMessageQueue()->push(msg);
        }
    }

    if (forwardChangeOwnDSP)
    {
        qDebug("PlutoSDRInput::applySettings: forward change to self");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    return true;
}

void PlutoSDRInput::getRSSI(std::string& rssiStr)
{
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    if (!plutoBox->getRxRSSI(rssiStr, 0)) {
        rssiStr = "xxx dB";
    }
}

void PlutoSDRInput::getGain(int& gaindB)
{
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();

    if (!plutoBox->getRxGain(gaindB, 0)) {
        gaindB = 0;
    }
}

bool PlutoSDRInput::fetchTemperature()
{
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    return plutoBox->fetchTemp();
}

float PlutoSDRInput::getTemperature()
{
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    return plutoBox->getTemp();
}

int PlutoSDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int PlutoSDRInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
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

