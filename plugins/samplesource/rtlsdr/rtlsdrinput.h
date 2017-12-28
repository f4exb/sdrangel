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

#ifndef INCLUDE_RTLSDRINPUT_H
#define INCLUDE_RTLSDRINPUT_H

#include <QString>
#include <QByteArray>

#include <dsp/devicesamplesource.h>
#include "rtlsdrsettings.h"
#include <rtl-sdr.h>

class DeviceSourceAPI;
class RTLSDRThread;
class FileRecord;

class RTLSDRInput : public DeviceSampleSource {
public:
	class MsgConfigureRTLSDR : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const RTLSDRSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureRTLSDR* create(const RTLSDRSettings& settings, bool force)
		{
			return new MsgConfigureRTLSDR(settings, force);
		}

	private:
		RTLSDRSettings m_settings;
		bool m_force;

		MsgConfigureRTLSDR(const RTLSDRSettings& settings, bool force) :
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

	RTLSDRInput(DeviceSourceAPI *deviceAPI);
	virtual ~RTLSDRInput();
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

	virtual bool handleMessage(const Message& message);

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

	const std::vector<int>& getGains() const { return m_gains; }
	void set_ds_mode(int on);

	static const quint64 frequencyLowRangeMin;
	static const quint64 frequencyLowRangeMax;
    static const quint64 frequencyHighRangeMin;
    static const quint64 frequencyHighRangeMax;
	static const int sampleRateLowRangeMin;
    static const int sampleRateLowRangeMax;
    static const int sampleRateHighRangeMin;
    static const int sampleRateHighRangeMax;

private:
	DeviceSourceAPI *m_deviceAPI;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
	QMutex m_mutex;
	RTLSDRSettings m_settings;
	rtlsdr_dev_t* m_dev;
	RTLSDRThread* m_rtlSDRThread;
	QString m_deviceDescription;
	std::vector<int> m_gains;
	bool m_running;

	bool openDevice();
	void closeDevice();
	bool applySettings(const RTLSDRSettings& settings, bool force);
	void webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const RTLSDRSettings& settings);
};

#endif // INCLUDE_RTLSDRINPUT_H
