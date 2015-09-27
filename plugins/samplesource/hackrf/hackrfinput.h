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
#include "libhackrf/hackrf.h"
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
		quint32 m_lnaGain;
		quint32 m_vgaGain;
		quint32 m_log2Decim;
		fcPos_t m_fcPos;
		bool m_biasT;
		bool m_lnaExt;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureHackRF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureHackRF* create(const Settings& settings)
		{
			return new MsgConfigureHackRF(settings);
		}

	private:
		Settings m_settings;

		MsgConfigureHackRF(const Settings& settings) :
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

	HackRFInput();
	virtual ~HackRFInput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;

	virtual bool handleMessage(const Message& message);

private:
	bool applySettings(const Settings& settings, bool force);
	hackrf_device *open_hackrf_from_sequence(int sequence);
	void setCenterFrequency(quint64 freq);

	QMutex m_mutex;
	Settings m_settings;
	struct hackrf_device* m_dev;
	HackRFThread* m_hackRFThread;
	QString m_deviceDescription;
};

#endif // INCLUDE_HACKRFINPUT_H
