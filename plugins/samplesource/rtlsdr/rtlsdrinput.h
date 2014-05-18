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

#include "dsp/samplesource/samplesource.h"
#include <rtl-sdr.h>
#include <QString>

class RTLSDRThread;

class RTLSDRInput : public SampleSource {
public:
	struct Settings {
		qint32 m_gain;
		qint32 m_decimation;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureRTLSDR : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureRTLSDR* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureRTLSDR(generalSettings, settings);
		}

	private:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureRTLSDR(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
		{ }
	};

	class MsgReportRTLSDR : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const std::vector<int>& getGains() const { return m_gains; }

		static MsgReportRTLSDR* create(const std::vector<int>& gains)
		{
			return new MsgReportRTLSDR(gains);
		}

	protected:
		std::vector<int> m_gains;

		MsgReportRTLSDR(const std::vector<int>& gains) :
			Message(),
			m_gains(gains)
		{ }
	};

	RTLSDRInput(MessageQueue* msgQueueToGUI);
	~RTLSDRInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;

	bool handleMessage(Message* message);

private:
	QMutex m_mutex;
	Settings m_settings;
	rtlsdr_dev_t* m_dev;
	RTLSDRThread* m_rtlSDRThread;
	QString m_deviceDescription;
	std::vector<int> m_gains;

	bool applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force);
};

#endif // INCLUDE_RTLSDRINPUT_H
