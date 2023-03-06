///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_VORLOCALIZERGUI_H
#define INCLUDE_VORLOCALIZERGUI_H

#include <QIcon>
#include <QAbstractListModel>
#include <QModelIndex>
#include <QProgressDialog>
#include <QGeoLocation>
#include <QGeoCoordinate>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>

#include "feature/featuregui.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "util/httpdownloadmanager.h"
#include "util/azel.h"
#include "util/openaip.h"
#include "settings/rollupstate.h"

#include "vorlocalizersettings.h"

class PluginAPI;
class FeatureUISet;
class Feature;
class BasebandSampleSink;
class VORLocalizer;
class VORLocalizerGUI;

namespace Ui {
    class VORLocalizerGUI;
}
class VORLocalizerGUI;

// Table items for each VOR
class VORGUI : public QObject {
    Q_OBJECT
public:
    NavAid *m_navAid;
    QVariantList m_coordinates;
    VORLocalizerGUI *m_gui;

    QTableWidgetItem *m_nameItem;
    QTableWidgetItem *m_frequencyItem;
    QTableWidgetItem *m_identItem;
    QTableWidgetItem *m_morseItem;
    QTableWidgetItem *m_radialItem;
    QTableWidgetItem *m_rxIdentItem;
    QTableWidgetItem *m_rxMorseItem;
    QTableWidgetItem *m_varMagItem;
    QTableWidgetItem *m_refMagItem;
    QWidget *m_muteItem;
    QToolButton *m_muteButton;

    VORGUI(NavAid *navAid, VORLocalizerGUI *gui);
private slots:
    void on_audioMute_toggled(bool checked);
};

// VOR model used for each VOR on the map
class VORModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles{
        positionRole = Qt::UserRole + 1,
        vorDataRole = Qt::UserRole + 2,
        vorImageRole = Qt::UserRole + 3,
        vorRadialRole = Qt::UserRole + 4,
        bubbleColourRole = Qt::UserRole + 5,
        selectedRole = Qt::UserRole + 6
    };

    VORModel(VORLocalizerGUI *gui) :
        m_gui(gui),
        m_radialsVisible(true)
    {
    }

    Q_INVOKABLE void addVOR(NavAid *vor) {
        if (!m_vors.contains(vor))
        {
            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            m_vors.append(vor);
            m_selected.append(false);
            m_radials.append(-1.0f);
            m_vorGUIs.append(nullptr);
            endInsertRows();
        }
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent)
        return m_vors.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void allVORUpdated()
    {
        for (int i = 0; i < m_vors.count(); i++)
        {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
        }
    }

    void removeVOR(NavAid *vor)
    {
        int row = m_vors.indexOf(vor);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_vors.removeAt(row);
            m_selected.removeAt(row);
            m_radials.removeAt(row);
            m_vorGUIs.removeAt(row);
            endRemoveRows();
        }
    }

    void removeAllVORs()
    {
        if (m_vors.count() > 0)
        {
            beginRemoveRows(QModelIndex(), 0, m_vors.count() - 1);
            m_vors.clear();
            m_selected.clear();
            m_radials.clear();
            m_vorGUIs.clear();
            endRemoveRows();
        }
    }

    void removeAllExceptSelected()
    {
        for (int i = 0; i < m_vors.count(); i++)
        {
            if (!m_selected[i])
            {
                removeVOR(m_vors[i]);
                i--;
            }
        }
    }

    QHash<int, QByteArray> roleNames() const {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[vorDataRole] = "vorData";
        roles[vorImageRole] = "vorImage";
        roles[vorRadialRole] = "vorRadial";
        roles[bubbleColourRole] = "bubbleColour";
        roles[selectedRole] = "selected";
        return roles;
    }

    void setRadialsVisible(bool radialsVisible)
    {
        m_radialsVisible = radialsVisible;
        allVORUpdated();
    }

    void setRadial(int id, bool valid, Real radial)
    {
        for (int i = 0; i < m_vors.count(); i++)
        {
            if (m_vors[i]->m_id == id)
            {
                if (valid)
                    m_radials[i] = radial;
                else
                    m_radials[i] = -1; // -1 to indicate invalid
                QModelIndex idx = index(i);
                emit dataChanged(idx, idx);
                break;
            }
        }
    }

    bool findIntersection(float &lat, float &lon);
    QString getRadials() const;

private:
    VORLocalizerGUI *m_gui;
    bool m_radialsVisible;
    QList<NavAid *> m_vors;
    QList<bool> m_selected;
    QList<Real> m_radials;
    QList<VORGUI *> m_vorGUIs;
};

class VORLocalizerGUI : public FeatureGUI {
    Q_OBJECT

public:
    static VORLocalizerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void selectVOR(VORGUI *vorGUI, bool selected);
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

private:
    friend class VORGUI;
    friend class VORModel;

    Ui::VORLocalizerGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    VORLocalizerSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;

    VORLocalizer* m_vorLocalizer;
    bool m_squelchOpen;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QMenu *menu;                        // Column select context menu
    HttpDownloadManager m_dlm;
    QProgressDialog *m_progressDialog;
    OpenAIP m_openAIP;
    int m_countryIndex;
    VORModel m_vorModel;
    QSharedPointer<const QList<NavAid *>> m_vors;
    QHash<int, VORGUI *> m_selectedVORs;
    AzEl m_azEl;                        // Position of station
    QIcon m_muteIcon;
	QTimer m_statusTimer;
	int m_lastFeatureState;
    int m_rrSecondsCount;
    QTimer m_redrawMapTimer;
    QString m_mapFeaturePositionName;
    QStringList m_mapFeatureRadialNames;

    explicit VORLocalizerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~VORLocalizerGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void redrawMap();
    void makeUIConnections();

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked);

    void calculateFreqOffset(VORGUI *vorGUI);
    void calculateFreqOffsets();
    void updateVORs();
    void readNavAids();
    void updateChannelList();
    void applyMapSettings();
    void clearFromMapFeature(const QString& name, int type);
    void sendPositionToMapFeature(float lat, float lon);
    void sendRadialToMapFeature(VORGUI *vorGUI, Real radial);

private slots:
    void on_startStop_toggled(bool checked);
    void on_getOpenAIPVORDB_clicked();
    void on_magDecAdjust_toggled(bool checked);
    void on_rrTime_valueChanged(int value);
    void on_centerShift_valueChanged(int value);
    void channelsRefresh();
    void vorData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void vorData_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void updateStatus();
    void tick();
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadNavAidsFinished();
    void preferenceChanged(int elementType);
    virtual void showEvent(QShowEvent *event);
    virtual bool eventFilter(QObject *obj, QEvent *event);
};

#endif // INCLUDE_VORLOCALIZERGUI_H

