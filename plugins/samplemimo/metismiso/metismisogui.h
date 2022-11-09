///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef _METISMISO_METISMISOGUI_H_
#define _METISMISO_METISMISOGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "metismisosettings.h"
#include "metismiso.h"

class DeviceUISet;

namespace Ui {
	class MetisMISOGui;
}

class MetisMISOGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit MetisMISOGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~MetisMISOGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::MetisMISOGui* ui;

	MetisMISOSettings m_settings;
    QList<QString> m_settingsKeys;
    int m_rxSampleRate;
    int m_txSampleRate;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
	bool m_doApplySettings;
    bool m_forceSettings;
	DeviceSampleMIMO* m_sampleMIMO;
	std::size_t m_tickCount;
    std::vector<int> m_deviceSampleRates;
    std::vector<quint64> m_deviceCenterFrequencies; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;
    static const int m_absMaxFreq = 500000; // kHz

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
    void displayFrequency();
    void displaySampleRate();
    void updateSubsamplingIndex();
    void updateSpectrum();
	void sendSettings();
	void setCenterFrequency(qint64 centerFrequency);
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_streamIndex_currentIndexChanged(int index);
    void on_spectrumSource_currentIndexChanged(int index);
    void on_streamLock_toggled(bool checked);
    void on_LOppm_valueChanged(int value);
	void on_startStop_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_samplerateIndex_currentIndexChanged(int index);
    void on_log2Decim_currentIndexChanged(int index);
    void on_subsamplingIndex_currentIndexChanged(int index);
    void on_dcBlock_toggled(bool checked);
    void on_iqCorrection_toggled(bool checked);
    void on_transverter_clicked();
    void on_preamp_toggled(bool checked);
    void on_random_toggled(bool checked);
    void on_dither_toggled(bool checked);
    void on_duplex_toggled(bool checked);
    void on_nbRxIndex_currentIndexChanged(int index);
    void on_txEnable_toggled(bool checked);
    void on_txDrive_valueChanged(int value);
    void openDeviceSettingsDialog(const QPoint& p);
    void updateStatus();
    void updateHardware();
};

#endif // _METISMISO_METISMISOGUI_H_
