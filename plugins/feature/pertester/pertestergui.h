///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_PERTESTERGUI_H_
#define INCLUDE_FEATURE_PERTESTERGUI_H_

#include <QTimer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "pertestersettings.h"

class PluginAPI;
class FeatureUISet;
class PERTester;

namespace Ui {
    class PERTesterGUI;
}

class PERTesterGUI : public FeatureGUI {
    Q_OBJECT
public:
    static PERTesterGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
    Ui::PERTesterGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    PERTesterSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;

    PERTester* m_perTester;
    MessageQueue m_inputMessageQueue;
    QTimer m_statusTimer;
    int m_lastFeatureState;

    explicit PERTesterGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~PERTesterGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_resetStats_clicked();
    void on_packetCount_valueChanged(int value);
    void on_start_currentIndexChanged(int index);
    void on_satellites_editingFinished();
    void on_interval_valueChanged(double value);
    void on_packet_textChanged();
    void on_leading_valueChanged(int value);
    void on_trailing_valueChanged(int value);
    void on_txUDPAddress_editingFinished();
    void on_txUDPPort_editingFinished();
    void on_rxUDPAddress_editingFinished();
    void on_rxUDPPort_editingFinished();
    void updateStatus();
};

#endif // INCLUDE_FEATURE_PERTESTERGUI_H_
