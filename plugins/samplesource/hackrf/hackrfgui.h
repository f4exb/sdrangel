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

#ifndef INCLUDE_HACKRFGUI_H
#define INCLUDE_HACKRFGUI_H

#include <QTimer>

#include "hackrfinput.h"
#include "plugin/plugingui.h"

#define HACKRF_MAX_DEVICE (32)

class PluginAPI;

namespace Ui {
	class HackRFGui;
}

class HackRFGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	typedef enum
	{
		HACKRF_IMGREJ_BYPASS = 0,
		HACKRF_IMGREJ_LOWPASS,
		HACKRF_IMGREJ_HIGHPASS,
		HACKRF_IMGREJ_NB
	} HackRFImgRejValue;

	explicit HackRFGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~HackRFGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	qint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual bool handleMessage(const Message& message);

private:
	Ui::HackRFGui* ui;

	PluginAPI* m_pluginAPI;
	HackRFInput::Settings m_settings;
	QTimer m_updateTimer;
	SampleSource* m_sampleSource;

	void displaySettings();
	void displaySampleRates();
	void displayBandwidths();
	void displayImgRejFilters();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_LOppm_valueChanged(int value);
	void on_sampleRate_currentIndexChanged(int index);
	void on_biasT_stateChanged(int state);
	void on_decim_valueChanged(int value);
	void on_fcPos_currentIndexChanged(int index);
	void on_lna_valueChanged(int value);
	void on_vga_valueChanged(int value);
	void updateHardware();
	void handleSourceMessages();
};

class HackRFSampleRates {
public:
	static unsigned int getRate(unsigned int rate_index);
	static unsigned int getRateIndex(unsigned int rate);
	static const unsigned int m_nb_rates = 7;
	static unsigned int m_rates[m_nb_rates];
};

class HackRFBandwidths {
public:
	static unsigned int getBandwidth(unsigned int bandwidth_index);
	static unsigned int getBandwidthIndex(unsigned int bandwidth);
	static const unsigned int m_nb_bw = 16;
	static unsigned int m_bw[m_nb_bw];
};

class HackRFImageRejectFilters {
public:
	static QString& getImageRejectFilterName(unsigned int filter_index);
	static const unsigned int m_nb_rej = HackRFGui::HACKRF_IMGREJ_NB;
	static QString m_rejName[m_nb_rej];
};

#endif // INCLUDE_HACKRFGUI_H
