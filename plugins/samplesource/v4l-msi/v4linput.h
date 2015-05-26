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

#ifndef INCLUDE_V4LINPUT_H
#define INCLUDE_V4LINPUT_H

#include "dsp/samplesource/samplesource.h"
#include <QString>

struct v4l_buffer {
	void *start;
	size_t length;
};

class V4LThread;

class V4LInput : public SampleSource {
public:
	struct Settings {
		qint32 m_gain, m_lna, m_mix;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureV4L : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureV4L* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureV4L(generalSettings, settings);
		}

	private:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureV4L(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
		{ }
	};

	V4LInput(MessageQueue* msgQueueToGUI);
	~V4LInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;
	bool handleMessage(Message* message);

private:
	QMutex m_mutex;
	Settings m_settings;
	V4LThread* m_V4LThread;
	QString m_deviceDescription;

	void applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force);
};

#endif // INCLUDE_V4L_H
