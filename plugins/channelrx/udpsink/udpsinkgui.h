///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_UDPSRCGUI_H
#define INCLUDE_UDPSRCGUI_H

#include <QHostAddress>
#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "udpsink.h"
#include "udpsinksettings.h"

class PluginAPI;
class DeviceUISet;
class UDPSink;
class SpectrumVis;

namespace Ui {
	class UDPSinkGUI;
}

class UDPSinkGUI : public ChannelGUI {
	Q_OBJECT

public:
	static UDPSinkGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
	void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
	Ui::UDPSinkGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	UDPSink* m_udpSink;
	UDPSinkSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	MovingAverage<double> m_channelPowerAvg;
    MovingAverage<double> m_inPowerAvg;
	uint32_t m_tickCount;

	// settings
	bool m_doApplySettings;
	bool m_rfBandwidthChanged;
	MessageQueue m_inputMessageQueue;

	// RF path
	SpectrumVis* m_spectrumVis;

	explicit UDPSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~UDPSinkGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void applySettingsImmediate(bool force = false);
	void displaySettings();
	void setSampleFormat(int index);
	void setSampleFormatIndex(const UDPSinkSettings::SampleFormat& sampleFormat);
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
	void on_deltaFrequency_changed(qint64 value);
	void on_sampleFormat_currentIndexChanged(int index);
	void on_outputUDPAddress_editingFinished();
	void on_outputUDPPort_editingFinished();
	void on_inputUDPAudioPort_editingFinished();
	void on_sampleRate_textEdited(const QString& arg1);
	void on_rfBandwidth_textEdited(const QString& arg1);
	void on_fmDeviation_textEdited(const QString& arg1);
	void on_audioActive_toggled(bool active);
	void on_audioStereo_toggled(bool stereo);
	void on_applyBtn_clicked();
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDialogCalled(const QPoint& p);
	void on_gain_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
    void on_squelchGate_valueChanged(int value);
	void on_agc_toggled(bool agc);
	void tick();
};

#endif // INCLUDE_UDPSRCGUI_H
