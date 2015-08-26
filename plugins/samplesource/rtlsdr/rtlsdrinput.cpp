///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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
#include <string.h>
#include <errno.h>
#include "rtlsdrinput.h"
#include "rtlsdrthread.h"
#include "rtlsdrgui.h"
#include "dsp/dspcommands.h"
#include "util/simpleserializer.h"

MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgConfigureRTLSDR, Message)
MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgReportRTLSDR, Message)

RTLSDRInput::Settings::Settings() :
	m_devSampleRate(1024*1000),
	m_centerFrequency(435000*1000),
	m_gain(0),
	m_loPpmCorrection(0),
	m_log2Decim(4)
{
}

void RTLSDRInput::Settings::resetToDefaults()
{
	m_devSampleRate = 1024*1000;
	m_centerFrequency = 435000*1000;
	m_gain = 0;
	m_loPpmCorrection = 0;
	m_log2Decim = 4;
}

QByteArray RTLSDRInput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_devSampleRate);
	s.writeU64(2, m_centerFrequency);
	s.writeS32(3, m_gain);
	s.writeS32(4, m_loPpmCorrection);
	s.writeU32(5, m_log2Decim);
	return s.final();
}

bool RTLSDRInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1)
	{
		d.readS32(1, &m_devSampleRate, 1024*1000);
		d.readU64(2, &m_centerFrequency, 435000*1000);
		d.readS32(3, &m_gain, 0);
		d.readS32(4, &m_loPpmCorrection, 0);
		d.readU32(5, &m_log2Decim, 4);
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

RTLSDRInput::RTLSDRInput() :
	m_settings(),
	m_dev(0),
	m_rtlSDRThread(0),
	m_deviceDescription()
{
}

RTLSDRInput::~RTLSDRInput()
{
	stop();
}

bool RTLSDRInput::init(const Message& message)
{
	return false;
}

bool RTLSDRInput::start(int device)
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
		qCritical("RTLSDRInput::start: Could not allocate SampleFifo");
		return false;
	}

	if ((res = rtlsdr_open(&m_dev, device)) < 0)
	{
		qCritical("RTLSDRInput::start: could not open RTLSDR #%d: %s", device, strerror(errno));
		return false;
	}

	vendor[0] = '\0';
	product[0] = '\0';
	serial[0] = '\0';

	if ((res = rtlsdr_get_usb_strings(m_dev, vendor, product, serial)) < 0)
	{
		qCritical("RTLSDRInput::start: error accessing USB device");
		stop();
		return false;
	}

	qWarning("RTLSDRInput::start: open: %s %s, SN: %s", vendor, product, serial);
	m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

	if ((res = rtlsdr_set_sample_rate(m_dev, 1152000)) < 0)
	{
		qCritical("RTLSDRInput::start: could not set sample rate: 1024k S/s");
		stop();
		return false;
	}

	if ((res = rtlsdr_set_tuner_gain_mode(m_dev, 1)) < 0)
	{
		qCritical("RTLSDRInput::start: error setting tuner gain mode");
		stop();
		return false;
	}

	if ((res = rtlsdr_set_agc_mode(m_dev, 0)) < 0)
	{
		qCritical("RTLSDRInput::start: error setting agc mode");
		stop();
		return false;
	}

	numberOfGains = rtlsdr_get_tuner_gains(m_dev, NULL);

	if (numberOfGains < 0)
	{
		qCritical("RTLSDRInput::start: error getting number of gain values supported");
		stop();
		return false;
	}

	m_gains.resize(numberOfGains);

	if (rtlsdr_get_tuner_gains(m_dev, &m_gains[0]) < 0)
	{
		qCritical("RTLSDRInput::start: error getting gain values");
		stop();
		return false;
	}
	else
	{
		qDebug() << "RTLSDRInput::start: " << m_gains.size() << "gains";
		MsgReportRTLSDR *message = MsgReportRTLSDR::create(m_gains);
		getOutputMessageQueueToGUI()->push(message);
	}

	if ((res = rtlsdr_reset_buffer(m_dev)) < 0)
	{
		qCritical("RTLSDRInput::start: could not reset USB EP buffers: %s", strerror(errno));
		stop();
		return false;
	}

	if ((m_rtlSDRThread = new RTLSDRThread(m_dev, &m_sampleFifo)) == NULL)
	{
		qFatal("RTLSDRInput::start: out of memory");
		stop();
		return false;
	}

	m_rtlSDRThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);

	return true;
}

void RTLSDRInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_rtlSDRThread != 0)
	{
		m_rtlSDRThread->stopWork();
		delete m_rtlSDRThread;
		m_rtlSDRThread = 0;
	}

	if (m_dev != 0)
	{
		rtlsdr_close(m_dev);
		m_dev = 0;
	}

	m_deviceDescription.clear();
}

const QString& RTLSDRInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int RTLSDRInput::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 RTLSDRInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool RTLSDRInput::handleMessage(const Message& message)
{
	if (MsgConfigureRTLSDR::match(message))
	{
		MsgConfigureRTLSDR& conf = (MsgConfigureRTLSDR&) message;

		if (!applySettings(conf.getSettings(), false))
		{
			qDebug("RTLSDR config error");
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool RTLSDRInput::applySettings(const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);
    bool forwardChange = false;

	if ((m_settings.m_gain != settings.m_gain) || force)
	{
		m_settings.m_gain = settings.m_gain;

		if(m_dev != 0)
		{
			if(rtlsdr_set_tuner_gain(m_dev, m_settings.m_gain) != 0)
			{
				qDebug("rtlsdr_set_tuner_gain() failed");
			}
		}
	}

	if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
	{
        forwardChange = true;
        
		if(m_dev != 0)
		{
			if( rtlsdr_set_sample_rate(m_dev, settings.m_devSampleRate) < 0)
			{
				qCritical("could not set sample rate: %d", settings.m_devSampleRate);
			}
			else
			{
				m_settings.m_devSampleRate = settings.m_devSampleRate;
				m_rtlSDRThread->setSamplerate(settings.m_devSampleRate);
			}
		}
	}

	if ((m_settings.m_loPpmCorrection != settings.m_loPpmCorrection) || force)
	{
		if (m_dev != 0)
		{
			if (rtlsdr_set_freq_correction(m_dev, settings.m_loPpmCorrection) < 0)
			{
				qCritical("could not set LO ppm correction: %d", settings.m_loPpmCorrection);
			}
			else
			{
				m_settings.m_loPpmCorrection = settings.m_loPpmCorrection;
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
        forwardChange = true;
        
		if(m_dev != 0)
		{
			m_settings.m_log2Decim = settings.m_log2Decim;
			m_rtlSDRThread->setLog2Decimation(settings.m_log2Decim);
		}
	}

    if (m_settings.m_centerFrequency != settings.m_centerFrequency)
    {
        forwardChange = true;
    }

	m_settings.m_centerFrequency = settings.m_centerFrequency;

	if(m_dev != 0)
	{
		qint64 centerFrequency = m_settings.m_centerFrequency + (m_settings.m_devSampleRate / 4);

		if (m_settings.m_log2Decim == 0)
		{ // Little wooby-doop if no decimation
			centerFrequency = m_settings.m_centerFrequency;
		}
		else
		{
			centerFrequency = m_settings.m_centerFrequency + (m_settings.m_devSampleRate / 4);
		}

		if (rtlsdr_set_center_freq( m_dev, centerFrequency ) != 0)
		{
			qDebug("rtlsdr_set_center_freq(%lld) failed", m_settings.m_centerFrequency);
		}
	}
    
    if (forwardChange)
    {
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		getOutputMessageQueue()->push(notif);        
    }

	return true;
}

void RTLSDRInput::set_ds_mode(int on)
{
	rtlsdr_set_direct_sampling(m_dev, on);
}

