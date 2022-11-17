///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

  	virtual void destroy();
    virtual void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue* getInputMessageQueue();
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return -1; }
    virtual void setStreamIndex(int streamIndex) { (void) streamIndex; }

private:
	Ui::InterferometerGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    InterferometerSettings m_settings;
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
	virtual ~InterferometerGUI();

	void blockApplySettings(bool block);
	void applySettings(bool force = false);
    void applyDecimation();
    void applyPosition();
	void displaySettings();
    void displayRateAndShift();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
    void on_decimationFactor_currentIndexChanged(int index);
    void on_position_valueChanged(int value);
    void on_phaseCorrection_valueChanged(int value);
    void on_correlationType_currentIndexChanged(int index);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
	void tick();
};

#endif // INCLUDE_INTERFEROMETERGUI_H
