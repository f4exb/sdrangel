///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_FEATURE_MORSEDECODERGUI_H_
#define INCLUDE_FEATURE_MORSEDECODERGUI_H_

#include <QTimer>
#include <QList>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "availablechannelorfeaturehandler.h"
#include "settings/rollupstate.h"

#include "morsedecodersettings.h"

class PluginAPI;
class FeatureUISet;
class MorseDecoder;
class Feature;
class ScopeVis;

namespace Ui {
	class MorseDecoderGUI;
}

class MorseDecoderGUI : public FeatureGUI {
	Q_OBJECT
public:
	static MorseDecoderGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

private:
	Ui::MorseDecoderGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	MorseDecoderSettings m_settings;
    QList<QString> m_settingsKeys;
	RollupState m_rollupState;
	int m_sampleRate;
	bool m_doApplySettings;

	MorseDecoder* m_morseDecoder;
    ScopeVis* m_scopeVis;
	MessageQueue m_inputMessageQueue;
	QTimer m_statusTimer;
	int m_lastFeatureState;
	AvailableChannelOrFeatureList m_availableChannels;
	ChannelAPI *m_selectedChannel;

	explicit MorseDecoderGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~MorseDecoderGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	void displaySampleRate(int sampleRate);
	void updateChannelList();
	bool handleMessage(const Message& message);
    void textReceived(const QString& text);
    void updateMorseStats(float estPitch, float estWPM, float cost);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_channels_currentIndexChanged(int index);
	void on_channelApply_clicked();
    void on_statLock_toggled(bool checked);
    void on_showThreshold_clicked(bool checked);
    void on_clearTable_clicked();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
	void updateStatus();
	void tick();

};

#endif // INCLUDE_FEATURE_MORSEDECODERGUI_H_
