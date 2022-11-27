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

#ifndef INCLUDE_FEATURE_RADIOSONDEGUI_H_
#define INCLUDE_FEATURE_RADIOSONDEGUI_H_

#include <QTimer>
#include <QMenu>
#include <QDateTime>
#include <QHash>
#include <QtCharts>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/radiosonde.h"
#include "settings/rollupstate.h"

#include "radiosondesettings.h"

class PluginAPI;
class FeatureUISet;
class Radiosonde;

namespace Ui {
    class RadiosondeGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class RadiosondeGUI : public FeatureGUI {
    Q_OBJECT

    // Holds information not in the table
    struct RadiosondeData {

        QList<QDateTime> m_messagesDateTime;
        QList<RS41Frame *> m_messages;

        RS41Subframe m_subframe;

        ~RadiosondeData()
        {
            qDeleteAll(m_messages);
        }

        void addMessage(QDateTime dateTime, RS41Frame *message)
        {
            m_messagesDateTime.append(dateTime);
            m_messages.append(message);
        }

    };

public:
    static RadiosondeGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
    Ui::RadiosondeGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    RadiosondeSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;

    Radiosonde* m_radiosonde;
    MessageQueue m_inputMessageQueue;
    int m_lastFeatureState;

    QHash<QString, RadiosondeData *> m_radiosondes; // Hash of serial to radiosondes

    QMenu *radiosondesMenu;                         // Column select context menu

    explicit RadiosondeGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~RadiosondeGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

    void sendToMap(const QString &name, const QString &label,
        const QString &image, const QString &text,
        const QString &model, float labelOffset,
        float latitude, float longitude, float altitude, QDateTime positionDateTime,
        float heading
    );
    void updateRadiosondes(RS41Frame *radiosonde, QDateTime dateTime);
    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked, const char *slot);
    void plotChart();
    float getData(RadiosondeSettings::ChartData dataType, RadiosondeData *radiosonde, RS41Frame *message);

    enum RadiosondeCol {
        RADIOSONDE_COL_SERIAL,
        RADIOSONDE_COL_TYPE,
        RADIOSONDE_COL_LATITUDE,
        RADIOSONDE_COL_LONGITUDE,
        RADIOSONDE_COL_ALTITUDE,
        RADIOSONDE_COL_SPEED,
        RADIOSONDE_COL_VERTICAL_RATE,
        RADIOSONDE_COL_HEADING,
        RADIOSONDE_COL_STATUS,
        RADIOSONDE_COL_PRESSURE,
        RADIOSONDE_COL_TEMPERATURE,
        RADIOSONDE_COL_HUMIDITY,
        RADIOSONDE_COL_ALT_MAX,
        RADIOSONDE_COL_FREQUENCY,
        RADIOSONDE_COL_BURSTKILL_STATUS,
        RADIOSONDE_COL_BURSTKILL_TIMER,
        RADIOSONDE_COL_LAST_UPDATE,
        RADIOSONDE_COL_MESSAGES
    };

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_radiosondes_itemSelectionChanged();
    void on_radiosondes_cellDoubleClicked(int row, int column);
    void radiosondes_customContextMenuRequested(QPoint pos);
    void radiosondes_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void radiosondes_sectionResized(int logicalIndex, int oldSize, int newSize);
    void radiosondesColumnSelectMenu(QPoint pos);
    void radiosondesColumnSelectMenuChecked(bool checked = false);
    void on_y1_currentIndexChanged(int index);
    void on_y2_currentIndexChanged(int index);
    void on_deleteAll_clicked();
};

#endif // INCLUDE_FEATURE_RADIOSONDEGUI_H_
