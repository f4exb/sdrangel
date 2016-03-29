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

#ifndef INCLUDE_AIRSPYGUI_H
#define INCLUDE_AIRSPYGUI_H

#include <QTimer>

#include "airspyinput.h"
#include "plugin/plugingui.h"

#define AIRSPY_MAX_DEVICE (32)

class PluginAPI;

namespace Ui {
	class AirspyGui;
	class AirspySampleRates;
}

class AirspyGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit AirspyGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~AirspyGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual bool handleMessage(const Message& message);
	uint32_t getDevSampleRate(unsigned int index);
	int getDevSampleRateIndex(uint32_t sampleRate);

private:
	Ui::AirspyGui* ui;

	PluginAPI* m_pluginAPI;
	AirspySettings m_settings;
	QTimer m_updateTimer;
	std::vector<uint32_t> m_rates;
	SampleSource* m_sampleSource;

	void displaySettings();
	void displaySampleRates();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_LOppm_valueChanged(int value);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_sampleRate_currentIndexChanged(int index);
	void on_biasT_stateChanged(int state);
	void on_decim_currentIndexChanged(int index);
	void on_fcPos_currentIndexChanged(int index);
	void on_lna_valueChanged(int value);
	void on_mix_valueChanged(int value);
	void on_vga_valueChanged(int value);
	void on_lnaAGC_stateChanged(int state);
	void on_mixAGC_stateChanged(int state);
	void updateHardware();
	void handleSourceMessages();
};

#endif // INCLUDE_AIRSPYGUI_H
