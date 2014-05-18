///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2013 by Dimitri Stolnikov <horiz0n@gmx.net>                     //
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

#ifndef INCLUDE_GNURADIOGUI_H
#define INCLUDE_GNURADIOGUI_H

#include <QTimer>
#include <QPair>
#include <QList>
#include <QString>
#include <QSlider>
#include <QLabel>
#include "plugin/plugingui.h"
#include "gnuradioinput.h"

namespace Ui {
class GNURadioGui;
}

class PluginAPI;

class GNURadioGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit GNURadioGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~GNURadioGui();
	void destroy();

	void setName(const QString& name);

	void resetToDefaults();
	QByteArray serializeGeneral() const;
	bool deserializeGeneral(const QByteArray&data);
	quint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	bool handleMessage(Message* message);

private:
	Ui::GNURadioGui* ui;
	PluginAPI* m_pluginAPI;
	SampleSource* m_sampleSource;
	QList< QPair<QString, QString> > m_devs;
	std::vector< std::pair< QString, std::vector<double> > > m_namedGains;
	double m_freqMin;
	double m_freqMax;
	double m_freqCorr;
	std::vector<double> m_sampRates;
	std::vector<QString> m_antennas;
	std::vector<QString> m_dcoffs;
	std::vector<QString> m_iqbals;
	std::vector<double> m_bandwidths;

	std::vector< QSlider* > m_gainSliders;
	std::vector< QLabel* > m_gainLabels;

	QList< QPair< QSlider*, QLabel* > > m_gainControls;

	SampleSource::GeneralSettings m_generalSettings;
	GNURadioInput::Settings m_settings;
	QTimer m_updateTimer;

	void displaySettings();
	void sendSettings();

private slots:
	void updateHardware();

	void on_cboDevices_currentIndexChanged(int index);
	void on_txtDeviceArgs_textChanged(const QString &arg1);
	void on_centerFrequency_changed(quint64 value);
	void on_sldFreqCorr_valueChanged(int value);

	void on_sldGain_valueChanged(int value);

	void on_cboSampleRate_currentIndexChanged(int index);
	void on_cboAntennas_currentIndexChanged(const QString &arg1);
	void on_cboDCOffset_currentIndexChanged(const QString &arg1);
	void on_cboIQBalance_currentIndexChanged(const QString &arg1);
	void on_cboBandwidth_currentIndexChanged(int index);
};

#endif // INCLUDE_GNURADIOGUI_H
