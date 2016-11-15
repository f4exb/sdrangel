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
#include "sdrplaygui.h"
#include "sdrplayinput.h"

#include <device/devicesourceapi.h>

#include "sdrplaythread.h"

MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgConfigureSDRPlay, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgReportSDRPlay, Message)

SDRPlayInput::SDRPlayInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
	m_dev(0),
    m_sdrPlayThread(0),
    m_deviceDescription("SDRPlay")
{
}

SDRPlayInput::~SDRPlayInput()
{
    stop();
}

bool SDRPlayInput::init(const Message& cmd)
{
    return false;
}

bool SDRPlayInput::start(uint32_t device)
{
    QMutexLocker mutexLocker(&m_mutex);

	if (m_dev != 0)
	{
		stop();
	}

	char vendor[256];
	char product[256];
	char serial[256];
	int res;
	int numberOfGains;

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("SDRPlayInput::start: could not allocate SampleFifo");
        return false;
    }

	if ((res = mirisdr_open(&m_dev, device)) < 0)
	{
		qCritical("SDRPlayInput::start: could not open SDRPlay #%d: %s", device, strerror(errno));
		return false;
	}

	vendor[0] = '\0';
	product[0] = '\0';
	serial[0] = '\0';

	if ((res = mirisdr_get_device_usb_strings(m_dev, vendor, product, serial)) < 0)
	{
		qCritical("SDRPlayInput::start: error accessing USB device");
		stop();
		return false;
	}

	qWarning("SDRPlayInput::start: open: %s %s, SN: %s", vendor, product, serial);
	m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

	if ((res = mirisdr_set_sample_rate(m_dev, 2048000)) < 0)
	{
		qCritical("SDRPlayInput::start: could not set sample rate: 2048kS/s");
		stop();
		return false;
	}

	if ((res = mirisdr_set_center_freq(m_dev, m_settings.m_centerFrequency)) < 0)
	{
		qCritical("SDRPlayInput::start: could not set frequency to: %lu Hz", m_settings.m_centerFrequency);
		stop();
		return false;
	}

	if ((res = mirisdr_set_sample_format(m_dev, "336_S16"))) // sample format S12
	{
		qCritical("SDRPlayInput::start: could not set sample format: rc: %d", res);
		stop();
		return false;
	}

	if ((res = mirisdr_set_transfer(m_dev, "BULK")) < 0)
	{
		qCritical("SDRPlayInput::start: could not set USB Bulk mode: rc: %d", res);
		stop();
		return false;
	}

	if ((res = mirisdr_set_if_freq(m_dev, SDRPlayIF::m_if[m_settings.m_ifFrequencyIndex])) < 0)
	{
		qCritical("SDRPlayInput::start: could not set IF frequency at index %d: rc: %d", m_settings.m_ifFrequencyIndex, res);
		stop();
		return false;
	}

	if ((res = mirisdr_set_bandwidth(m_dev, SDRPlayBandwidths::m_bw[m_settings.m_bandwidthIndex])) < 0)
	{
		qCritical("SDRPlayInput::start: could not set bandwidth at index %d: rc: %d", m_settings.m_bandwidthIndex, res);
		stop();
		return false;
	}

	if ((res = mirisdr_set_tuner_gain_mode(m_dev, 1)) < 0)
	{
		qCritical("SDRPlayInput::start: error setting tuner gain mode");
		stop();
		return false;
	}

	numberOfGains = mirisdr_get_tuner_gains(m_dev, 0);

	if (numberOfGains < 0)
	{
		qCritical("SDRPlayInput::start: error getting number of gain values supported");
		stop();
		return false;
	}
	else
	{
		qDebug("SDRPlayInput::start: supported gain values: %d", numberOfGains);
	}

	m_gains.resize(numberOfGains);

	if (mirisdr_get_tuner_gains(m_dev, &m_gains[0]) < 0)
	{
		qCritical("SDRPlayInput::start: error getting gain values");
		stop();
		return false;
	}
	else
	{
		qDebug() << "SDRPlayInput::start: " << m_gains.size() << "gains";
		MsgReportSDRPlay *message = MsgReportSDRPlay::create(m_gains);
		getOutputMessageQueueToGUI()->push(message);
	}

	if ((res = mirisdr_reset_buffer(m_dev)) < 0)
	{
		qCritical("SDRPlayInput::start: could not reset USB EP buffers: %s", strerror(errno));
		stop();
		return false;
	}

    if((m_sdrPlayThread = new SDRPlayThread(&m_sampleFifo)) == 0)
    {
        qFatal("SDRPlayInput::start: failed to create thread");
        return false;
    }

    m_sdrPlayThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);
}

void SDRPlayInput::stop()
{
    mir_sdr_ErrT r;
    QMutexLocker mutexLocker(&m_mutex);

    r = mir_sdr_StreamUninit();

    if (r != mir_sdr_Success)
    {
        qCritical("SDRPlayInput::stop: stream uninit failed with code %d", (int) r);
    }

    m_mirStreamRunning = false;

    if(m_sdrPlayThread != 0)
    {
        m_sdrPlayThread->stopWork();
        delete m_sdrPlayThread;
        m_sdrPlayThread = 0;
    }
}

const QString& SDRPlayInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SDRPlayInput::getSampleRate() const
{
    int rate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);
    return (rate * 1000) / (1<<m_settings.m_log2Decim);
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

        bool success = applySettings(conf.getSettings(), false);

        if (!success)
        {
            qDebug("SDRPlayInput::handleMessage: config error");
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool SDRPlayInput::applySettings(const SDRPlaySettings& settings, bool force)
{
    bool forwardChange = false;
    QMutexLocker mutexLocker(&m_mutex);

    if (m_settings.m_dcBlock != settings.m_dcBlock)
    {
        m_settings.m_dcBlock = settings.m_dcBlock;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    if (m_settings.m_iqCorrection != settings.m_iqCorrection)
    {
        m_settings.m_iqCorrection = settings.m_iqCorrection;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        m_settings.m_log2Decim = settings.m_log2Decim;
        forwardChange = true;

        if (m_sdrPlayThread != 0)
        {
            m_sdrPlayThread->setLog2Decimation(m_settings.m_log2Decim);
            qDebug() << "SDRPlayInput: set decimation to " << (1<<m_settings.m_log2Decim);
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

    if ((m_settings.m_devSampleRateIndex != settings.m_devSampleRateIndex) || force)
    {
        m_settings.m_devSampleRateIndex = settings.m_devSampleRateIndex;
        forwardChange = true;

        if (m_mirStreamRunning)
        {
            reinitMirSDR(mir_sdr_CHANGE_FS_FREQ);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        forwardChange = true;

        if (m_mirStreamRunning)
        {
            reinitMirSDR(mir_sdr_CHANGE_RF_FREQ);
        }
    }

    if ((m_settings.m_frequencyBandIndex != settings.m_frequencyBandIndex) || force)
    {
        m_settings.m_frequencyBandIndex = settings.m_frequencyBandIndex;
        // change of frequency is done already
    }

    if ((m_settings.m_bandwidthIndex != settings.m_bandwidthIndex) || force)
    {
        m_settings.m_bandwidthIndex = settings.m_bandwidthIndex;

        if (m_mirStreamRunning)
        {
            reinitMirSDR(mir_sdr_CHANGE_BW_TYPE);
        }
    }

    // TODO: change IF mode
    // TODO: change LO mode

    if ((m_settings.m_gainRedctionIndex != settings.m_gainRedctionIndex) || force)
    {
        if (m_settings.m_bandwidthIndex < 4)
        {
            m_settings.m_gainRedctionIndex = settings.m_gainRedctionIndex;

            if (m_mirStreamRunning)
            {
                reinitMirSDR(mir_sdr_CHANGE_GR);
            }
        }
        else
        {
            if (settings.m_gainRedctionIndex > 85)
            {
                if (m_settings.m_gainRedctionIndex < 85)
                {
                    m_settings.m_gainRedctionIndex = 85;

                    if (m_mirStreamRunning)
                    {
                        reinitMirSDR(mir_sdr_CHANGE_GR);
                    }
                }
            }
            else
            {
                m_settings.m_gainRedctionIndex = settings.m_gainRedctionIndex;

                if (m_mirStreamRunning)
                {
                    reinitMirSDR(mir_sdr_CHANGE_GR);
                }
            }
        }
    }

    if ((m_settings.m_LOppmCorrection != settings.m_LOppmCorrection) || force)
    {
        m_settings.m_LOppmCorrection = settings.m_LOppmCorrection;

        mir_sdr_ErrT r = mir_sdr_SetPpm(m_settings.m_LOppmCorrection / 10.0);

        if (r != mir_sdr_Success)
        {
            qDebug("SDRPlayInput::applySettings: mir_sdr_SetPpm failed with code %d", (int) r);
        }
    }

    if ((m_settings.m_mirDcCorrIndex != settings.m_mirDcCorrIndex) || force)
    {
        m_settings.m_mirDcCorrIndex = settings.m_mirDcCorrIndex;

        if (m_mirStreamRunning)
        {
            mir_sdr_ErrT r = mir_sdr_SetDcMode(m_settings.m_mirDcCorrIndex, 0);

            if (r != mir_sdr_Success)
            {
                qDebug("SDRPlayInput::applySettings: mir_sdr_SetDcMode failed with code %d", (int) r);
            }
        }
    }

    if ((m_settings.m_mirDcCorrTrackTimeIndex != settings.m_mirDcCorrTrackTimeIndex) || force)
    {
        m_settings.m_mirDcCorrTrackTimeIndex = settings.m_mirDcCorrTrackTimeIndex;

        if (m_mirStreamRunning)
        {
            mir_sdr_ErrT r = mir_sdr_SetDcTrackTime(m_settings.m_mirDcCorrTrackTimeIndex);

            if (r != mir_sdr_Success)
            {
                qDebug("SDRPlayInput::applySettings: mir_sdr_SetDcTrackTime failed with code %d", (int) r);
            }
        }
    }

    if (forwardChange)
    {
        int sampleRate = getSampleRate();
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
    }

    return true;
}

void SDRPlayInput::reinitMirSDR(mir_sdr_ReasonForReinitT reasonForReinit)
{
    int grdB = -m_settings.m_gainRedctionIndex;
    int rate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);
    int gRdBsystem;

    mir_sdr_ErrT r = mir_sdr_Reinit(
            &grdB,
            rate / 1e3,
            m_settings.m_centerFrequency / 1e6,
            (mir_sdr_Bw_MHzT) m_settings.m_bandwidthIndex,
            mir_sdr_IF_Zero,
            mir_sdr_LO_Auto,
            1, // LNA
            &gRdBsystem,
            0, // use mir_sdr_SetGr() to set initial gain reduction
            &m_samplesPerPacket,
            reasonForReinit);

    if (r != mir_sdr_Success)
    {
        qCritical("SDRPlayInput::reinitMirSDR (%d): MirSDR stream reinit failed with code %d", (int) reasonForReinit, (int) r);
        m_mirStreamRunning = false;
    }
    else
    {
        qDebug("SDRPlayInput::reinitMirSDR (%d): MirSDR stream restarted: samplesPerPacket: %d", (int) reasonForReinit, m_samplesPerPacket);
    }
}

void SDRPlayInput::callbackGC(unsigned int gRdB, unsigned int lnaGRdB, void *cbContext)
{
    return;
}
