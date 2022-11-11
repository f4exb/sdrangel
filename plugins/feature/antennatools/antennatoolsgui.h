///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_ANTENNATOOLSGUI_H_
#define INCLUDE_FEATURE_ANTENNATOOLSGUI_H_

#include <QTimer>
#include <QAbstractListModel>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "antennatoolssettings.h"

class PluginAPI;
class FeatureUISet;
class AntennaTools;

namespace Ui {
    class AntennaToolsGUI;
}

class AntennaToolsGUI : public FeatureGUI {
    Q_OBJECT
public:
    static AntennaToolsGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
    Ui::AntennaToolsGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    AntennaToolsSettings m_settings;
    RollupState m_rollupState;
    bool m_doApplySettings;

    AntennaTools* m_antennatools;
    MessageQueue m_inputMessageQueue;
    QTimer m_statusTimer;
    unsigned int m_deviceSets;

    explicit AntennaToolsGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~AntennaToolsGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

    void calcDipoleLength();
    double calcDipoleFrequency(double totalLength);
    void calcDishFocalLength();
    void calcDishBeamwidth();
    void calcDishGain();
    void calcDishEffectiveArea();
    double dishLambda() const;
    double dishLengthMetres(double length) const;
    double dishMetresToLength(double m) const;
    double dishDiameterMetres() const;
    double dishDepthMetres() const;
    double dishSurfaceErrorMetres() const;
    double getDeviceSetFrequencyMHz(int index);

private slots:
    void on_dipoleFrequency_valueChanged(double value);
    void on_dipoleFrequencySelect_currentIndexChanged(int index);
    void on_dipoleEndEffectFactor_valueChanged(double value);
    void on_dipoleLengthUnits_currentIndexChanged(int index);
    void on_dipoleLength_valueChanged(double value);
    void on_dipoleElementLength_valueChanged(double value);
    void on_dishFrequency_valueChanged(double value);
    void on_dishFrequencySelect_currentIndexChanged(int index);
    void on_dishDiameter_valueChanged(double value);
    void on_dishLengthUnits_currentIndexChanged(int index);
    void on_dishDepth_valueChanged(double value);
    void on_dishEfficiency_valueChanged(int value);
    void on_dishSurfaceError_valueChanged(double value);
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void updateStatus();
};

#endif // INCLUDE_FEATURE_ANTENNATOOLSGUI_H_
