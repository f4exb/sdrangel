///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_UDPSRCGUI_H
#define INCLUDE_UDPSRCGUI_H

#include <plugin/plugininstancegui.h>
#include <QHostAddress>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"

#include "udpsrc.h"
#include "udpsrcsettings.h"

class PluginAPI;
class DeviceUISet;
class UDPSrc;
class SpectrumVis;

namespace Ui {
	class UDPSrcGUI;
}

class UDPSrcGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static UDPSrcGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
	virtual void destroy();

	void setName(const QString& name);
	QString getName() const;
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
	virtual bool handleMessage(const Message& message);

public slots:
	void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
	Ui::UDPSrcGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	UDPSrc* m_udpSrc;
	UDPSrcSettings m_settings;
	ChannelMarker m_channelMarker;
	MovingAverage<double> m_channelPowerAvg;
    MovingAverage<double> m_inPowerAvg;
	uint32_t m_tickCount;

	// settings
	bool m_doApplySettings;
	bool m_rfBandwidthChanged;
	MessageQueue m_inputMessageQueue;

	// RF path
	SpectrumVis* m_spectrumVis;

	explicit UDPSrcGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~UDPSrcGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void applySettingsImmediate(bool force = false);
	void displaySettings();
	void displayUDPAddress();
	void setSampleFormat(int index);
	void setSampleFormatIndex(const UDPSrcSettings::SampleFormat& sampleFormat);

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_sampleFormat_currentIndexChanged(int index);
    void on_sampleSize_currentIndexChanged(int index);
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
