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

// FIXME: FCD is handled very badly!

#include <QDebug>
#include <string.h>
#include <errno.h>

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include <dsp/filerecord.h>
#include "fcdproplusinput.h"

#include <device/devicesourceapi.h>

#include "fcdproplusgui.h"
#include "fcdproplusthread.h"
#include "fcdtraits.h"
#include "fcdproplusconst.h"

MESSAGE_CLASS_DEFINITION(FCDProPlusInput::MsgConfigureFCD, Message)
MESSAGE_CLASS_DEFINITION(FCDProPlusInput::MsgFileRecord, Message)

FCDProPlusInput::FCDProPlusInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_dev(0),
	m_settings(),
	m_FCDThread(0),
	m_deviceDescription(fcd_traits<ProPlus>::displayedName),
	m_running(false)
{
    openDevice();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}

FCDProPlusInput::~FCDProPlusInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void FCDProPlusInput::destroy()
{
    delete this;
}

bool FCDProPlusInput::openDevice()
{
    int device = m_deviceAPI->getSampleSourceSequence();
    qDebug() << "FCDProPlusInput::openDevice with device #" << device;

    m_dev = fcdOpen(fcd_traits<ProPlus>::vendorId, fcd_traits<ProPlus>::productId, device);

    if (m_dev == 0)
    {
        qCritical("FCDProPlusInput::start: could not open FCD");
        return false;
    }

    return true;
}

bool FCDProPlusInput::start()
{

//	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

    qDebug() << "FCDProPlusInput::start";

	/* Apply settings before streaming to avoid bus contention;
	 * there is very little spare bandwidth on a full speed USB device.
	 * Failure is harmless if no device is found
	 * ... This is rubbish...*/

	//applySettings(m_settings, true);

	if(!m_sampleFifo.setSize(96000*4))
	{
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if ((m_FCDThread = new FCDProPlusThread(&m_sampleFifo)) == NULL)
	{
		qFatal("out of memory");
		return false;
	}

	m_FCDThread->startWork();

//	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("FCDProPlusInput::started");
	m_running = true;

	return true;
}

void FCDProPlusInput::closeDevice()
{
    if (m_dev == 0) { // was never open
        return;
    }

    fcdClose(m_dev);
    m_dev = 0;
}

void FCDProPlusInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		m_FCDThread->stopWork();
		// wait for thread to quit ?
		delete m_FCDThread;
		m_FCDThread = 0;
	}

	m_running = false;
}

const QString& FCDProPlusInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FCDProPlusInput::getSampleRate() const
{
	return fcd_traits<ProPlus>::sampleRate;
}

quint64 FCDProPlusInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool FCDProPlusInput::handleMessage(const Message& message)
{
	if(MsgConfigureFCD::match(message))
	{
		qDebug() << "FCDProPlusInput::handleMessage: MsgConfigureFCD";
		MsgConfigureFCD& conf = (MsgConfigureFCD&) message;
		applySettings(conf.getSettings(), conf.getForce());
		return true;
	}
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "FCDProPlusInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

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

void FCDProPlusInput::applySettings(const FCDProPlusSettings& settings, bool force)
{
	bool signalChange = false;

	if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
            || (m_settings.m_transverterMode != settings.m_transverterMode)
            || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency))
	{
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

        if (m_dev != 0)
        {
            set_center_freq((double) deviceCenterFrequency);
        }

        qDebug() << "FCDProPlusInput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
                << " device center freq: " << deviceCenterFrequency << " Hz";

        m_settings.m_centerFrequency = settings.m_centerFrequency;
        signalChange = true;
	}

	if ((m_settings.m_lnaGain != settings.m_lnaGain) || force)
	{
		m_settings.m_lnaGain = settings.m_lnaGain;

		if (m_dev != 0)
		{
			set_lna_gain(settings.m_lnaGain);
		}
	}

	if ((m_settings.m_biasT != settings.m_biasT) || force)
	{
		m_settings.m_biasT = settings.m_biasT;

		if (m_dev != 0)
		{
			set_bias_t(settings.m_biasT);
		}
	}

	if ((m_settings.m_mixGain != settings.m_mixGain) || force)
	{
		m_settings.m_mixGain = settings.m_mixGain;

		if (m_dev != 0)
		{
			set_mixer_gain(settings.m_mixGain);
		}
	}

	if ((m_settings.m_ifGain != settings.m_ifGain) || force)
	{
		m_settings.m_ifGain = settings.m_ifGain;

		if (m_dev != 0)
		{
			set_if_gain(settings.m_ifGain);
		}
	}

	if ((m_settings.m_ifFilterIndex != settings.m_ifFilterIndex) || force)
	{
		m_settings.m_ifFilterIndex = settings.m_ifFilterIndex;

		if (m_dev != 0)
		{
			set_if_filter(settings.m_ifFilterIndex);
		}
	}

	if ((m_settings.m_rfFilterIndex != settings.m_rfFilterIndex) || force)
	{
		m_settings.m_rfFilterIndex = settings.m_rfFilterIndex;

		if (m_dev != 0)
		{
			set_rf_filter(settings.m_rfFilterIndex);
		}
	}

	if ((m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force)
	{
		m_settings.m_LOppmTenths = settings.m_LOppmTenths;

		if (m_dev != 0)
		{
			set_lo_ppm();
		}
	}

	if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
	{
		m_settings.m_dcBlock = settings.m_dcBlock;
		m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqImbalance);
	}

	if ((m_settings.m_iqImbalance != settings.m_iqImbalance) || force)
	{
		m_settings.m_iqImbalance = settings.m_iqImbalance;
		m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqImbalance);
	}

	if (signalChange)
    {
		DSPSignalNotification *notif = new DSPSignalNotification(fcd_traits<ProPlus>::sampleRate, m_settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

void FCDProPlusInput::set_center_freq(double freq)
{
	freq += freq*(((double) m_settings.m_LOppmTenths)/10000000.0);

	if (fcdAppSetFreq(m_dev, freq) == FCD_MODE_NONE)
	{
		qDebug("No FCD HID found for frquency change");
	}
}

void FCDProPlusInput::set_bias_t(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_BIAS_TEE, &cmd, 1);
}

void FCDProPlusInput::set_lna_gain(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_LNA_GAIN, &cmd, 1);
}

void FCDProPlusInput::set_mixer_gain(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_MIXER_GAIN, &cmd, 1);
}

void FCDProPlusInput::set_if_gain(int gain)
{
	if (gain < 0)
	{
		return;
	}

	quint8 cmd_value = gain;

	if (fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_IF_GAIN, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_if_gain: failed to set at " << cmd_value;
	}
}

void FCDProPlusInput::set_if_filter(int filterIndex)
{
	if ((filterIndex < 0) || (filterIndex >= FCDProPlusConstants::fcdproplus_if_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProPlusConstants::if_filters[filterIndex].value;

	if (fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_IF_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_if_filter: failed to set at " << cmd_value;
	}
}


void FCDProPlusInput::set_rf_filter(int filterIndex)
{
	if ((filterIndex < 0) || (filterIndex >= FCDProPlusConstants::fcdproplus_rf_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProPlusConstants::rf_filters[filterIndex].value;

	if (fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_RF_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_rf_filter: failed to set at " << cmd_value;
	}
}

void FCDProPlusInput::set_lo_ppm()
{
	set_center_freq((double) m_settings.m_centerFrequency);
}




