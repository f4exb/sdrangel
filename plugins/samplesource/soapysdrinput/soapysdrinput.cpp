///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "util/simpleserializer.h"

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "dsp/filerecord.h"
#include "dsp/dspengine.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdrinputthread.h"
#include "soapysdrinput.h"

MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgConfigureSoapySDRInput, Message)
MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgStartStop, Message)

SoapySDRInput::SoapySDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_deviceDescription("SoapySDRInput"),
    m_running(false),
    m_thread(0)
{
    openDevice();

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

SoapySDRInput::~SoapySDRInput()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
}

void SoapySDRInput::destroy()
{
    delete this;
}

bool SoapySDRInput::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("SoapySDRInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("SoapySDRInput::openDevice: allocated SampleFifo");
    }

    // look for Rx buddies and get reference to the device object
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("SoapySDRInput::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDRInput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
        {
            qCritical("SoapySDRInput::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
        m_deviceShared.m_deviceParams = deviceSoapySDRShared->m_deviceParams;
    }
    // look for Tx buddies and get reference to the device object
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("SoapySDRInput::openDevice: look in Tx buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDRInput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
        {
            qCritical("SoapySDRInput::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
        m_deviceShared.m_deviceParams = deviceSoapySDRShared->m_deviceParams;
    }
    // There are no buddies then create the first SoapySDR device
    else
    {
        qDebug("SoapySDRInput::openDevice: open device here");
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        m_deviceShared.m_device = deviceSoapySDR.openSoapySDR(m_deviceAPI->getSampleSourceSequence());

        if (!m_deviceShared.m_device)
        {
            qCritical("BladeRF2Input::openDevice: cannot open BladeRF2 device");
            return false;
        }

        m_deviceShared.m_deviceParams = new DeviceSoapySDRParams(m_deviceShared.m_device);
    }

    m_deviceShared.m_channel = m_deviceAPI->getItemIndex(); // publicly allocate channel
    m_deviceShared.m_source = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void SoapySDRInput::closeDevice()
{
    if (m_deviceShared.m_device == 0) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

    if (m_thread) { // stills own the thread => transfer to a buddy
        moveThreadToBuddy();
    }

    m_deviceShared.m_channel = -1; // publicly release channel
    m_deviceShared.m_source = 0;

    // No buddies so effectively close the device and delete parameters

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        deviceSoapySDR.closeSoapySdr(m_deviceShared.m_device);
        m_deviceShared.m_device = 0;
    }
}

void SoapySDRInput::getFrequencyRange(uint64_t& min, uint64_t& max)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);

    if (channelSettings && (channelSettings->m_frequencySettings.size() > 0))
    {
        DeviceSoapySDRParams::FrequencySetting freqSettings = channelSettings->m_frequencySettings[0];
        SoapySDR::RangeList rangeList = freqSettings.m_ranges;

        if (rangeList.size() > 0) // TODO: handle multiple ranges
        {
            SoapySDR::Range range = rangeList[0];
            min = range.minimum();
            max = range.maximum();
        }
        else
        {
            min = 0;
            max = 0;
        }
    }
    else
    {
        min = 0;
        max = 0;
    }
}

const SoapySDR::RangeList& SoapySDRInput::getRateRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_ratesRanges;
}

void SoapySDRInput::init()
{
}

SoapySDRInputThread *SoapySDRInput::findThread()
{
    if (m_thread == 0) // this does not own the thread
    {
        SoapySDRInputThread *soapySDRInputThread = 0;

        // find a buddy that has allocated the thread
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it)
        {
            SoapySDRInput *buddySource = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source;

            if (buddySource)
            {
                soapySDRInputThread = buddySource->getThread();

                if (soapySDRInputThread) {
                    break;
                }
            }
        }

        return soapySDRInputThread;
    }
    else
    {
        return m_thread; // own thread
    }
}

void SoapySDRInput::moveThreadToBuddy()
{
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

    for (; it != sourceBuddies.end(); ++it)
    {
        SoapySDRInput *buddySource = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source;

        if (buddySource)
        {
            buddySource->setThread(m_thread);
            m_thread = 0;  // zero for others
        }
    }
}

bool SoapySDRInput::start()
{
    return false;
}

void SoapySDRInput::stop()
{
}

QByteArray SoapySDRInput::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SoapySDRInput::deserialize(const QByteArray& data __attribute__((unused)))
{
    return false;
}

const QString& SoapySDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SoapySDRInput::getSampleRate() const
{
    return 0;
}

quint64 SoapySDRInput::getCenterFrequency() const
{
    return 0;
}

void SoapySDRInput::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

bool SoapySDRInput::setDeviceCenterFrequency(SoapySDR::Device *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths)
{
    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz += df;

    try
    {
        dev->setFrequency(SOAPY_SDR_RX,
                requestedChannel,
                m_deviceShared.m_deviceParams->getRxChannelMainTunableElementName(requestedChannel),
                freq_hz);
        qDebug("SoapySDRInput::setDeviceCenterFrequency: setFrequency(%llu)", freq_hz);
        return true;
    }
    catch (const std::exception &ex)
    {
        qCritical("SoapySDRInput::applySettings: could not set frequency: %llu: %s", freq_hz, ex.what());
        return false;
    }
}

bool SoapySDRInput::handleMessage(const Message& message __attribute__((unused)))
{
    if (MsgConfigureSoapySDRInput::match(message))
    {
        MsgConfigureSoapySDRInput& conf = (MsgConfigureSoapySDRInput&) message;
        qDebug() << "SoapySDRInput::handleMessage: MsgConfigureSoapySDRInput";

        if (!applySettings(conf.getSettings(), conf.getForce())) {
            qDebug("SoapySDRInput::handleMessage: MsgConfigureSoapySDRInput config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "SoapySDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SoapySDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
        }

        return true;
    }
    else if (DeviceSoapySDRShared::MsgReportBuddyChange::match(message))
    {
        int requestedChannel = m_deviceAPI->getItemIndex();
        DeviceSoapySDRShared::MsgReportBuddyChange& report = (DeviceSoapySDRShared::MsgReportBuddyChange&) message;
        SoapySDRInputSettings settings = m_settings;
        settings.m_fcPos = (SoapySDRInputSettings::fcPos_t) report.getFcPos();

        settings.m_centerFrequency = m_deviceShared.m_device->getFrequency(
                SOAPY_SDR_RX,
                requestedChannel,
                m_deviceShared.m_deviceParams->getRxChannelMainTunableElementName(requestedChannel));

        settings.m_devSampleRate = m_deviceShared.m_device->getSampleRate(SOAPY_SDR_RX, requestedChannel);

        SoapySDRInputThread *inputThread = findThread();

        if (inputThread)
        {
            inputThread->setFcPos(requestedChannel, (int) settings.m_fcPos);
        }

        m_settings = settings;

        return true;
    }
    else
    {
        return false;
    }
}

bool SoapySDRInput::applySettings(const SoapySDRInputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeToBuddies  = false;

    SoapySDR::Device *dev = m_deviceShared.m_device;
    SoapySDRInputThread *inputThread = findThread();
    int requestedChannel = m_deviceAPI->getItemIndex();
    qint64 xlatedDeviceCenterFrequency = settings.m_centerFrequency;
    xlatedDeviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    xlatedDeviceCenterFrequency = xlatedDeviceCenterFrequency < 0 ? 0 : xlatedDeviceCenterFrequency;

    if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
        (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        forwardChangeOwnDSP = true;
        forwardChangeToBuddies = true;

        if (dev != 0)
        {
            try
            {
                dev->setSampleRate(SOAPY_SDR_RX, requestedChannel, settings.m_devSampleRate);
                qDebug() << "SoapySDRInput::applySettings: setSampleRate OK: " << settings.m_devSampleRate;

                if (inputThread)
                {
                    bool wasRunning = inputThread->isRunning();
                    inputThread->stopWork();
                    inputThread->setSampleRate(settings.m_devSampleRate);

                    if (wasRunning) {
                        inputThread->startWork();
                    }
                }
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: could not set sample rate: %d: %s",
                        settings.m_devSampleRate, ex.what());
            }
        }
    }

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        SoapySDRInputThread *inputThread = findThread();

        if (inputThread != 0)
        {
            inputThread->setFcPos(requestedChannel, (int) settings.m_fcPos);
            qDebug() << "SoapySDRInput::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        forwardChangeOwnDSP = true;
        SoapySDRInputThread *inputThread = findThread();

        if (inputThread != 0)
        {
            inputThread->setLog2Decimation(requestedChannel, settings.m_log2Decim);
            qDebug() << "SoapySDRInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_transverterMode != settings.m_transverterMode)
        || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency)
        || (m_settings.m_LOppmTenths != settings.m_LOppmTenths)
        || (m_settings.m_devSampleRate != settings.m_devSampleRate)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                xlatedDeviceCenterFrequency,
                0,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate);

        forwardChangeOwnDSP = true;
        forwardChangeToBuddies = true;

        if (dev != 0) {
            setDeviceCenterFrequency(dev, requestedChannel, deviceCenterFrequency, settings.m_LOppmTenths);
        }
    }

    if (forwardChangeOwnDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardChangeToBuddies)
    {
        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();

        for (const auto &itSource : sourceBuddies)
        {
            DeviceSoapySDRShared::MsgReportBuddyChange *report = DeviceSoapySDRShared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    (int) settings.m_fcPos,
                    settings.m_devSampleRate,
                    true);
            itSource->getSampleSourceInputMessageQueue()->push(report);
        }

        for (const auto &itSink : sinkBuddies)
        {
            DeviceSoapySDRShared::MsgReportBuddyChange *report = DeviceSoapySDRShared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    (int) settings.m_fcPos,
                    settings.m_devSampleRate,
                    true);
            itSink->getSampleSinkInputMessageQueue()->push(report);
        }
    }

    m_settings = settings;

    qDebug() << "SoapySDRInput::applySettings: "
            << " m_transverterMode: " << m_settings.m_transverterMode
            << " m_transverterDeltaFrequency: " << m_settings.m_transverterDeltaFrequency
            << " m_centerFrequency: " << m_settings.m_centerFrequency << " Hz"
            << " m_LOppmTenths: " << m_settings.m_LOppmTenths
            << " m_log2Decim: " << m_settings.m_log2Decim
            << " m_fcPos: " << m_settings.m_fcPos
            << " m_devSampleRate: " << m_settings.m_devSampleRate
            << " m_dcBlock: " << m_settings.m_dcBlock
            << " m_iqCorrection: " << m_settings.m_iqCorrection;

    return true;
}
