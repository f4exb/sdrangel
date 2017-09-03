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
#include <QWidget>

#include "airspyinput.h"
#include "plugin/plugingui.h"

#define AIRSPY_MAX_DEVICE (32)

class DeviceSourceAPI;
class FileRecord;

namespace Ui {
	class AirspyGui;
	class AirspySampleRates;
}

class AirspyGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit AirspyGui(DeviceSourceAPI *deviceAPI, QWidget* parent = NULL);
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

	DeviceSourceAPI* m_deviceAPI;
	AirspySettings m_settings;
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	std::vector<uint32_t> m_rates;
	DeviceSampleSource* m_sampleSource;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;

	void displaySettings();
	void displaySampleRates();
	void sendSettings();
    void updateSampleRateAndFrequency();

private slots:
    void handleDSPMessages();
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
	void on_startStop_toggled(bool checked);
    void on_record_toggled(bool checked);
	void updateHardware();
    void updateStatus();
	void handleSourceMessages();
};

#endif // INCLUDE_AIRSPYGUI_H
