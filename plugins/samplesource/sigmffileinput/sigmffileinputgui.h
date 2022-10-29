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

#ifndef INCLUDE_SIGMFFILEINPUTGUI_H
#define INCLUDE_SIGMFFILEINPUTGUI_H

#include <QTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "sigmffileinputsettings.h"
#include "sigmffiledata.h"
#include "sigmffileinput.h"

class DeviceUISet;

namespace Ui {
	class SigMFFileInputGUI;
}

class SigMFFileInputGUI : public DeviceGUI {
	Q_OBJECT

public:
	explicit SigMFFileInputGUI(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~SigMFFileInputGUI();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::SigMFFileInputGUI* ui;

	SigMFFileInputSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_forceSettings;
    int m_currentTrackIndex;
	bool m_doApplySettings;
	QTimer m_statusTimer;
	std::vector<int> m_gains;
	DeviceSampleSource* m_sampleSource;
    bool m_startStop;
    bool m_trackMode;
    QString m_metaFileName;
    QString m_recordInfo;
	int m_sampleRate;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
    quint64 m_recordLength;
    quint64 m_startingTimeStamp;
    quint64 m_samplesCount;
    quint64 m_trackSamplesCount;
    quint64 m_trackTimeStart;
    int m_trackNumber;
	std::size_t m_tickCount;
	bool m_enableTrackNavTime;
	bool m_enableFullNavTime;
    int m_deviceSampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;
    SigMFFileMetaInfo m_metaInfo;
    QList<SigMFFileCapture> m_captures;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void displayTime();
    QString displayScaled(uint64_t value, int precision);
    void addCaptures(const QList<SigMFFileCapture>& captures);
	void sendSettings();
    void updateSampleRateAndFrequency();
	void configureFileName();
    void updateStartStop();
	void updateWithStreamData();
	void updateWithStreamTime();
    void setAccelerationCombo();
    void setNumberStr(int n, QString& s);
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
    void on_infoDetails_clicked();
    void on_captureTable_itemSelectionChanged();
    void on_trackNavTimeSlider_valueChanged(int value);
	void on_playTrack_toggled(bool checked);
	void on_playTrackLoop_toggled(bool checked);
    void on_fullNavTimeSlider_valueChanged(int value);
	void on_playFull_toggled(bool checked);
	void on_playFullLoop_toggled(bool checked);
	void on_showFileDialog_clicked(bool checked);
	void on_acceleration_currentIndexChanged(int index);
    void updateStatus();
	void tick();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_SIGMFFILEINPUTGUI_H
