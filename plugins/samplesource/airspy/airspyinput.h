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

#ifndef INCLUDE_AIRSPYINPUT_H
#define INCLUDE_AIRSPYINPUT_H

#include <dsp/devicesamplesource.h>

#include "airspysettings.h"
#include <libairspy/airspy.h>
#include <QString>

class DeviceSourceAPI;
class AirspyThread;
class FileRecord;

class AirspyInput : public DeviceSampleSource {
public:
	class MsgConfigureAirspy : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const AirspySettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureAirspy* create(const AirspySettings& settings, bool force)
		{
			return new MsgConfigureAirspy(settings, force);
		}

	private:
		AirspySettings m_settings;
		bool m_force;

		MsgConfigureAirspy(const AirspySettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
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

	AirspyInput(DeviceSourceAPI *deviceAPI);
	virtual ~AirspyInput();
	virtual void destroy();

	virtual bool start();
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
	const std::vector<uint32_t>& getSampleRates() const { return m_sampleRates; }

	virtual bool handleMessage(const Message& message);

	static const qint64 loLowLimitFreq;
	static const qint64 loHighLimitFreq;

private:
	bool openDevice();
	void closeDevice();
	bool applySettings(const AirspySettings& settings, bool force);
	struct airspy_device *open_airspy_from_sequence(int sequence);
	void setCenterFrequency(quint64 freq);

	DeviceSourceAPI *m_deviceAPI;
	QMutex m_mutex;
	AirspySettings m_settings;
	struct airspy_device* m_dev;
	AirspyThread* m_airspyThread;
	QString m_deviceDescription;
	std::vector<uint32_t> m_sampleRates;
	bool m_running;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
};

#endif // INCLUDE_AIRSPYINPUT_H
