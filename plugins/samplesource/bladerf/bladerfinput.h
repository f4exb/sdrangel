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

#ifndef INCLUDE_BLADERFINPUT_H
#define INCLUDE_BLADERFINPUT_H

#include "dsp/samplesource/samplesource.h"
#include <libbladeRF.h>
#include <QString>

class BladerfThread;

class BladerfInput : public SampleSource {
public:
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA
	} fcPos_t;

	struct Settings {
		qint32 m_lnaGain;
		qint32 m_vga1;
		qint32 m_vga2;
		qint32 m_samplerate;
		qint32 m_bandwidth;
		quint32 m_log2Decim;
		fcPos_t m_fcPos;
		bool m_xb200;
		bladerf_xb200_path m_xb200Path;
		bladerf_xb200_filter m_xb200Filter;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureBladerf : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureBladerf* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureBladerf(generalSettings, settings);
		}

	private:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureBladerf(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
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

	BladerfInput(MessageQueue* msgQueueToGUI);
	~BladerfInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;

	bool handleMessage(Message* message);

private:
	QMutex m_mutex;
	Settings m_settings;
	struct bladerf* m_dev;
	BladerfThread* m_bladerfThread;
	QString m_deviceDescription;

	bool applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force);
	bladerf_lna_gain getLnaGain(int lnaGain);
	struct bladerf *open_bladerf_from_serial(const char *serial);
};

#endif // INCLUDE_BLADERFINPUT_H
