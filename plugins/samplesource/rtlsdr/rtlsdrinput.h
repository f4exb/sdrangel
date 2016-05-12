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

#include "dsp/samplesource.h"
#include "rtlsdrsettings.h"
#include <rtl-sdr.h>
#include <QString>

class PluginAPI;
class RTLSDRThread;

class RTLSDRInput : public SampleSource {
public:
	class MsgConfigureRTLSDR : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const RTLSDRSettings& getSettings() const { return m_settings; }

		static MsgConfigureRTLSDR* create(const RTLSDRSettings& settings)
		{
			return new MsgConfigureRTLSDR(settings);
		}

	private:
		RTLSDRSettings m_settings;

		MsgConfigureRTLSDR(const RTLSDRSettings& settings) :
			Message(),
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

	RTLSDRInput(PluginAPI *pluginAPI);
	virtual ~RTLSDRInput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;

	virtual bool handleMessage(const Message& message);

	void set_ds_mode(int on);

private:
	PluginAPI *m_pluginAPI;
	QMutex m_mutex;
	RTLSDRSettings m_settings;
	rtlsdr_dev_t* m_dev;
	RTLSDRThread* m_rtlSDRThread;
	QString m_deviceDescription;
	std::vector<int> m_gains;

	bool applySettings(const RTLSDRSettings& settings, bool force);
};

#endif // INCLUDE_RTLSDRINPUT_H
