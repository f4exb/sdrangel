///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_HACKRFINPUT_H
#define INCLUDE_HACKRFINPUT_H

#include <dsp/devicesamplesource.h>
#include "libhackrf/hackrf.h"
#include <QString>
#include "hackrfinputsettings.h"

class DeviceSourceAPI;
class HackRFInputThread;

class HackRFInput : public DeviceSampleSource {
public:

	class MsgConfigureHackRF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const HackRFInputSettings& getSettings() const { return m_settings; }

		static MsgConfigureHackRF* create(const HackRFInputSettings& settings)
		{
			return new MsgConfigureHackRF(settings);
		}

	private:
		HackRFInputSettings m_settings;

		MsgConfigureHackRF(const HackRFInputSettings& settings) :
			Message(),
			m_settings(settings)
		{ }
	};

	class MsgReportHackRF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgReportHackRF* create()
		{
			return new MsgReportHackRF();
		}

	protected:

		MsgReportHackRF() :
			Message()
		{ }
	};

	HackRFInput(DeviceSourceAPI *deviceAPI);
	virtual ~HackRFInput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;

	virtual bool handleMessage(const Message& message);

private:
	bool applySettings(const HackRFInputSettings& settings, bool force);
	hackrf_device *open_hackrf_from_sequence(int sequence);
	void setCenterFrequency(quint64 freq);

	DeviceSourceAPI *m_deviceAPI;
	QMutex m_mutex;
	HackRFInputSettings m_settings;
	struct hackrf_device* m_dev;
	HackRFInputThread* m_hackRFThread;
	QString m_deviceDescription;
};

#endif // INCLUDE_HACKRFINPUT_H
