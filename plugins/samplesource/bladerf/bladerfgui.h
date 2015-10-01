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

#ifndef INCLUDE_BLADERFGUI_H
#define INCLUDE_BLADERFGUI_H

#include <QTimer>
#include "plugin/plugingui.h"

#include "bladerfinput.h"

class PluginAPI;

namespace Ui {
	class BladerfGui;
	class BladerfSampleRates;
}

class BladerfGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit BladerfGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~BladerfGui();
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
	Ui::BladerfGui* ui;

	PluginAPI* m_pluginAPI;
	BladeRFSettings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;

	void displaySettings();
	void sendSettings();
	unsigned int getXb200Index(bool xb_200, bladerf_xb200_path xb200Path, bladerf_xb200_filter xb200Filter);

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_samplerate_valueChanged(int value);
	void on_bandwidth_valueChanged(int value);
	void on_decim_valueChanged(int value);
	void on_lna_valueChanged(int value);
	void on_vga1_valueChanged(int value);
	void on_vga2_valueChanged(int value);
	void on_xb200_currentIndexChanged(int index);
	void on_fcPos_currentIndexChanged(int index);
	void updateHardware();
};

class BladerfSampleRates {
public:
	static unsigned int getRate(unsigned int rate_index);
	static unsigned int getRateIndex(unsigned int rate);
private:
	static unsigned int m_rates[14];
	static unsigned int m_nb_rates;
};

class BladerfBandwidths {
public:
	static unsigned int getBandwidth(unsigned int bandwidth_index);
	static unsigned int getBandwidthIndex(unsigned int bandwidth);
private:
	static unsigned int m_halfbw[16];
	static unsigned int m_nb_halfbw;
};

#endif // INCLUDE_BLADERFGUI_H
