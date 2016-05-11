#ifndef INCLUDE_FCDGUI_H
#define INCLUDE_FCDGUI_H

#include <QTimer>

#include "fcdproplusinput.h"
#include "plugin/plugingui.h"

class PluginAPI;

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

namespace Ui {
	class FCDProPlusGui;
}

class FCDProPlusGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit FCDProPlusGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~FCDProPlusGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);

private:
	Ui::FCDProPlusGui* ui;

	PluginAPI* m_pluginAPI;
	FCDProPlusSettings m_settings;
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;
	int m_lastEngineState;

	void displaySettings();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_checkBoxG_stateChanged(int state);
	void on_checkBoxB_stateChanged(int state);
	void on_mixGain_stateChanged(int state);
	void on_ifGain_valueChanged(int value);
	void on_filterRF_currentIndexChanged(int index);
	void on_filterIF_currentIndexChanged(int index);
	void on_ppm_valueChanged(int value);
	void on_startStop_toggled(bool checked);
	void updateHardware();
	void updateStatus();
};

#endif // INCLUDE_FCDGUI_H
