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

#ifndef INCLUDE_FILESOURCEGUI_H
#define INCLUDE_FILESOURCEGUI_H

#include <QTimer>
#include "plugin/plugingui.h"

#include "filesourceinput.h"

class PluginAPI;

namespace Ui {
	class FileSourceGui;
}

class FileSourceGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit FileSourceGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~FileSourceGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	QByteArray serializeGeneral() const;
	bool deserializeGeneral(const QByteArray&data);
	qint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	bool handleMessage(Message* message);

private:
	Ui::FileSourceGui* ui;

	PluginAPI* m_pluginAPI;
	SampleSource::GeneralSettings m_generalSettings;
	FileSourceInput::Settings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;
    bool m_acquisition;
    QString m_fileName;
	int m_sampleRate;
	quint64 m_centerFrequency;
	std::time_t m_startingTimeStamp;
	int m_samplesCount;

	void displaySettings();
	void displayTime();
	void sendSettings();
	void updateHardware();
	void configureFileName();
	void updateWithAcquisition();
	void updateWithStreamData();
	void updateWithStreamTime();

private slots:
	void on_playLoop_toggled(bool checked);
	void on_play_toggled(bool checked);
	void on_showFileDialog_clicked(bool checked);
};

#endif // INCLUDE_FILESOURCEGUI_H
