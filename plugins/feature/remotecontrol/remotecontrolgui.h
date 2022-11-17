///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_REMOTECONTROLGUI_H_
#define INCLUDE_FEATURE_REMOTECONTROLGUI_H_

#include <QTimer>
#include <QHash>
#include <QIcon>
#include <QtCharts>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/iot/device.h"
#include "gui/buttonswitch.h"
#include "settings/rollupstate.h"

#include "remotecontrolsettings.h"

class PluginAPI;
class FeatureUISet;
class RemoteControl;
class QGroupBox;
class QLabel;
class FlowLayout;

namespace Ui {
    class RemoteControlGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class RemoteControlGUI : public FeatureGUI {
    Q_OBJECT

    struct RemoteControlDeviceGUI {
        RemoteControlDevice *m_rcDevice;
        QWidget *m_container;
        QHash<QString, QList<QWidget *>> m_controls;
        QHash<QString, QLabel *> m_sensorValueLabels;
        QHash<QString, QTableWidgetItem *> m_sensorValueItems;
        QChartView *m_chartView;
        QChart *m_chart;
        QHash<QString, QLineSeries *> m_series;
        QHash<QString, QLineSeries *> m_onePointSeries; // Workaround for charts not drawing series with only one point properly

        RemoteControlDeviceGUI(RemoteControlDevice *rcDevice) :
            m_rcDevice(rcDevice),
            m_container(nullptr),
            m_chartView(nullptr),
            m_chart(nullptr)
        {
        }

        ~RemoteControlDeviceGUI()
        {
        }
    };

public:
    static RemoteControlGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
    Ui::RemoteControlGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    RemoteControlSettings m_settings;
    RollupState m_rollupState;
    bool m_doApplySettings;

    RemoteControl* m_remoteControl;
    MessageQueue m_inputMessageQueue;
    QList<RemoteControlDeviceGUI *> m_deviceGUIs;

    QIcon m_startStopIcon;

    enum SensorCol {
        COL_LABEL,
        COL_VALUE,
        COL_UNITS
    };

    explicit RemoteControlGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~RemoteControlGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

    void createControls(RemoteControlDeviceGUI *gui, QBoxLayout *vBox, FlowLayout *flow, int &widgetCnt);
    void createChart(RemoteControlDeviceGUI *gui, QVBoxLayout *vBox, const QString &id, const QString &units);
    void createSensors(RemoteControlDeviceGUI *gui, QVBoxLayout *vBox, FlowLayout *flow, int &widgetCnt, bool &hasCharts);
    RemoteControlDeviceGUI *createDeviceGUI(RemoteControlDevice *device);
    void createGUI();
    void updateControl(QWidget *widget, const DeviceDiscoverer::ControlInfo *controlInfo, const QString &key, const QVariant &value);
    void updateChart(RemoteControlDeviceGUI *deviceGUI, const QString &key, const QVariant &value);
    void deviceUpdated(const QString &protocol, const QString &deviceId, const QHash<QString, QVariant> &status);
    void deviceUnavailable(const QString &protocol, const QString &deviceId);

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_update_clicked();
    void on_settings_clicked();
    void on_clearData_clicked();
};

#endif // INCLUDE_FEATURE_REMOTECONTROLGUI_H_
