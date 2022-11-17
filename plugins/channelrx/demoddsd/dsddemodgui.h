///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef INCLUDE_DSDDEMODGUI_H
#define INCLUDE_DSDDEMODGUI_H

#include <QMenu>

#include "channel/channelgui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "dsddemodsettings.h"
#include "dsdstatustextdialog.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVisXY;
class DSDDemod;

namespace Ui {
	class DSDDemodGUI;
}

class DSDDemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static DSDDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
//	typedef enum
//	{
//	    signalFormatNone,
//	    signalFormatDMR,
//	    signalFormatDStar,
//	    signalFormatDPMR,
//		signalFormatYSF
//	} SignalFormat;

	Ui::DSDDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	DSDDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_doApplySettings;
    QList<DSDDemodSettings::AvailableAMBEFeature> m_availableAMBEFeatures;

    ScopeVisXY* m_scopeVisXY;

	DSDDemod* m_dsdDemod;
	bool m_enableCosineFiltering;
	bool m_syncOrConstellation;
	bool m_slot1On;
    bool m_slot2On;
    bool m_tdmaStereo;
    bool m_audioMute;
	bool m_squelchOpen;
    int  m_audioSampleRate;
	uint32_t m_tickCount;

	float m_myLatitude;
	float m_myLongitude;

	MessageQueue m_inputMessageQueue;

	DSDStatusTextDialog m_dsdStatusTextDialog;

	explicit DSDDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~DSDDemodGUI();

	void blockApplySettings(bool block);
	void applySettings(bool force = false);
    void displaySettings();
    void updateAMBEFeaturesList();
	void updateMyPosition();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_demodGain_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_baudRate_currentIndexChanged(int index);
    void on_enableCosineFiltering_toggled(bool enable);
    void on_syncOrConstellation_toggled(bool checked);
	void on_traceLength_valueChanged(int value);
    void on_traceStroke_valueChanged(int value);
    void on_traceDecay_valueChanged(int value);
    void on_slot1On_toggled(bool checked);
    void on_slot2On_toggled(bool checked);
    void on_tdmaStereoSplit_toggled(bool checked);
    void on_fmDeviation_valueChanged(int value);
    void on_squelchGate_valueChanged(int value);
    void on_squelch_valueChanged(int value);
    void on_highPassFilter_toggled(bool checked);
    void on_audioMute_toggled(bool checked);
    void on_symbolPLLLock_toggled(bool checked);
    void on_ambeSupport_clicked(bool checked);
    void on_ambeFeatures_currentIndexChanged(int index);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void on_viewStatusLog_clicked();
    void handleInputMessages();
    void audioSelect();
    void tick();
};

#endif // INCLUDE_DSDDEMODGUI_H
