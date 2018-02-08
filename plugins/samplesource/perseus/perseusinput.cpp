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

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "dsp/filerecord.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesourceapi.h"
#include "perseus/deviceperseus.h"

#include "perseusinput.h"
#include "perseusthread.h"

MESSAGE_CLASS_DEFINITION(PerseusInput::MsgConfigurePerseus, Message)
MESSAGE_CLASS_DEFINITION(PerseusInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(PerseusInput::MsgStartStop, Message)

PerseusInput::PerseusInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_fileSink(0),
    m_deviceDescription("PerseusInput"),
    m_running(false),
    m_perseusThread(0),
    m_perseusDescriptor(0)
{
    openDevice();
    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}

PerseusInput::~PerseusInput()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void PerseusInput::destroy()
{
    delete this;
}

void PerseusInput::init()
{
    applySettings(m_settings, true);
}

bool PerseusInput::start()
{
    if (m_running) stop();

    applySettings(m_settings, true);

    // start / stop streaming is done in the thread.

    if ((m_perseusThread = new PerseusThread(m_perseusDescriptor, &m_sampleFifo)) == 0)
    {
        qFatal("PerseusInput::start: cannot create thread");
        stop();
        return false;
    }
    else
    {
        qDebug("PerseusInput::start: thread created");
    }

    m_perseusThread->setLog2Decimation(m_settings.m_log2Decim);
    m_perseusThread->startWork();

    m_running = true;

    return true;
}

void PerseusInput::stop()
{
    if (m_perseusThread != 0)
    {
        m_perseusThread->stopWork();
        delete m_perseusThread;
        m_perseusThread = 0;
    }

    m_running = false;
}

QByteArray PerseusInput::serialize() const
{
    return m_settings.serialize();
}

bool PerseusInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePerseus* message = MsgConfigurePerseus::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePerseus* messageToGUI = MsgConfigurePerseus::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& PerseusInput::getDeviceDescription() const
{
    return m_deviceDescription;
}
int PerseusInput::getSampleRate() const
{
    if (m_settings.m_devSampleRateIndex < m_sampleRates.size()) {
        return (m_sampleRates[m_settings.m_devSampleRateIndex] / (1<<m_settings.m_log2Decim));
    } else {
        return (m_sampleRates[0] / (1<<m_settings.m_log2Decim));
    }
}

quint64 PerseusInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PerseusInput::setCenterFrequency(qint64 centerFrequency)
{
    PerseusSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigurePerseus* message = MsgConfigurePerseus::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePerseus* messageToGUI = MsgConfigurePerseus::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool PerseusInput::handleMessage(const Message& message)
{
    if (MsgConfigurePerseus::match(message))
    {
        MsgConfigurePerseus& conf = (MsgConfigurePerseus&) message;
        qDebug() << "PerseusInput::handleMessage: MsgConfigurePerseus";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success) {
            qDebug("MsgConfigurePerseus::handleMessage: Perseus config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "PerseusInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "PerseusInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool PerseusInput::openDevice()
{
    if (m_perseusDescriptor != 0) {
        closeDevice();
    }

    if (!m_sampleFifo.setSize(PERSEUS_NBSAMPLES))
    {
        qCritical("PerseusInput::start: could not allocate SampleFifo");
        return false;
    }

    m_deviceAPI->getSampleSourceSerial();
    int deviceSequence = DevicePerseus::instance().getSequenceFromSerial(m_deviceAPI->getSampleSourceSerial().toStdString());

    if ((m_perseusDescriptor = perseus_open(deviceSequence)) == 0)
    {
        qCritical("PerseusInput::openDevice: cannot open device: %s", perseus_errorstr());
        return false;
    }

    int buf[32];
    m_sampleRates.clear();

    if (perseus_get_sampling_rates(m_perseusDescriptor, buf, sizeof(buf)/sizeof(buf[0])) < 0)
    {
        qCritical("PerseusInput::openDevice: cannot get sampling rates: %s", perseus_errorstr());
        perseus_close(m_perseusDescriptor);
        return false;
    }
    else
    {
         for (int i = 0; (i < 32) && (buf[i] != 0); i++)
         {
             qDebug("PerseusInput::openDevice: sample rate: %d", buf[i]);
             m_sampleRates.push_back(buf[i]);
         }
    }

    return true;
}

void PerseusInput::closeDevice()
{
    if (m_perseusDescriptor)
    {
        perseus_stop_async_input(m_perseusDescriptor);
        perseus_close(m_perseusDescriptor);
    }
}

void PerseusInput::setDeviceCenterFrequency(quint64 freq_hz, const PerseusSettings& settings)
{
    qint64 df = ((qint64)freq_hz * settings.m_LOppmTenths) / 10000000LL;
    freq_hz += df;

    // wideband flag is inverted since parameter is set to enable preselection filters
    int rc = perseus_set_ddc_center_freq(m_perseusDescriptor, freq_hz, settings.m_wideBand ? 0 : 1);

    if (rc < 0) {
        qWarning("PerseusInput::setDeviceCenterFrequency: could not set frequency to %llu Hz: %s", freq_hz, perseus_errorstr());
    } else {
        qDebug("PerseusInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
    }
}

bool PerseusInput::applySettings(const PerseusSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);

    bool forwardChange = false;
    int sampleRateIndex = settings.m_devSampleRateIndex;

    if ((m_settings.m_devSampleRateIndex != settings.m_devSampleRateIndex) || force)
    {
        forwardChange = true;

        if (settings.m_devSampleRateIndex >= m_sampleRates.size()) {
            sampleRateIndex = m_sampleRates.size() - 1;
        }

        if (m_perseusDescriptor != 0)
        {
            int rate = m_sampleRates[m_settings.m_devSampleRateIndex < m_sampleRates.size() ? m_settings.m_devSampleRateIndex: 0];
            int rc = perseus_set_sampling_rate(m_perseusDescriptor, rate);

            if (rc < 0) {
                qCritical("PerseusInput::applySettings: could not set sample rate index %u (%d S/s): %s",
                        settings.m_devSampleRateIndex, rate, perseus_errorstr());
            }
            else if (m_perseusThread != 0)
            {
                qDebug("PerseusInput::applySettings: sample rate set to index: %u (%d S/s)", settings.m_devSampleRateIndex, rate);
                m_perseusThread->setSamplerate(rate);
            }
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        forwardChange = true;

        if (m_perseusThread != 0)
        {
            m_perseusThread->setLog2Decimation(settings.m_log2Decim);
            qDebug("PerseusInput: set decimation to %d", (1<<settings.m_log2Decim));
        }
    }

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
            || (m_settings.m_LOppmTenths != settings.m_LOppmTenths)
            || (m_settings.m_wideBand != settings.m_wideBand)
            || (m_settings.m_transverterMode != settings.m_transverterMode)
            || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency))
    {
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

        if (m_perseusDescriptor != 0)
        {
            setDeviceCenterFrequency(deviceCenterFrequency, settings);
            qDebug("PerseusInput::applySettings: center freq: %llu Hz", settings.m_centerFrequency);
        }

        forwardChange = true;
    }

    if ((m_settings.m_attenuator != settings.m_attenuator) || force)
    {
        int rc = perseus_set_attenuator_n(m_perseusDescriptor, (int) settings.m_attenuator);

        if (rc < 0) {
            qWarning("PerseusInput::applySettings: cannot set attenuator to %d dB: %s", (int) settings.m_attenuator*10, perseus_errorstr());
        } else {
            qDebug("PerseusInput::applySettings: attenuator set to %d dB", (int) settings.m_attenuator*10);
        }
    }

    if ((m_settings.m_adcDither != settings.m_adcDither)
       || (m_settings.m_adcPreamp != settings.m_adcPreamp) || force)
    {
        int rc = perseus_set_adc(m_perseusDescriptor, settings.m_adcDither ? 1 : 0, settings.m_adcPreamp ? 1 : 0);

        if (rc < 0) {
            qWarning("PerseusInput::applySettings: cannot set ADC to dither %s and preamp %s: %s",
                    settings.m_adcDither ? "on" : "off", settings.m_adcPreamp ? "on" : "off", perseus_errorstr());
        } else {
            qDebug("PerseusInput::applySettings: ADC set to dither %s and preamp %s",
                    settings.m_adcDither ? "on" : "off", settings.m_adcPreamp ? "on" : "off");
        }
    }

    if (forwardChange)
    {
        int sampleRate = m_sampleRates[sampleRateIndex]/(1<<settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    m_settings = settings;
    m_settings.m_devSampleRateIndex = sampleRateIndex;
    return true;
}

int PerseusInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int PerseusInput::webapiRun(
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
