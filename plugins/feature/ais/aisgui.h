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

#ifndef INCLUDE_FEATURE_AISGUI_H_
#define INCLUDE_FEATURE_AISGUI_H_

#include <QTimer>
#include <QMenu>
#include <QDateTime>
#include <QHash>
#include <QRandomGenerator>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/ais.h"
#include "settings/rollupstate.h"

#include "aissettings.h"

class PluginAPI;
class FeatureUISet;
class AIS;

namespace Ui {
    class AISGUI;
}

class AISGUI : public FeatureGUI {
    Q_OBJECT

    // Holds information not in the table
    struct Vessel {
        QString m_image;
        QString m_model;
    };

public:
    static AISGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
    Ui::AISGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    AISSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;

    AIS* m_ais;
    MessageQueue m_inputMessageQueue;
    QTimer m_timer;
    int m_lastFeatureState;

    QRandomGenerator m_random;
    QHash<QString, Vessel *> m_vessels;             // Hash of mmsi to vessels
    static QStringList m_shipModels;
    static QStringList m_sailboatModels;
    static QHash<QString, float> m_labelOffset;
    static QHash<QString, float> m_modelOffset;

    QMenu *vesselsMenu;                         // Column select context menu

    explicit AISGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~AISGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);

    void sendToMap(const QString &name, const QString &label,
        const QString &image, const QString &text,
        const QString &model, float modelOffset, float labelOffset,
        float latitude, float longitude, QDateTime positionDateTime,
        float heading
    );
    void updateVessels(AISMessage *ais, QDateTime dateTime);
    void getImageAndModel(const QString &type, const QString &shipType, int length, const QString &status, Vessel *vessel);
    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked, const char *slot);

    enum VesselCol {
        VESSEL_COL_MMSI,
        VESSEL_COL_TYPE,
        VESSEL_COL_LATITUDE,
        VESSEL_COL_LONGITUDE,
        VESSEL_COL_COURSE,
        VESSEL_COL_SPEED,
        VESSEL_COL_HEADING,
        VESSEL_COL_STATUS,
        VESSEL_COL_IMO,
        VESSEL_COL_NAME,
        VESSEL_COL_CALLSIGN,
        VESSEL_COL_SHIP_TYPE,
        VESSEL_COL_LENGTH,
        VESSEL_COL_DESTINATION,
        VESSEL_COL_POSITION_UPDATE,
        VESSEL_COL_LAST_UPDATE,
        VESSEL_COL_MESSAGES
    };

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_vessels_cellDoubleClicked(int row, int column);
    void vessels_customContextMenuRequested(QPoint pos);
    void vessels_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void vessels_sectionResized(int logicalIndex, int oldSize, int newSize);
    void vesselsColumnSelectMenu(QPoint pos);
    void vesselsColumnSelectMenuChecked(bool checked = false);
    void removeOldVessels();
};

#endif // INCLUDE_FEATURE_AISGUI_H_
