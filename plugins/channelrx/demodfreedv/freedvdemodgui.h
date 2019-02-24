///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FREEDVDEMODGUI_H
#define INCLUDE_FREEDVDEMODGUI_H

#include <QIcon>

#include "plugin/plugininstancegui.h"
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "freedvdemodsettings.h"

class PluginAPI;
class DeviceUISet;

class AudioFifo;
class FreeDVDemod;
class SpectrumVis;
class BasebandSampleSink;

namespace Ui {
	class FreeDVDemodGUI;
}

class FreeDVDemodGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static FreeDVDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	Ui::FreeDVDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	FreeDVDemodSettings m_settings;
	bool m_doApplySettings;
    int m_spectrumRate;
	bool m_audioBinaural;
	bool m_audioFlipChannels;
	bool m_audioMute;
	bool m_squelchOpen;
	uint32_t m_tickCount;

	FreeDVDemod* m_freeDVDemod;
	SpectrumVis* m_spectrumVis;
	MessageQueue m_inputMessageQueue;

	QIcon m_iconDSBUSB;
	QIcon m_iconDSBLSB;

	explicit FreeDVDemodGUI(PluginAPI* pluginAPI, DeviceUISet* deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~FreeDVDemodGUI();

    bool blockApplySettings(bool block);
	void applySettings(bool force = false);
	void applyBandwidths(int spanLog2, bool force = false);
	void displaySettings();

	void displayAGCPowerThreshold(int value);

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_audioBinaural_toggled(bool binaural);
	void on_audioFlipChannels_toggled(bool flip);
	void on_dsb_toggled(bool dsb);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_agc_toggled(bool checked);
    void on_agcClamping_toggled(bool checked);
	void on_agcTimeLog2_valueChanged(int value);
    void on_agcPowerThreshold_valueChanged(int value);
    void on_agcThresholdGate_valueChanged(int value);
	void on_audioMute_toggled(bool checked);
	void on_spanLog2_valueChanged(int value);
	void on_flipSidebands_clicked(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect();
	void tick();
};

#endif // INCLUDE_FREEDVDEMODGUI_H
