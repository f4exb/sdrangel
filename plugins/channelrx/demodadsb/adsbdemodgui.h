///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_ADSBDEMODGUI_H
#define INCLUDE_ADSBDEMODGUI_H

#include <QTableWidgetItem>
#include <QGeoCoordinate>
#include <QDateTime>
#include <QAbstractListModel>

#include "channel/channelgui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "util/azel.h"

#include "adsbdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ADSBDemod;

namespace Ui {
    class ADSBDemodGUI;
}

// Data about an aircraft extracted from an ADS-B frames
struct Aircraft {
    int m_icao;                 // 24-bit ICAO aircraft address
    QString m_flight;           // Flight callsign
    Real m_latitude;            // Latitude in decimal degrees
    Real m_longitude;           // Longitude in decimal degrees
    int m_altitude;             // Altitude in feet
    int m_speed;                // Speed in knots
    enum SpeedType {
        GS,                     // Ground speed
        TAS,                    // True air speed
        IAS                     // Indicated air speed
    } m_speedType;
    static const char *m_speedTypeNames[];
    int m_heading;              // Heading in degrees
    int m_verticalRate;         // Vertical climb rate in ft/min
    QString m_emitterCategory;  // Aircraft type
    QString m_status;           // Aircraft status
    Real m_range;               // Distance from station to aircraft
    Real m_azimuth;             // Azimuth from station to aircraft
    Real m_elevation;           // Elevation from station to aicraft;
    QDateTime m_time;           // When last updated

    bool m_positionValid;       // Indicates if we have valid data for the above fields
    bool m_altitudeValid;
    bool m_speedValid;
    bool m_headingValid;
    bool m_verticalRateValid;

    // State for calculating position using two CPR frames
    bool m_cprValid[2];
    Real m_cprLat[2];
    Real m_cprLong[2];

    int m_adsbFrameCount;       // Number of ADS-B frames for this aircraft
    float m_minCorrelation;
    float m_maxCorrelation;
    float m_correlation;
    bool m_isBeingTracked;      // Are we tracking this aircraft

    // GUI table items for above data
    QTableWidgetItem *m_icaoItem;
    QTableWidgetItem *m_flightItem;
    QTableWidgetItem *m_latitudeItem;
    QTableWidgetItem *m_longitudeItem;
    QTableWidgetItem *m_altitudeItem;
    QTableWidgetItem *m_speedItem;
    QTableWidgetItem *m_headingItem;
    QTableWidgetItem *m_verticalRateItem;
    QTableWidgetItem *m_emitterCategoryItem;
    QTableWidgetItem *m_statusItem;
    QTableWidgetItem *m_rangeItem;
    QTableWidgetItem *m_azElItem;
    QTableWidgetItem *m_timeItem;
    QTableWidgetItem *m_adsbFrameCountItem;
    QTableWidgetItem *m_correlationItem;

    Aircraft() :
        m_icao(0),
        m_latitude(0),
        m_longitude(0),
        m_altitude(0),
        m_speed(0),
        m_heading(0),
        m_verticalRate(0),
        m_azimuth(0),
        m_elevation(0),
        m_positionValid(false),
        m_altitudeValid(false),
        m_speedValid(false),
        m_headingValid(false),
        m_verticalRateValid(false),
        m_adsbFrameCount(0),
        m_minCorrelation(INFINITY),
        m_maxCorrelation(-INFINITY),
        m_correlation(0.0f),
        m_isBeingTracked(false)
    {
        for (int i = 0; i < 2; i++)
        {
            m_cprValid[i] = false;
        }
        // These are deleted by QTableWidget
        m_icaoItem = new QTableWidgetItem();
        m_flightItem = new QTableWidgetItem();
        m_latitudeItem = new QTableWidgetItem();
        m_longitudeItem = new QTableWidgetItem();
        m_altitudeItem = new QTableWidgetItem();
        m_speedItem = new QTableWidgetItem();
        m_headingItem = new QTableWidgetItem();
        m_verticalRateItem = new QTableWidgetItem();
        m_emitterCategoryItem = new QTableWidgetItem();
        m_statusItem = new QTableWidgetItem();
        m_rangeItem = new QTableWidgetItem();
        m_azElItem = new QTableWidgetItem();
        m_timeItem = new QTableWidgetItem();
        m_adsbFrameCountItem = new QTableWidgetItem();
        m_correlationItem = new QTableWidgetItem();
    }
};

// Aircraft data model used by QML map item
class AircraftModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles{
        positionRole = Qt::UserRole + 1,
        headingRole = Qt::UserRole + 2,
        adsbDataRole = Qt::UserRole + 3,
        aircraftImageRole = Qt::UserRole + 4,
        bubbleColourRole  = Qt::UserRole + 5
    };

    Q_INVOKABLE void addAircraft(Aircraft *aircraft) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_aircrafts.append(aircraft);
        endInsertRows();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent)
        return m_aircrafts.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void aircraftUpdated(Aircraft *aircraft) {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    void removeAircraft(Aircraft *aircraft) {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_aircrafts.removeAt(row);
            endRemoveRows();
        }
    }

    QHash<int, QByteArray> roleNames() const {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[headingRole] = "heading";
        roles[adsbDataRole] = "adsbData";
        roles[aircraftImageRole] = "aircraftImage";
        roles[bubbleColourRole] = "bubbleColour";
        return roles;
    }

private:
    QList<Aircraft *> m_aircrafts;
};

class ADSBDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static ADSBDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::ADSBDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    ADSBDemodSettings m_settings;
    bool m_basicSettingsShown;
    bool m_doApplySettings;

    ADSBDemod* m_adsbDemod;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QHash<int,Aircraft *> m_aircraft;  // Hashed on ICAO
    AircraftModel m_aircraftModel;

    AzEl m_azEl;                        // Position of station
    Aircraft *m_trackAircraft;          // Aircraft we want to track in Channel Report

    explicit ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~ADSBDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayStreamIndex();
    bool handleMessage(const Message& message);
    void updatePosition(Aircraft *aircraft);
    void handleADSB(const QByteArray data, const QDateTime dateTime, float correlationOnes, float correlationZeros);
    void resizeTable();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int value);
    void on_threshold_valueChanged(int value);
    void on_adsbData_cellDoubleClicked(int row, int column);
    void on_spb_currentIndexChanged(int value);
    void on_beastEnabled_stateChanged(int state);
    void on_host_editingFinished(QString value);
    void on_port_valueChanged(int value);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
signals:
    void homePositionChanged();
};

#endif // INCLUDE_ADSBDEMODGUI_H
