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

#include "hackrf/devicehackrf.h"
#include "hackrf/devicehackrfparam.h"
#include "hackrfinputsettings.h"

class DeviceSourceAPI;
class HackRFInputThread;
class FileRecord;

class HackRFInput : public DeviceSampleSource {
public:

	class MsgConfigureHackRF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const HackRFInputSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureHackRF* create(const HackRFInputSettings& settings, bool force = false)
		{
			return new MsgConfigureHackRF(settings, force);
		}

	private:
		HackRFInputSettings m_settings;
		bool m_force;

		MsgConfigureHackRF(const HackRFInputSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
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

    class MsgFileRecord : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgFileRecord* create(bool startStop) {
            return new MsgFileRecord(startStop);
        }

    protected:
        bool m_startStop;

        MsgFileRecord(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

	HackRFInput(DeviceSourceAPI *deviceAPI);
	virtual ~HackRFInput();
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
	bool applySettings(const HackRFInputSettings& settings, bool force);
//	hackrf_device *open_hackrf_from_sequence(int sequence);
	void setCenterFrequency(quint64 freq);

	DeviceSourceAPI *m_deviceAPI;
	QMutex m_mutex;
	HackRFInputSettings m_settings;
	struct hackrf_device* m_dev;
	HackRFInputThread* m_hackRFThread;
	QString m_deviceDescription;
	DeviceHackRFParams m_sharedParams;
	bool m_running;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
};

#endif // INCLUDE_HACKRFINPUT_H
