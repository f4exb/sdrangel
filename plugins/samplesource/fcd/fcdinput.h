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

#ifndef INCLUDE_FCDINPUT_H
#define INCLUDE_FCDINPUT_H

#include "dsp/samplesource/samplesource.h"
#include <QString>

struct fcd_buffer {
	void *start;
	size_t length;
};

class FCDThread;

class FCDInput : public SampleSource {
public:
	struct Settings {
		Settings();
		qint32 range;
		qint32 gain;
		qint32 bias;
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureFCD : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureFCD* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureFCD(generalSettings, settings);
		}

	private:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureFCD(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
		{ }
	};


	FCDInput(MessageQueue* msgQueueToGUI);
	~FCDInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;
	bool handleMessage(Message* message);

private:
	QMutex m_mutex;
	Settings m_settings;
	FCDThread* m_FCDThread;
	QString m_deviceDescription;
	bool applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force);
};

#endif // INCLUDE_FCD_H
