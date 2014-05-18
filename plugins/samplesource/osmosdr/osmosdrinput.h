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

#ifndef INCLUDE_OSMOSDRINPUT_H
#define INCLUDE_OSMOSDRINPUT_H

#include "dsp/samplesource/samplesource.h"
#include <osmosdr.h>
#include <QString>

class OsmoSDRThread;

class OsmoSDRInput : public SampleSource {
public:
	struct Settings {
		bool m_swapIQ;
		qint32 m_decimation;
		qint32 m_lnaGain;
		qint32 m_mixerGain;
		qint32 m_mixerEnhancement;
		qint32 m_if1gain;
		qint32 m_if2gain;
		qint32 m_if3gain;
		qint32 m_if4gain;
		qint32 m_if5gain;
		qint32 m_if6gain;
		qint32 m_opAmpI1;
		qint32 m_opAmpI2;
		qint32 m_opAmpQ1;
		qint32 m_opAmpQ2;
		qint32 m_dcOfsI;
		qint32 m_dcOfsQ;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureOsmoSDR : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureOsmoSDR* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureOsmoSDR(generalSettings, settings);
		}

	protected:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureOsmoSDR(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
		{ }
	};

	OsmoSDRInput(MessageQueue* msgQueueToGUI);
	~OsmoSDRInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;

	bool handleMessage(Message* message);

private:
	QMutex m_mutex;
	Settings m_settings;
	osmosdr_dev_t* m_dev;
	OsmoSDRThread* m_osmoSDRThread;
	QString m_deviceDescription;

	bool applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force);
};

#endif // INCLUDE_OSMOSDRINPUT_H
