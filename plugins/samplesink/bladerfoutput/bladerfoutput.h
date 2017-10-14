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

#ifndef INCLUDE_BLADERFOUTPUT_H
#define INCLUDE_BLADERFOUTPUT_H

#include <dsp/devicesamplesink.h>
#include "bladerf/devicebladerf.h"
#include "bladerf/devicebladerfparam.h"

#include <libbladeRF.h>
#include <QString>

#include "bladerfoutputsettings.h"

class DeviceSinkAPI;
class BladerfOutputThread;

class BladerfOutput : public DeviceSampleSink {
public:
	class MsgConfigureBladerf : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const BladeRFOutputSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureBladerf* create(const BladeRFOutputSettings& settings, bool force)
		{
			return new MsgConfigureBladerf(settings, force);
		}

	private:
		BladeRFOutputSettings m_settings;
		bool m_force;

		MsgConfigureBladerf(const BladeRFOutputSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
		{ }
	};

	class MsgReportBladerf : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgReportBladerf* create()
		{
			return new MsgReportBladerf();
		}

	protected:

		MsgReportBladerf() :
			Message()
		{ }
	};

	BladerfOutput(DeviceSinkAPI *deviceAPI);
	virtual ~BladerfOutput();
	virtual void destroy();

	virtual bool start();
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;

	virtual bool handleMessage(const Message& message);

private:
    bool openDevice();
    void closeDevice();
	bool applySettings(const BladeRFOutputSettings& settings, bool force);

	DeviceSinkAPI *m_deviceAPI;
	QMutex m_mutex;
	BladeRFOutputSettings m_settings;
	struct bladerf* m_dev;
	BladerfOutputThread* m_bladerfThread;
	QString m_deviceDescription;
    DeviceBladeRFParams m_sharedParams;
    bool m_running;
};

#endif // INCLUDE_BLADERFOUTPUT_H
