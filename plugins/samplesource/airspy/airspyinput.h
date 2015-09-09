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

#include "dsp/samplesource.h"
#include <libairspy/airspy.h>
#include <QString>

class AirspyThread;

class AirspyInput : public SampleSource {
public:
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

	struct Settings {
		quint64 m_centerFrequency;
		qint32  m_LOppmTenths;
		int m_devSampleRate;
		quint32 m_lnaGain;
		quint32 m_mixerGain;
		quint32 m_vgaGain;
		quint32 m_log2Decim;
		fcPos_t m_fcPos;
		bool m_biasT;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureAirspy : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureAirspy* create(const Settings& settings)
		{
			return new MsgConfigureAirspy(settings);
		}

	private:
		Settings m_settings;

		MsgConfigureAirspy(const Settings& settings) :
			Message(),
			m_settings(settings)
		{ }
	};

	class MsgReportAirspy : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const std::vector<int>& getSampleRates() const { return m_sampleRates; }

		static MsgReportAirspy* create(const std::vector<int>& sampleRates)
		{
			return new MsgReportAirspy(sampleRates);
		}

	protected:
		std::vector<int> m_sampleRates;

		MsgReportAirspy(const std::vector<int>& sampleRates) :
			Message(),
			m_sampleRates(sampleRates)
		{ }
	};

	AirspyInput();
	virtual ~AirspyInput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;

	virtual bool handleMessage(const Message& message);

private:
	bool applySettings(const Settings& settings, bool force);
	struct airspy_device *open_airspy_from_sequence(int sequence);

	QMutex m_mutex;
	Settings m_settings;
	struct airspy_device* m_dev;
	AirspyThread* m_airspyThread;
	QString m_deviceDescription;
	std::vector<int> m_sampleRates;
};

#endif // INCLUDE_AIRSPYINPUT_H
