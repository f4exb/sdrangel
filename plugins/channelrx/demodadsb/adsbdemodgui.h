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
#include <QMenu>
#include <QGeoCoordinate>
#include <QDateTime>
#include <QAbstractListModel>
#include <QProgressDialog>

#include "channel/channelgui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "util/azel.h"
#include "util/movingaverage.h"
#include "util/httpdownloadmanager.h"
#include "maincore.h"

#include "adsbdemodsettings.h"
#include "ourairportsdb.h"
#include "osndb.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ADSBDemod;
class WebAPIAdapterInterface;
class HttpDownloadManager;
class ADSBDemodGUI;

namespace Ui {
    class ADSBDemodGUI;
}

// Custom widget to allow formatted decimal numbers to be sorted numerically
class CustomDoubleTableWidgetItem : public QTableWidgetItem
{
public:
    CustomDoubleTableWidgetItem(const QString text = QString("")) :
        QTableWidgetItem(text)
    {
    }

    bool operator <(const QTableWidgetItem& other) const
    {
        // Treat "" as less than 0
        QString thisText = text();
        QString otherText = other.text();
        if (thisText == "")
            return true;
        if (otherText == "")
            return false;
        return thisText.toDouble() < otherText.toDouble();
    }
};

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
    int m_squawk;               // Mode-A code
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
    QDateTime m_cprTime[2];

    int m_adsbFrameCount;       // Number of ADS-B frames for this aircraft
    float m_minCorrelation;
    float m_maxCorrelation;
    float m_correlation;
    MovingAverageUtil<float, double, 100> m_correlationAvg;

    bool m_isTarget;            // Are we targetting this aircraft (sending az/el to rotator)
    bool m_isHighlighted;       // Are we highlighting this aircraft in the table and map
    bool m_showAll;

    QVariantList m_coordinates; // Coordinates we've recorded the aircraft at

    AircraftInformation *m_aircraftInfo; // Info about the aircraft from the database
    ADSBDemodGUI *m_gui;

    // GUI table items for above data
    QTableWidgetItem *m_icaoItem;
    QTableWidgetItem *m_flightItem;
    QTableWidgetItem *m_modelItem;
    QTableWidgetItem *m_airlineItem;
    QTableWidgetItem *m_latitudeItem;
    QTableWidgetItem *m_longitudeItem;
    QTableWidgetItem *m_altitudeItem;
    QTableWidgetItem *m_speedItem;
    QTableWidgetItem *m_headingItem;
    QTableWidgetItem *m_verticalRateItem;
    CustomDoubleTableWidgetItem *m_rangeItem;
    QTableWidgetItem *m_azElItem;
    QTableWidgetItem *m_emitterCategoryItem;
    QTableWidgetItem *m_statusItem;
    QTableWidgetItem *m_squawkItem;
    QTableWidgetItem *m_registrationItem;
    QTableWidgetItem *m_countryItem;
    QTableWidgetItem *m_registeredItem;
    QTableWidgetItem *m_manufacturerNameItem;
    QTableWidgetItem *m_ownerItem;
    QTableWidgetItem *m_operatorICAOItem;
    QTableWidgetItem *m_timeItem;
    QTableWidgetItem *m_adsbFrameCountItem;
    QTableWidgetItem *m_correlationItem;
    QTableWidgetItem *m_rssiItem;

    Aircraft(ADSBDemodGUI *gui) :
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
        m_isTarget(false),
        m_isHighlighted(false),
        m_showAll(false),
        m_aircraftInfo(nullptr),
        m_gui(gui)
    {
        for (int i = 0; i < 2; i++)
        {
            m_cprValid[i] = false;
        }
        // These are deleted by QTableWidget
        m_icaoItem = new QTableWidgetItem();
        m_flightItem = new QTableWidgetItem();
        m_modelItem = new QTableWidgetItem();
        m_airlineItem = new QTableWidgetItem();
        m_altitudeItem = new QTableWidgetItem();
        m_speedItem = new QTableWidgetItem();
        m_headingItem = new QTableWidgetItem();
        m_verticalRateItem = new QTableWidgetItem();
        m_rangeItem = new CustomDoubleTableWidgetItem();
        m_azElItem = new QTableWidgetItem();
        m_latitudeItem = new QTableWidgetItem();
        m_longitudeItem = new QTableWidgetItem();
        m_emitterCategoryItem = new QTableWidgetItem();
        m_statusItem = new QTableWidgetItem();
        m_squawkItem = new QTableWidgetItem();
        m_registrationItem = new QTableWidgetItem();
        m_countryItem = new QTableWidgetItem();
        m_registeredItem = new QTableWidgetItem();
        m_manufacturerNameItem = new QTableWidgetItem();
        m_ownerItem = new QTableWidgetItem();
        m_operatorICAOItem = new QTableWidgetItem();
        m_timeItem = new QTableWidgetItem();
        m_adsbFrameCountItem = new QTableWidgetItem();
        m_correlationItem = new QTableWidgetItem();
        m_rssiItem = new QTableWidgetItem();
    }

    QString getImage();
    QString getText(bool all=false);

    // Name to use when selected as a target
    QString targetName()
    {
        if (!m_flight.isEmpty())
            return QString("Flight: %1").arg(m_flight);
        else
            return QString("ICAO: %1").arg(m_icao, 0, 16);
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
        bubbleColourRole = Qt::UserRole + 5,
        aircraftPathRole = Qt::UserRole + 6,
        showAllRole = Qt::UserRole + 7,
        highlightedRole = Qt::UserRole + 8,
        targetRole = Qt::UserRole + 9
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

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void aircraftUpdated(Aircraft *aircraft) {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    void allAircraftUpdated() {
        /*
        // Not sure why this doesn't work - it should be more efficient
        // than the following code
        emit dataChanged(index(0), index(rowCount()));
        */
        for (int i = 0; i < m_aircrafts.count(); i++)
        {
            QModelIndex idx = index(i);
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
        roles[aircraftPathRole] = "aircraftPath";
        roles[showAllRole] = "showAll";
        roles[highlightedRole] = "highlighted";
        roles[targetRole] = "target";
        return roles;
    }

    void setFlightPaths(bool flightPaths)
    {
        m_flightPaths = flightPaths;
        allAircraftUpdated();
    }

    void setAllFlightPaths(bool allFlightPaths)
    {
        m_allFlightPaths = allFlightPaths;
        allAircraftUpdated();
    }

private:
    QList<Aircraft *> m_aircrafts;
    bool m_flightPaths;
    bool m_allFlightPaths;
};

// Airport data model used by QML map item
class AirportModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles {
        positionRole = Qt::UserRole + 1,
        airportDataRole = Qt::UserRole + 2,
        airportDataRowsRole = Qt::UserRole + 3,
        airportImageRole = Qt::UserRole + 4,
        bubbleColourRole = Qt::UserRole + 5,
        showFreqRole  = Qt::UserRole + 6,
        selectedFreqRole  = Qt::UserRole + 7
    };

    AirportModel(ADSBDemodGUI *gui) :
        m_gui(gui)
    {
    }

    Q_INVOKABLE void addAirport(AirportInformation *airport, float az, float el, float distance) {
        QString text;
        int rows;

        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_airports.append(airport);
        airportFreq(airport, az, el, distance, text, rows);
        m_airportDataFreq.append(text);
        m_airportDataFreqRows.append(rows);
        m_showFreq.append(false);
        m_azimuth.append(az);
        m_elevation.append(el);
        endInsertRows();
    }

    void removeAirport(AirportInformation *airport) {
        int row = m_airports.indexOf(airport);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_airports.removeAt(row);
            m_airportDataFreq.removeAt(row);
            m_airportDataFreqRows.removeAt(row);
            m_showFreq.removeAt(row);
            m_azimuth.removeAt(row);
            m_elevation.removeAt(row);
            endRemoveRows();
        }
    }

    void removeAllAirports() {
        if (m_airports.count() > 0)
        {
            beginRemoveRows(QModelIndex(), 0, m_airports.count() - 1);
            m_airports.clear();
            m_airportDataFreq.clear();
            m_airportDataFreqRows.clear();
            m_showFreq.clear();
            m_azimuth.clear();
            m_elevation.clear();
            endRemoveRows();
        }
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent)
        return m_airports.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void airportFreq(AirportInformation *airport, float az, float el, float distance, QString& text, int& rows) {
        // Create the text to go in the bubble next to the airport
        // Display name and frequencies
        QStringList list;

        list.append(QString("%1: %2").arg(airport->m_ident).arg(airport->m_name));
        rows = 1;
        for (int i = 0; i < airport->m_frequencies.size(); i++)
        {
            AirportInformation::FrequencyInformation *frequencyInfo = airport->m_frequencies[i];
            list.append(QString("%1: %2 MHz").arg(frequencyInfo->m_type).arg(frequencyInfo->m_frequency));
            rows++;
        }
        list.append(QString("Az/El: %1/%2").arg((int)std::round(az)).arg((int)std::round(el)));
        list.append(QString("Distance: %1 km").arg(distance/1000.0f, 0, 'f', 1));
        rows += 2;
        text = list.join("\n");
    }

    void airportUpdated(AirportInformation *airport) {
        int row = m_airports.indexOf(airport);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    QHash<int, QByteArray> roleNames() const {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[airportDataRole] = "airportData";
        roles[airportDataRowsRole] = "airportDataRows";
        roles[airportImageRole] = "airportImage";
        roles[bubbleColourRole] = "bubbleColour";
        roles[showFreqRole] = "showFreq";
        roles[selectedFreqRole] = "selectedFreq";
        return roles;
    }

private:
    ADSBDemodGUI *m_gui;
    QList<AirportInformation *> m_airports;
    QList<QString> m_airportDataFreq;
    QList<int> m_airportDataFreqRows;
    QList<bool> m_showFreq;
    QList<float> m_azimuth;
    QList<float> m_elevation;
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
    void highlightAircraft(Aircraft *aircraft);
    void targetAircraft(Aircraft *aircraft);
    void target(const QString& name, float az, float el);
    bool setFrequency(float frequency);
    bool useSIUints() { return m_settings.m_siUnits; }

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

    QHash<int, Aircraft *> m_aircraft;  // Hashed on ICAO
    QHash<int, AircraftInformation *> *m_aircraftInfo;
    QHash<int, AirportInformation *> *m_airportInfo; // Hashed on id
    AircraftModel m_aircraftModel;
    AirportModel m_airportModel;
    QHash<QString, QIcon *> m_airlineIcons; // Hashed on airline ICAO
    QHash<QString, QIcon *> m_flagIcons;    // Hashed on country
    QHash<QString, QString> *m_prefixMap;   // Registration to country (flag name)
    QHash<QString, QString> *m_militaryMap;   // Operator airforce to military (flag name)

    AzEl m_azEl;                        // Position of station
    Aircraft *m_trackAircraft;          // Aircraft we want to track in Channel Report
    MovingAverageUtil<float, double, 10> m_correlationAvg;
    MovingAverageUtil<float, double, 10> m_correlationOnesAvg;
    Aircraft *m_highlightAircraft;      // Aircraft we want to highlight, when selected in table

    float m_currentAirportRange;        // Current settings, so we only update if changed
    ADSBDemodSettings::AirportType m_currentAirportMinimumSize;
    bool m_currentDisplayHeliports;

    QMenu *menu;                        // Column select context menu

    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    HttpDownloadManager m_dlm;
    QProgressDialog *m_progressDialog;

    explicit ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~ADSBDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayStreamIndex();
    bool handleMessage(const Message& message);
    void updatePosition(Aircraft *aircraft);
    bool updateLocalPosition(Aircraft *aircraft, double latitude, double longitude, bool surfacePosition);
    void handleADSB(
        const QByteArray data,
        const QDateTime dateTime,
        float correlation,
        float correlationOnes);
    void resizeTable();
    QString getDataDir();
    QString getAirportDBFilename();
    QString getAirportFrequenciesDBFilename();
    QString getOSNDBFilename();
    QString getFastDBFilename();
    qint64 fileAgeInDays(QString filename);
    bool confirmDownload(QString filename);
    void readAirportDB(const QString& filename);
    void readAirportFrequenciesDB(const QString& filename);
    bool readOSNDB(const QString& filename);
    bool readFastDB(const QString& filename);
    void updateAirports();
    QIcon *getAirlineIcon(const QString &operatorICAO);
    QIcon *getFlagIcon(const QString &country);
    void updateDeviceSetList();
    QAction *createCheckableItem(QString& text, int idx, bool checked);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int value);
    void on_threshold_valueChanged(int value);
    void on_phaseSteps_valueChanged(int value);
    void on_tapsPerPhase_valueChanged(int value);
    void on_adsbData_cellClicked(int row, int column);
    void on_adsbData_cellDoubleClicked(int row, int column);
    void adsbData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void adsbData_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void on_spb_currentIndexChanged(int value);
    void on_correlateFullPreamble_clicked(bool checked);
    void on_demodModeS_clicked(bool checked);
    void on_feed_clicked(bool checked);
    void on_getOSNDB_clicked();
    void on_getAirportDB_clicked();
    void on_flightPaths_clicked(bool checked);
    void on_allFlightPaths_clicked(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
    void updateDownloadProgress(qint64 bytesRead, qint64 totalBytes);
    void downloadFinished(const QString& filename, bool success);
    void on_devicesRefresh_clicked();
    void on_device_currentIndexChanged(int index);
    void feedSelect();
    void on_displaySettings_clicked();
signals:
    void homePositionChanged();
};

#endif // INCLUDE_ADSBDEMODGUI_H
