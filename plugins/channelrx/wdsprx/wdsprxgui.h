///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>   //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_WDSPRXGUI_H
#define INCLUDE_WDSPRXGUI_H

#include <QIcon>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "gui/fftnrdialog.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "wdsprxsettings.h"

class PluginAPI;
class DeviceUISet;

class AudioFifo;
class WDSPRx;
class WDSPRxAGCDialog;
class WDSPRxDNBDialog;
class WDSPRxDNRDialog;
class WDSPRxAMDialog;
class WDSPRxFMDialog;
class WDSPRxCWPeakDialog;
class WDSPRxSquelchDialog;
class WDSPRxEqDialog;
class WDSPRxPanDialog;
class SpectrumVis;
class BasebandSampleSink;
class CRightClickEnabler;

namespace Ui {
	class WDSPRxGUI;
}

class WDSPRxGUI : public ChannelGUI {
	Q_OBJECT

public:
	static WDSPRxGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);

	void resetToDefaults() final;
	QByteArray serialize() const final;
	bool deserialize(const QByteArray& data) final;
	MessageQueue *getInputMessageQueue() final { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) final { m_settings.m_workspaceIndex = index; };
    int getWorkspaceIndex() const final { return m_settings.m_workspaceIndex; };
    void setGeometryBytes(const QByteArray& blob) final { m_settings.m_geometryBytes = blob; };
    QByteArray getGeometryBytes() const final { return m_settings.m_geometryBytes; };
    QString getTitle() const final { return m_settings.m_title; };
    QColor getTitleColor() const final  { return m_settings.m_rgbColor; };
    void zetHidden(bool hidden) final { m_settings.m_hidden = hidden; }
    bool getHidden() const final { return m_settings.m_hidden; }
    ChannelMarker& getChannelMarker() final { return m_channelMarker; }
    int getStreamIndex() const final { return m_settings.m_streamIndex; }
    void setStreamIndex(int streamIndex) final { m_settings.m_streamIndex = streamIndex; }

public slots:
	void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
	Ui::WDSPRxGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	WDSPRxSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_doApplySettings;
    int m_spectrumRate;
	bool m_audioBinaural;
	bool m_audioFlipChannels;
	bool m_audioMute;
	bool m_squelchOpen;
    int m_audioSampleRate;
	uint32_t m_tickCount;

	WDSPRx* m_wdspRx;
	SpectrumVis* m_spectrumVis;
	MessageQueue m_inputMessageQueue;
    WDSPRxAGCDialog* m_agcDialog;
    WDSPRxDNBDialog* m_dnbDialog;
    WDSPRxDNRDialog* m_dnrDialog;
    WDSPRxAMDialog* m_amDialog;
    WDSPRxFMDialog* m_fmDialog;
    WDSPRxCWPeakDialog* m_cwPeakDialog;
    WDSPRxSquelchDialog* m_squelchDialog;
    WDSPRxEqDialog* m_equalizerDialog;
    WDSPRxPanDialog* m_panDialog;

    CRightClickEnabler *m_audioMuteRightClickEnabler;
    CRightClickEnabler *m_agcRightClickEnabler;
    CRightClickEnabler *m_dnbRightClickEnabler;
    CRightClickEnabler *m_dnrRightClickEnabler;
    CRightClickEnabler *m_cwPeakRightClickEnabler;
    CRightClickEnabler *m_squelchRightClickEnabler;
    CRightClickEnabler *m_equalizerRightClickEnabler;
    CRightClickEnabler *m_panRightClickEnabler;
    CRightClickEnabler *m_demodRightClickEnabler;

	QIcon m_iconDSBUSB;
	QIcon m_iconDSBLSB;

	explicit WDSPRxGUI(PluginAPI* pluginAPI, DeviceUISet* deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
	~WDSPRxGUI() final;

    bool blockApplySettings(bool block);
	void applySettings(bool force = false);
	void applyBandwidths(unsigned int spanLog2, bool force = false);
    unsigned int spanLog2Max() const;
	void displaySettings();
	bool handleMessage(const Message& message);
    void makeUIConnections() const;
    void updateAbsoluteCenterFrequency();
	uint32_t getValidAudioSampleRate() const;

	void leaveEvent(QEvent*) final;
	void enterEvent(EnterEventType*) final;

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_audioBinaural_toggled(bool binaural);
	void on_audioFlipChannels_toggled(bool flip);
	void on_dsb_toggled(bool dsb);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_agc_toggled(bool checked);
    void on_dnr_toggled(bool checked);
    void on_dnb_toggled(bool checked);
    void on_anf_toggled(bool checked);
	void on_agcGain_valueChanged(int value);
	void on_audioMute_toggled(bool checked);
	void on_spanLog2_valueChanged(int value);
	void on_flipSidebands_clicked(bool checked);
    void on_fftWindow_currentIndexChanged(int index);
    void on_profileIndex_valueChanged(int value);
    void on_demod_currentIndexChanged(int index);
    void on_cwPeaking_toggled(bool checked);
    void on_squelch_toggled(bool checked);
    void on_squelchThreshold_valueChanged(int value);
    void on_equalizer_toggled(bool checked);
    void on_rit_toggled(bool checked);
    void on_ritFrequency_valueChanged(int value);
    void on_dbOrS_toggled(bool checked);
	void onWidgetRolled(const QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect(const QPoint& p);
    void agcSetupDialog(const QPoint& p);
    void agcSetup(int valueChanged);
    void dnbSetupDialog(const QPoint& p);
    void dnbSetup(int valueChanged);
    void dnrSetupDialog(const QPoint& p);
    void dnrSetup(int valueChanged);
    void cwPeakSetupDialog(const QPoint& p);
    void cwPeakSetup(int valueChanged);
    void demodSetupDialog(const QPoint& p);
    void amSetup(int valueChanged);
    void fmSetup(int valueChanged);
    void squelchSetupDialog(const QPoint& p);
    void squelchSetup(int valueChanged);
    void equalizerSetupDialog(const QPoint& p);
    void equalizerSetup(int valueChanged);
    void panSetupDialog(const QPoint& p);
    void panSetup(int valueChanged);
	void tick();
};

#endif // INCLUDE_WDSPRXGUI_H
