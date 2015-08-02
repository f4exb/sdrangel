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

#ifndef INCLUDE_FILESOURCEINPUT_H
#define INCLUDE_FILESOURCEINPUT_H

#include "dsp/samplesource/samplesource.h"
#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <fstream>

class FileSourceThread;

class FileSourceInput : public SampleSource {
public:
	struct Settings {
		QString m_fileName;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureFileSource : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureFileSource* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureFileSource(generalSettings, settings);
		}

	private:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureFileSource(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
		{ }
	};

	class MsgReportFileSource : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgReportFileSource* create()
		{
			return new MsgReportFileSource();
		}

	protected:

		MsgReportFileSource() :
			Message()
		{ }
	};

	FileSourceInput(MessageQueue* msgQueueToGUI, const QTimer& masterTimer);
	~FileSourceInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;
	std::time_t getStartingTimeStamp() const;

	bool handleMessage(Message* message);

private:
	QMutex m_mutex;
	Settings m_settings;
	std::ifstream m_ifstream;
	FileSourceThread* m_fileSourceThread;
	QString m_deviceDescription;
	int m_sampleRate;
	quint64 m_centerFrequency;
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;

	bool applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force);
	void openFileStream();
};

#endif // INCLUDE_FILESOURCEINPUT_H
