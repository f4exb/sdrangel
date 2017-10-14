///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include <dsp/filerecord.h>
#include "sdrplaygui.h"
#include "sdrplayinput.h"

#include <device/devicesourceapi.h>

#include "sdrplaythread.h"

MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgConfigureSDRPlay, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgReportSDRPlayGains, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgFileRecord, Message)

SDRPlayInput::SDRPlayInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
	m_dev(0),
    m_sdrPlayThread(0),
    m_deviceDescription("SDRPlay"),
    m_devNumber(0),
    m_running(false)
{
    openDevice();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}

SDRPlayInput::~SDRPlayInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void SDRPlayInput::destroy()
{
    delete this;
}

bool SDRPlayInput::openDevice()
{
    m_devNumber = m_deviceAPI->getSampleSourceSequence();

    if (m_dev != 0)
    {
        closeDevice();
    }

    int res;
    //int numberOfGains;

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("SDRPlayInput::start: could not allocate SampleFifo");
        return false;
    }

    if ((res = mirisdr_open(&m_dev, MIRISDR_HW_SDRPLAY, m_devNumber)) < 0)
    {
        qCritical("SDRPlayInput::start: could not open SDRPlay #%d: %s", m_devNumber, strerror(errno));
        return false;
    }

    char vendor[256];
    char product[256];
    char serial[256];

    vendor[0] = '\0';
    product[0] = '\0';
    serial[0] = '\0';

    if ((res = mirisdr_get_device_usb_strings(m_devNumber, vendor, product, serial)) < 0)
    {
        qCritical("SDRPlayInput::start: error accessing USB device");
        stop();
        return false;
    }

    qWarning("SDRPlayInput::start: open: %s %s, SN: %s", vendor, product, serial);
    m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

    return true;
}

bool SDRPlayInput::start()
{
//    QMutexLocker mutexLocker(&m_mutex);
    int res;

    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

	char s12FormatString[] = "336_S16";

	if ((res = mirisdr_set_sample_format(m_dev, s12FormatString))) // sample format S12
	{
		qCritical("SDRPlayInput::start: could not set sample format: rc: %d", res);
		stop();
		return false;
	}

	int sampleRate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);

	if ((res = mirisdr_set_sample_rate(m_dev, sampleRate)))
    {
        qCritical("SDRPlayInput::start: could not set sample rate to %d: rc: %d", sampleRate, res);
        stop();
        return false;
    }

    char bulkFormatString[] = "BULK";

	if ((res = mirisdr_set_transfer(m_dev, bulkFormatString)) < 0)
	{
		qCritical("SDRPlayInput::start: could not set USB Bulk mode: rc: %d", res);
		stop();
		return false;
	}

	if ((res = mirisdr_reset_buffer(m_dev)) < 0)
	{
		qCritical("SDRPlayInput::start: could not reset USB EP buffers: %s", strerror(errno));
		stop();
		return false;
	}

    if((m_sdrPlayThread = new SDRPlayThread(m_dev, &m_sampleFifo)) == 0)
    {
        qFatal("SDRPlayInput::start: failed to create thread");
        return false;
    }

    m_sdrPlayThread->setLog2Decimation(m_settings.m_log2Decim);
    m_sdrPlayThread->setFcPos((int) m_settings.m_fcPos);

    m_sdrPlayThread->startWork();

//	mutexLocker.unlock();

	applySettings(m_settings, true, true);
	m_running = true;

	return true;
}

void SDRPlayInput::closeDevice()
{
    if (m_dev != 0)
    {
        mirisdr_close(m_dev);
        m_dev = 0;
    }

    m_deviceDescription.clear();
}

void SDRPlayInput::stop()
{
//    QMutexLocker mutexLocker(&m_mutex);

    if(m_sdrPlayThread != 0)
    {
        m_sdrPlayThread->stopWork();
        delete m_sdrPlayThread;
        m_sdrPlayThread = 0;
    }

    m_running = false;
}

const QString& SDRPlayInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SDRPlayInput::getSampleRate() const
{
    int rate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);
    return rate / (1<<m_settings.m_log2Decim);
}

quint64 SDRPlayInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

bool SDRPlayInput::handleMessage(const Message& message)
{
    if (MsgConfigureSDRPlay::match(message))
    {
        MsgConfigureSDRPlay& conf = (MsgConfigureSDRPlay&) message;
        qDebug() << "SDRPlayInput::handleMessage: MsgConfigureSDRPlay";
        const SDRPlaySettings& settings = conf.getSettings();

        // change of sample rate needs full stop / start sequence that includes the standard apply settings
        // only if in started state (iff m_dev != 0)
        if ((m_dev != 0) && (m_settings.m_devSampleRateIndex != settings.m_devSampleRateIndex))
        {
            m_settings = settings;
            stop();
            start();
        }
        // standard changes
        else
        {
            if (!applySettings(settings, false, conf.getForce()))
            {
                qDebug("SDRPlayInput::handleMessage: config error");
            }
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "SDRPlayInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

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

bool SDRPlayInput::applySettings(const SDRPlaySettings& settings, bool forwardChange, bool force)
{
    bool forceGainSetting = false;
    //settings.debug("SDRPlayInput::applySettings");
    QMutexLocker mutexLocker(&m_mutex);

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
    {
        m_settings.m_dcBlock = settings.m_dcBlock;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_settings.m_iqCorrection = settings.m_iqCorrection;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    // gains processing

    if ((m_settings.m_tunerGainMode != settings.m_tunerGainMode) || force)
    {
        m_settings.m_tunerGainMode = settings.m_tunerGainMode;
        forceGainSetting = true;
    }

    if (m_settings.m_tunerGainMode) // auto
    {
        if ((m_settings.m_tunerGain != settings.m_tunerGain) || forceGainSetting)
        {
            m_settings.m_tunerGain = settings.m_tunerGain;

            if(m_dev != 0)
            {
                int r = mirisdr_set_tuner_gain(m_dev, m_settings.m_tunerGain);

                if (r < 0)
                {
                    qDebug("SDRPlayInput::applySettings: could not set tuner gain");
                }
                else
                {
                    int lnaGain;

                    if (m_settings.m_frequencyBandIndex < 3) // bands using AM mode
                    {
                        lnaGain = mirisdr_get_mixbuffer_gain(m_dev);
                    }
                    else
                    {
                        lnaGain = mirisdr_get_lna_gain(m_dev);
                    }

                    MsgReportSDRPlayGains *message = MsgReportSDRPlayGains::create(
                            lnaGain,
                            mirisdr_get_mixer_gain(m_dev),
                            mirisdr_get_baseband_gain(m_dev),
                            mirisdr_get_tuner_gain(m_dev)
                    );

                    if (getMessageQueueToGUI()) {
                        getMessageQueueToGUI()->push(message);
                    }
                }
            }
        }
    }
    else // manual
    {
        bool anyChange = false;

        if ((m_settings.m_lnaOn != settings.m_lnaOn) || forceGainSetting)
        {
            if(m_dev != 0)
            {
                if (m_settings.m_frequencyBandIndex < 3) // bands using AM mode
                {
                    int r = mirisdr_set_mixbuffer_gain(m_dev, settings.m_lnaOn ? 0 : 1); // mirisdr_set_mixbuffer_gain takes gain reduction

                    if (r != 0)
                    {
                        qDebug("SDRPlayInput::applySettings: could not set mixer buffer gain");
                    }
                    else
                    {
                        m_settings.m_lnaOn = settings.m_lnaOn;
                        anyChange = true;
                    }
                }
                else
                {
                    int r = mirisdr_set_lna_gain(m_dev, settings.m_lnaOn ? 0 : 1); // mirisdr_set_lna_gain takes gain reduction

                    if (r != 0)
                    {
                        qDebug("SDRPlayInput::applySettings: could not set LNA gain");
                    }
                    else
                    {
                        m_settings.m_lnaOn = settings.m_lnaOn;
                        anyChange = true;
                    }
                }
            }
        }

        if ((m_settings.m_mixerAmpOn != settings.m_mixerAmpOn) || forceGainSetting)
        {
            if(m_dev != 0)
            {
                int r = mirisdr_set_mixer_gain(m_dev, settings.m_mixerAmpOn ? 0 : 1); // mirisdr_set_lna_gain takes gain reduction

                if (r != 0)
                {
                    qDebug("SDRPlayInput::applySettings: could not set mixer gain");
                }
                else
                {
                    m_settings.m_mixerAmpOn = settings.m_mixerAmpOn;
                    anyChange = true;
                }
            }
        }

        if ((m_settings.m_basebandGain != settings.m_basebandGain) || forceGainSetting)
        {
            if(m_dev != 0)
            {
                int r = mirisdr_set_baseband_gain(m_dev, settings.m_basebandGain);

                if (r != 0)
                {
                    qDebug("SDRPlayInput::applySettings: could not set mixer gain");
                }
                else
                {
                    m_settings.m_basebandGain = settings.m_basebandGain;
                    anyChange = true;
                }
            }
        }

        if (anyChange)
        {
            int lnaGain;

            if (m_settings.m_frequencyBandIndex < 3) // bands using AM mode
            {
                lnaGain = mirisdr_get_mixbuffer_gain(m_dev);
            }
            else
            {
                lnaGain = mirisdr_get_lna_gain(m_dev);
            }

            MsgReportSDRPlayGains *message = MsgReportSDRPlayGains::create(
                    lnaGain,
                    mirisdr_get_mixer_gain(m_dev),
                    mirisdr_get_baseband_gain(m_dev),
                    mirisdr_get_tuner_gain(m_dev)
            );

            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(message);
            }
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        m_settings.m_log2Decim = settings.m_log2Decim;
        forwardChange = true;

        if (m_sdrPlayThread != 0)
        {
            m_sdrPlayThread->setLog2Decimation(m_settings.m_log2Decim);
            qDebug() << "SDRPlayInput::applySettings: set decimation to " << (1<<m_settings.m_log2Decim);
        }
    }

    if (m_settings.m_centerFrequency != settings.m_centerFrequency)
    {
        forwardChange = true;
    }

    qint64 deviceCenterFrequency = m_settings.m_centerFrequency;
    qint64 f_img = deviceCenterFrequency;
    quint32 devSampleRate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
            || (m_settings.m_LOppmTenths != settings.m_LOppmTenths)
            || (m_settings.m_fcPos != settings.m_fcPos))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        m_settings.m_LOppmTenths = settings.m_LOppmTenths;

        if ((m_settings.m_log2Decim == 0) || (settings.m_fcPos == SDRPlaySettings::FC_POS_CENTER))
        {
            deviceCenterFrequency = m_settings.m_centerFrequency;
            f_img = deviceCenterFrequency;
        }
        else
        {
            if (settings.m_fcPos == SDRPlaySettings::FC_POS_INFRA)
            {
                deviceCenterFrequency = m_settings.m_centerFrequency + (devSampleRate / 4);
                f_img = deviceCenterFrequency + devSampleRate/2;
            }
            else if (settings.m_fcPos == SDRPlaySettings::FC_POS_SUPRA)
            {
                deviceCenterFrequency = m_settings.m_centerFrequency - (devSampleRate / 4);
                f_img = deviceCenterFrequency - devSampleRate/2;
            }
        }

        if(m_dev != 0)
        {
            if (setCenterFrequency(deviceCenterFrequency))
            {
                qDebug() << "SDRPlayInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
                        << " device center freq: " << deviceCenterFrequency << " Hz"
                        << " device sample rate: " << devSampleRate << "Hz"
                        << " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
                        << " img: " << f_img << "Hz";
            }
        }
    }

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        m_settings.m_fcPos = settings.m_fcPos;

        if (m_sdrPlayThread != 0)
        {
            m_sdrPlayThread->setFcPos((int) m_settings.m_fcPos);
            qDebug() << "SDRPlayInput: set fc pos (enum) to " << (int) m_settings.m_fcPos;
        }
    }

    if ((m_settings.m_frequencyBandIndex != settings.m_frequencyBandIndex) || force)
    {
        m_settings.m_frequencyBandIndex = settings.m_frequencyBandIndex;
        // change of frequency is done already
    }

    if ((m_settings.m_bandwidthIndex != settings.m_bandwidthIndex) || force)
    {
        int bandwidth = SDRPlayBandwidths::getBandwidth(settings.m_bandwidthIndex);
        int r = mirisdr_set_bandwidth(m_dev, bandwidth);

        if (r < 0)
        {
            qCritical("SDRPlayInput::applySettings: set bandwidth %d failed: rc: %d", bandwidth, r);
        }
        else
        {
            qDebug("SDRPlayInput::applySettings: bandwidth set to %d", bandwidth);
            m_settings.m_bandwidthIndex = settings.m_bandwidthIndex;
        }
    }

    if ((m_settings.m_ifFrequencyIndex != settings.m_ifFrequencyIndex) || force)
    {
        int iFFrequency = SDRPlayIF::getIF(settings.m_ifFrequencyIndex);
        int r = mirisdr_set_if_freq(m_dev, iFFrequency);

        if (r < 0)
        {
            qCritical("SDRPlayInput::applySettings: set IF frequency to %d failed: rc: %d", iFFrequency, r);
        }
        else
        {
            qDebug("SDRPlayInput::applySettings: IF frequency set to %d", iFFrequency);
            m_settings.m_ifFrequencyIndex = settings.m_ifFrequencyIndex;
        }
    }

    if (forwardChange)
    {
        int sampleRate = getSampleRate();
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    return true;
}

bool SDRPlayInput::setCenterFrequency(quint64 freq_hz)
{
    qint64 df = ((qint64)freq_hz * m_settings.m_LOppmTenths) / 10000000LL;
    freq_hz += df;

    int r = mirisdr_set_center_freq(m_dev, static_cast<uint32_t>(freq_hz));

    if (r != 0)
    {
        qWarning("SDRPlayInput::setCenterFrequency: could not frequency to %llu Hz", freq_hz);
        return false;
    }
    else
    {
        qWarning("SDRPlayInput::setCenterFrequency: frequency set to %llu Hz", freq_hz);
        return true;
    }
}


