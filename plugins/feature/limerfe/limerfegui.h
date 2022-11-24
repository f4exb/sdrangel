///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_LIMERFEGUI_H_
#define INCLUDE_FEATURE_LIMERFEGUI_H_

#include <QTimer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "settings/rollupstate.h"

#include "limerfesettings.h"

class PluginAPI;
class FeatureUISet;
class Feature;
class LimeRFE;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;

namespace Ui {
	class LimeRFEGUI;
}

class LimeRFEGUI : public FeatureGUI
{
    Q_OBJECT
public:
	static LimeRFEGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
	Ui::LimeRFEGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	LimeRFESettings m_settings;
    QList<QString> m_settingsKeys;
	RollupState m_rollupState;
    bool m_rxOn;
    bool m_txOn;
	bool m_doApplySettings;
    bool m_rxTxToggle;
    QTimer m_timer;
    double m_currentPowerCorrection;
    bool m_avgPower;
    MovingAverageUtil<double, double, 10> m_powerMovingAverage;
    bool m_deviceSetSync;
    std::vector<DSPDeviceSourceEngine*> m_sourceEngines;
    std::vector<int> m_rxDeviceSetIndex;
    std::vector<DSPDeviceSinkEngine*> m_sinkEngines;
    std::vector<int> m_txDeviceSetIndex;

	LimeRFE* m_limeRFE;
	MessageQueue m_inputMessageQueue;

	explicit LimeRFEGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~LimeRFEGUI();

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void applySettings(bool force = false);
	void displaySettings();
    void displayMode();
    void displayPower();
    void refreshPower();
    void setRxChannels();
    void setTxChannels();
    int getPowerCorectionIndex();
    double getPowerCorrection();
    void setPowerCorrection(double dbValue);
    void updateAbsPower(double powerCorrDB);
    void updateDeviceSetList();
    void stopStartRx(bool start);
    void stopStartTx(bool start);
    void syncRxTx();
    void highlightApplyButton(bool highlight);
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_openDevice_clicked();
    void on_closeDevice_clicked();
    void on_deviceToGUI_clicked();
    void on_rxChannelGroup_currentIndexChanged(int index);
    void on_rxChannel_currentIndexChanged(int index);
    void on_rxPort_currentIndexChanged(int index);
    void on_attenuation_currentIndexChanged(int index);
    void on_amFmNotchFilter_clicked();
    void on_txFollowsRx_clicked();
    void on_txChannelGroup_currentIndexChanged(int index);
    void on_txChannel_currentIndexChanged(int index);
    void on_txPort_currentIndexChanged(int index);
    void on_powerEnable_clicked();
    void on_powerSource_currentIndexChanged(int index);
    void on_powerRefresh_clicked();
    void on_powerAutoRefresh_toggled(bool checked);
    void on_powerAbsAvg_clicked();
    void on_powerCorrValue_textEdited(const QString &text);
    void on_modeRx_toggled(bool checked);
    void on_modeTx_toggled(bool checked);
    void on_rxTxToggle_clicked();
    void on_deviceSetRefresh_clicked();
    void on_deviceSetSync_clicked();
    void on_apply_clicked();
    void tick();
};

#endif
