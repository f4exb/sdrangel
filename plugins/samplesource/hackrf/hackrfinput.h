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

#include "dsp/samplesource.h"
#include <libhackrf/hackrf.h>
#include <QString>

class HackRFThread;

class HackRFInput : public SampleSource {
public:
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

	struct Settings {
		quint64 m_centerFrequency;
		qint32  m_LOppmTenths;
		quint32 m_devSampleRateIndex;
		quint32 m_bandwidthIndex;
		quint32 m_imjRejFilterIndex;
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

	class MsgConfigureHackRT : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureHackRT* create(const Settings& settings)
		{
			return new MsgConfigureHackRT(settings);
		}

	private:
		Settings m_settings;

		MsgConfigureHackRT(const Settings& settings) :
			Message(),
			m_settings(settings)
		{ }
	};

	class MsgReportHackRF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const std::vector<uint32_t>& getSampleRates() const { return m_sampleRates; }

		static MsgReportHackRF* create(const std::vector<uint32_t>& sampleRates)
		{
			return new MsgReportHackRF(sampleRates);
		}

	protected:
		std::vector<uint32_t> m_sampleRates;

		MsgReportHackRF(const std::vector<uint32_t>& sampleRates) :
			Message(),
			m_sampleRates(sampleRates)
		{ }
	};

	HackRFInput();
	virtual ~HackRFInput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
	const std::vector<uint32_t>& getSampleRates() const { return m_sampleRates; }

	virtual bool handleMessage(const Message& message);

private:
	bool applySettings(const Settings& settings, bool force);
	struct airspy_device *open_airspy_from_sequence(int sequence);
	void setCenterFrequency(quint64 freq);

	QMutex m_mutex;
	Settings m_settings;
	struct airspy_device* m_dev;
	HackRFThread* m_airspyThread;
	QString m_deviceDescription;
	std::vector<uint32_t> m_sampleRates;
};

#endif // INCLUDE_HACKRFINPUT_H
