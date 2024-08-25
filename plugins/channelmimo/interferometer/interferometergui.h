///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_INTERFEROMETERGUI_H
#define INCLUDE_INTERFEROMETERGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "interferometersettings.h"

class PluginAPI;
class DeviceUISet;
class MIMOChannel;
class Interferometer;
class SpectrumVis;
class ScopeVis;

namespace Ui {
	class InterferometerGUI;
}

class InterferometerGUI : public ChannelGUI {
	Q_OBJECT
public:
    static InterferometerGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *mimoChannel);

    void resetToDefaults() final;
    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;
    MessageQueue* getInputMessageQueue() final;
    void setWorkspaceIndex(int index) final { m_settings.m_workspaceIndex = index; };
    int getWorkspaceIndex() const final { return m_settings.m_workspaceIndex; };
    void setGeometryBytes(const QByteArray& blob) final { m_settings.m_geometryBytes = blob; };
    QByteArray getGeometryBytes() const final { return m_settings.m_geometryBytes; };
    QString getTitle() const final { return m_settings.m_title; };
    QColor getTitleColor() const final  { return m_settings.m_rgbColor; };
    void zetHidden(bool hidden) final { m_settings.m_hidden = hidden; }
    bool getHidden() const final { return m_settings.m_hidden; }
    ChannelMarker& getChannelMarker() final { return m_channelMarker; }
    int getStreamIndex() const final { return -1; }
    void setStreamIndex(int streamIndex) final { (void) streamIndex; }

private:
	Ui::InterferometerGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    InterferometerSettings m_settings;
    QList<QString> m_settingsKeys;
    int m_sampleRate;
    qint64 m_centerFrequency;
    double m_shiftFrequencyFactor; //!< Channel frequency shift factor
    bool m_doApplySettings;
    MovingAverageUtil<double, double, 40> m_channelPowerAvg;
    Interferometer *m_interferometer;
	SpectrumVis* m_spectrumVis;
	ScopeVis* m_scopeVis;
	MessageQueue m_inputMessageQueue;
    uint32_t m_tickCount;

	explicit InterferometerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *rxChannel, QWidget* parent = nullptr);
	~InterferometerGUI() final;

	void blockApplySettings(bool block);
	void applySettings(bool force = false);
    void applyDecimation();
    void applyPosition();
	void displaySettings();
    void displayRateAndShift();
    bool handleMessage(const Message& message);
    void makeUIConnections() const;
    void updateAbsoluteCenterFrequency();
    void updateDeviceSetList(const QList<int>& deviceSetIndexes);
    int getLocalDeviceIndexInCombo(int localDeviceIndex) const;

	void leaveEvent(QEvent*) final;
	void enterEvent(EnterEventType*) final;

private slots:
    void handleSourceMessages();
    void on_decimationFactor_currentIndexChanged(int index);
    void on_position_valueChanged(int value);
    void on_phaseCorrection_valueChanged(int value);
    void on_gain_valueChanged(int value);
    void on_phaseCorrectionLabel_clicked();
    void on_gainLabel_clicked();
    void on_correlationType_currentIndexChanged(int index);
    void on_localDevice_currentIndexChanged(int index);
    void on_localDevicePlay_toggled(bool checked);
    void onWidgetRolled(const QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
	void tick();
};

#endif // INCLUDE_INTERFEROMETERGUI_H
