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

#ifndef INCLUDE_AIRSPYHFINPUT_H
#define INCLUDE_AIRSPYHFINPUT_H

#include <QString>
#include <QByteArray>

#include <libairspyhf/airspyhf.h>
#include <dsp/devicesamplesource.h>
#include "airspyhfsettings.h"

class DeviceSourceAPI;
class AirspyHFThread;
class FileRecord;

class AirspyHFInput : public DeviceSampleSource {
public:
	class MsgConfigureAirspyHF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const AirspyHFSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureAirspyHF* create(const AirspyHFSettings& settings, bool force)
		{
			return new MsgConfigureAirspyHF(settings, force);
		}

	private:
		AirspyHFSettings m_settings;
		bool m_force;

		MsgConfigureAirspyHF(const AirspyHFSettings& settings, bool force) :
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

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

	AirspyHFInput(DeviceSourceAPI *deviceAPI);
	virtual ~AirspyHFInput();
	virtual void destroy();

	virtual void init();
	virtual bool start();
	virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

	virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);
	const std::vector<uint32_t>& getSampleRates() const { return m_sampleRates; }

	virtual bool handleMessage(const Message& message);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    static const qint64 loLowLimitFreqHF;
	static const qint64 loHighLimitFreqHF;
    static const qint64 loLowLimitFreqVHF;
    static const qint64 loHighLimitFreqVHF;

private:
	bool openDevice();
	void closeDevice();
	bool applySettings(const AirspyHFSettings& settings, bool force);
	airspyhf_device_t *open_airspyhf_from_serial(const QString& serialStr);
	void setDeviceCenterFrequency(quint64 freq, const AirspyHFSettings& settings);

	DeviceSourceAPI *m_deviceAPI;
	QMutex m_mutex;
	AirspyHFSettings m_settings;
	airspyhf_device_t* m_dev;
	AirspyHFThread* m_airspyHFThread;
	QString m_deviceDescription;
	std::vector<uint32_t> m_sampleRates;
	bool m_running;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
};

#endif // INCLUDE_AIRSPYHFINPUT_H
