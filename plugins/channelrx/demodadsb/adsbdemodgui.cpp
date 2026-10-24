///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020-2024 Jon Beniston, M7RCE                                   //
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

#include <cmath>

#include <QDockWidget>
#include <QMainWindow>
#include <QQuickItem>
#include <QGeoLocation>
#include <QQmlContext>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QDebug>
#include <QProcess>
#include <QClipboard>
#include <QFileDialog>
#include <QQmlProperty>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDir>
#ifdef QT_LOCATION_FOUND
#include <QGeoServiceProvider>
#endif

#include "ui_adsbdemodgui.h"
#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "dsp/devicesamplesource.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "util/aircraftreport.h"
#include "plugin/pluginapi.h"
#include "util/airlines.h"
#include "util/crc.h"
#include "util/db.h"
#include "util/units.h"
#include "util/morse.h"
#include "util/profiler.h"
#include "gui/messagedialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/clickablelabel.h"
#include "gui/tabletapandhold.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/checklist.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "adsbdemodreport.h"
#include "adsbdemod.h"
#include "util/flightpathpalette.h"

#include "adsbdemodgui.h"
#include "adsbdemodfeeddialog.h"
#include "adsbdemoddisplaydialog.h"
#include "adsbdemodnotificationdialog.h"
#include "adsb.h"
#include "adsbosmtemplateserver.h"
#include "gui/fastitemdelegate.h"
#include "gui/fastheaderview.h"

static const unsigned char colors[3*64] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x55,
    0x00, 0x00, 0xaa,
    0x00, 0x00, 0xff,
    0x00, 0x55, 0x00,
    0x00, 0x55, 0x55,
    0x00, 0x55, 0xaa,
    0x00, 0x55, 0xff,
    0x00, 0xaa, 0x00,
    0x00, 0xaa, 0x55,
    0x00, 0xaa, 0xaa,
    0x00, 0xaa, 0xff,
    0x00, 0xff, 0x00,
    0x00, 0xff, 0x55,
    0x00, 0xff, 0xaa,
    0x00, 0xff, 0xff,
    0x55, 0x00, 0x00,
    0x55, 0x00, 0x55,
    0x55, 0x00, 0xaa,
    0x55, 0x00, 0xff,
    0x55, 0x55, 0x00,
    0x55, 0x55, 0x55,
    0x55, 0x55, 0xaa,
    0x55, 0x55, 0xff,
    0x55, 0xaa, 0x00,
    0x55, 0xaa, 0x55,
    0x55, 0xaa, 0xaa,
    0x55, 0xaa, 0xff,
    0x55, 0xff, 0x00,
    0x55, 0xff, 0x55,
    0x55, 0xff, 0xaa,
    0x55, 0xff, 0xff,
    0xaa, 0x00, 0x00,
    0xaa, 0x00, 0x55,
    0xaa, 0x00, 0xaa,
    0xaa, 0x00, 0xff,
    0xaa, 0x55, 0x00,
    0xaa, 0x55, 0x55,
    0xaa, 0x55, 0xaa,
    0xaa, 0x55, 0xff,
    0xaa, 0xaa, 0x00,
    0xaa, 0xaa, 0x55,
    0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xff,
    0xaa, 0xff, 0x00,
    0xaa, 0xff, 0x55,
    0xaa, 0xff, 0xaa,
    0xaa, 0xff, 0xff,
    0xff, 0x00, 0x00,
    0xff, 0x00, 0x55,
    0xff, 0x00, 0xaa,
    0xff, 0x00, 0xff,
    0xff, 0x55, 0x00,
    0xff, 0x55, 0x55,
    0xff, 0x55, 0xaa,
    0xff, 0x55, 0xff,
    0xff, 0xaa, 0x00,
    0xff, 0xaa, 0x55,
    0xff, 0xaa, 0xaa,
    0xff, 0xaa, 0xff,
    0xff, 0xff, 0x00,
    0xff, 0xff, 0x55,
    0xff, 0xff, 0xaa,
    0xff, 0xff, 0xff,
};

ADSBDemodGUI* ADSBDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    ADSBDemodGUI* gui = new ADSBDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void ADSBDemodGUI::destroy()
{
    delete this;
}

void ADSBDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings(QStringList(), true);
    applyAllSettings();
}

QByteArray ADSBDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool ADSBDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        updateChannelList();
        displaySettings(QStringList(), true);
        applyAllSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void Aircraft::addCoordinate(const ADSBAircraftPosition& position, AircraftModel *model)
{
    int color = FlightPathPalette::getColorIndex(position.m_altitude);

    bool showFullPath = (model->m_settings->m_flightPaths && m_isHighlighted) || model->m_settings->m_allFlightPaths;
    bool showATCPath = model->m_settings->m_atcLabels && !showFullPath;

    QGeoCoordinate coord(position.m_latitude, position.m_longitude, position.m_altitude);
    if (m_coordinates.isEmpty())
    {
        QVariantList list;
        list.push_back(QVariant::fromValue(coord));
        m_coordinates.push_back(list);
        m_coordinateColors.push_back(color);
        if (showFullPath) {
            model->aircraftCoordsAdded(this);
        }
    }
    else
    {
        if (color == m_lastColor)
        {
            QVariantList& list = m_coordinates.back();
            list.push_back(QVariant::fromValue(coord));
            if (showFullPath) {
                model->aircraftCoordsUpdated(this);
            }
        }
        else
        {
            QVariantList& prevList = m_coordinates.back();
            prevList.push_back(QVariant::fromValue(coord));
            m_coordinateDateTimes.push_back(position.m_dateTime);
            if (showFullPath) {
                model->aircraftCoordsUpdated(this);
            }

            QVariantList list;
            list.push_back(QVariant::fromValue(coord));
            m_coordinates.push_back(list);
            m_coordinateColors.push_back(color);
            if (showFullPath) {
                model->aircraftCoordsAdded(this);
            }
        }
    }

    if (m_recentCoordinates.isEmpty())
    {
        QVariantList list;
        list.push_back(QVariant::fromValue(coord));
        m_recentCoordinates.push_back(list);
        m_recentCoordinateColors.push_back(color);
        if (showATCPath) {
            model->aircraftCoordsAdded(this);
        }
    }
    else
    {
        if (color == m_lastColor)
        {
            QVariantList& list = m_recentCoordinates.back();
            list.push_back(QVariant::fromValue(coord));
            if (showATCPath) {
                model->aircraftCoordsUpdated(this);
            }
        }
        else
        {
            QVariantList& prevList = m_recentCoordinates.back();
            prevList.push_back(QVariant::fromValue(coord));
            if (showATCPath) {
                model->aircraftCoordsUpdated(this);
            }

            QVariantList list;
            list.push_back(QVariant::fromValue(coord));
            m_recentCoordinates.push_back(list);
            m_recentCoordinateColors.push_back(color);
            if (showATCPath) {
                model->aircraftCoordsAdded(this);
            }
        }
    }
    m_coordinateDateTimes.push_back(position.m_dateTime);
    m_lastColor = color;

    // Prune recent list so only last 25 seconds of coordinates
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-25);
    int keepCount = 0;
    for (int i = m_coordinateDateTimes.size() - 1; i >= 0; i--)
    {
        if (m_coordinateDateTimes[i] < cutoff) {
            break;
        }
        keepCount++;
    }
    bool removed = false;
    for (int i = m_recentCoordinates.size() - 1; i >= 0; i--)
    {
        int size = (int) m_recentCoordinates[i].size();

        if (keepCount <= 0)
        {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            m_recentCoordinates.remove(0, i + 1);
            m_recentCoordinateColors.remove(0, i + 1);
#else
            for (int j = 0; j < i + 1; j++)
            {
                m_recentCoordinates.removeAt(0);
                m_recentCoordinateColors.removeAt(0);
            }
#endif
            removed = true;
            break;
        }
        else
        {
            if (size > keepCount)
            {
                int remove = size - keepCount + 1;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                m_recentCoordinates[i].remove(0, remove);
#else
                for (int j = 0; j < remove; j++) {
                    m_recentCoordinates[i].removeAt(0);
                }
#endif
                removed = true;
                keepCount -= remove;
            }
            else
            {
                keepCount -= size;
            }
        }
    }
    if (showATCPath && removed) {
        model->aircraftCoordsRemoved(this);
    }
}

void Aircraft::clearCoordinates(AircraftModel *model)
{
    m_coordinates.clear();
    m_coordinateColors.clear();
    m_recentCoordinates.clear();
    m_recentCoordinateColors.clear();
    m_coordinateDateTimes.clear();
    model->clearCoords(this);
}

void AircraftPathModel::add()
{
    beginInsertRows(QModelIndex(), m_paths, m_paths);
    m_paths++;
    endInsertRows();
}

void AircraftPathModel::updateLast()
{
    QModelIndex idx = index(m_paths - 1);
    emit dataChanged(idx, idx);
}

void AircraftPathModel::clear()
{
    if (m_paths > 0)
    {
        beginRemoveRows(QModelIndex(), 0, m_paths - 1);
        m_paths = 0;
        endRemoveRows();
    }
}

void AircraftPathModel::removed()
{
    beginResetModel();
    // Coordinates are never removed from m_coordinates
    m_paths = m_aircraft->m_recentCoordinates.size();
    endResetModel();
}

void AircraftPathModel::settingsUpdated()
{
    if (!m_aircraftModel->m_settings) {
        return;
    }

    bool showFullPath = (m_aircraftModel->m_settings->m_flightPaths && m_aircraft->m_isHighlighted) || m_aircraftModel->m_settings->m_allFlightPaths;
    bool showATCPath = m_aircraftModel->m_settings->m_atcLabels && !showFullPath;

    if (showFullPath)
    {
        if (!m_showFullPath)
        {
            m_showFullPath = showFullPath;
            m_showATCPath = false;
            beginResetModel();
            m_paths = m_aircraft->m_coordinates.size();
            endResetModel();
        }
    }
    else if (showATCPath)
    {
        if (!m_showATCPath)
        {
            m_showFullPath = false;
            m_showATCPath = showATCPath;
            beginResetModel();
            m_paths = m_aircraft->m_recentCoordinates.size();
            endResetModel();
        }
    }
    else if (m_showFullPath || m_showATCPath)
    {
        m_showFullPath = false;
        m_showATCPath = false;
        beginResetModel();
        m_paths = 0;
        endResetModel();
    }
}

QVariant AircraftPathModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_paths)) {
        return QVariant();
    }
    /*qDebug() << "AircraftPathModel::data: row" << row << "m_paths" << m_paths << "m_showFullPath" << m_showFullPath << "m_showATCPath" << m_showATCPath
        << "m_aircraft->m_coordinates.size()" << m_aircraft->m_coordinates.size() << "m_aircraft->m_recentCoordinates" << m_aircraft->m_recentCoordinates.size()
        << "m_aircraft->m_coordinateColors.size()" << m_aircraft->m_coordinateColors.size() << "m_aircraft->m_recentCoordinateColors" << m_aircraft->m_recentCoordinateColors.size();*/

    if (role == AircraftPathModel::coordinatesRole)
    {
        if (m_showFullPath) {
            return m_aircraft->m_coordinates[row];
        } else if (m_showATCPath) {
            return m_aircraft->m_recentCoordinates[row];
        } else {
            return QVariantList();
        }
    }
    else if (role == AircraftPathModel::colorRole)
    {
        if (m_showFullPath) {
            return m_aircraftModel->m_settings->m_flightPathPalette[m_aircraft->m_coordinateColors[row]];
        } else if (m_showATCPath) {
            return m_aircraftModel->m_settings->m_flightPathPalette[m_aircraft->m_recentCoordinateColors[row]];
        } else {
            return QVariant();
        }
    }
    return QVariant();
}

QVariant AircraftModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_aircrafts.count()))
        return QVariant();
    if (role == AircraftModel::positionRole)
    {
        // Coordinates to display the aircraft icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_aircrafts[row]->m_latitude);
        coords.setLongitude(m_aircrafts[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_aircrafts[row]->m_altitude));
        return QVariant::fromValue(coords);
    }
    else if (role == AircraftModel::headingRole)
    {
        // What rotation to draw the aircraft icon at
        if (m_aircrafts[row]->m_trackValid) {
            return QVariant::fromValue(m_aircrafts[row]->m_track);
        } else {
            return QVariant::fromValue(m_aircrafts[row]->m_heading);
        }
    }
    else if (role == AircraftModel::adsbDataRole)
    {
        // Create the text to go in the bubble next to the aircraft
        return QVariant::fromValue(m_aircrafts[row]->getText(m_settings, m_aircrafts[row]->m_showAll));
    }
    else if (role == AircraftModel::aircraftImageRole)
    {
        // Select an image to use for the aircraft
        if (m_settings && m_settings->m_useLiveryIcons)
        {
            QString image = m_aircrafts[row]->getLiveryImage();
            if (!image.isEmpty()) {
                return QVariant::fromValue(image);
            }
        }
        return QVariant::fromValue(m_aircrafts[row]->getImage());
    }
    else if (role == AircraftModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the aircraft
        if (m_aircrafts[row]->m_isTarget)
            return  QVariant::fromValue(QColor("lightgreen"));
        else if (m_aircrafts[row]->m_isHighlighted)
            return  QVariant::fromValue(QColor("orange"));
        else if ((m_aircrafts[row]->m_status.length() > 0) && m_aircrafts[row]->m_status.compare("No emergency"))
            return QVariant::fromValue(QColor("lightred"));
        else
            return QVariant::fromValue(QColor("lightblue"));
    }
    else if (role == AircraftModel::showAllRole)
        return QVariant::fromValue(m_aircrafts[row]->m_showAll);
    else if (role == AircraftModel::highlightedRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isHighlighted);
    else if (role == AircraftModel::targetRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isTarget);
    else if (role == AircraftModel::radiusRole)
        return QVariant::fromValue(m_aircrafts[row]->m_radius);
    else if (role == AircraftModel::aircraftPathModelRole)
        return QVariant::fromValue(m_pathModels[row]);
    return QVariant();
}

bool AircraftModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_aircrafts.count()))
        return false;
    if (role == AircraftModel::showAllRole)
    {
        bool showAll = value.toBool();
        if (showAll != m_aircrafts[row]->m_showAll)
        {
            m_aircrafts[row]->m_showAll = showAll;
            emit dataChanged(index, index);
        }
        return true;
    }
    else if (role == AircraftModel::highlightedRole)
    {
        bool highlight = value.toBool();
        if (highlight != m_aircrafts[row]->m_isHighlighted)
        {
            m_aircrafts[row]->m_gui->highlightAircraft(m_aircrafts[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    else if (role == AircraftModel::targetRole)
    {
        bool target = value.toBool();
        if (target != m_aircrafts[row]->m_isTarget)
        {
            m_aircrafts[row]->m_gui->targetAircraft(m_aircrafts[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    return true;
}

// The name the Map feature has this aircraft under, which is not always the name this
// demodulator chose. With an Aircraft feature present, ADSBDemodTracker::sendToMap sends
// the map item to that feature instead of to the Map, and it draws the aircraft under
// its registration so that an airframe heard on several protocols is one item. Asking
// for the ICAO address then finds nothing, which is what this fixes.
//
// Which case we are in is decided the same way sendToMap decides it - by whether anything
// is listening on the aircraftreport pipe - and the name itself comes from the rule both
// sides share, so the two cannot drift apart.
QString ADSBDemodGUI::mapItemName(const Aircraft *aircraft) const
{
    QList<ObjectPipe*> reportPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_adsbDemod, "aircraftreport", reportPipes);
    if (reportPipes.isEmpty()) {
        return aircraft->m_icaoHex;     // Drawn by this demodulator, under its own name
    }
    const QString registration = aircraft->m_aircraftInfo
        ? aircraft->m_aircraftInfo->m_registration : QString();
    return aircraftMapItemName(registration, aircraft->m_callsign, (quint32) aircraft->m_icao);
}

void AircraftModel::findOnMap(int index)
{
    if ((index < 0) || (index >= m_aircrafts.count())) {
        return;
    }
    const Aircraft *aircraft = m_aircrafts[index];
    FeatureWebAPIUtils::mapFind(aircraft->m_gui->mapItemName(aircraft));
}

// Get list of frequeny scanners to use in menu
QStringList AirportModel::getFreqScanners() const
{
    return MainCore::instance()->getChannelIds("sdrangel.channel.freqscanner");
}

// Send airport frequencies to frequency scanner with given id (Rn:n)
void AirportModel::sendToFreqScanner(int index, const QString& id)
{
    if ((index < 0) || (index >= m_airports.count())) {
        return;
    }
    const AirportInformation *airport = m_airports[index];
    unsigned int deviceSet, channelIndex;

    if (MainCore::getDeviceAndChannelIndexFromId(id, deviceSet, channelIndex))
    {
        QJsonArray array;
        for (const auto airportFrequency : airport->m_frequencies)
        {
            QJsonObject obj;
            QJsonValue frequency(airportFrequency->m_frequency * 1000000);
            QJsonValue enabled(1); // true doesn't work
            QJsonValue notes(QString("%1 %2").arg(airport->m_ident).arg(airportFrequency->m_description));
            obj.insert("frequency", frequency);
            obj.insert("enabled", enabled);
            obj.insert("notes", notes);
            QJsonValue value(obj);
            array.append(value);
        }
        ChannelWebAPIUtils::patchChannelSetting(deviceSet, channelIndex, "frequencies", array);
    }
    else
    {
        qDebug() << "AirportModel::sendToFreqScanner: Malformed id: " << id;
    }
}

QVariant AirportModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_airports.count()))
        return QVariant();
    if (role == AirportModel::positionRole)
    {
        // Coordinates to display the airport icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_airports[row]->m_latitude);
        coords.setLongitude(m_airports[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_airports[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == AirportModel::airportDataRole)
    {
        if (m_showFreq[row])
        {
            QString text = m_airportDataFreq[row];
            if (!m_metar[row].isEmpty()) {
                text = text + "\n" + m_metar[row];
            }
            return QVariant::fromValue(text);
        }
        else
            return QVariant::fromValue(m_airports[row]->m_ident);
    }
    else if (role == AirportModel::airportDataRowsRole)
    {
        if (m_showFreq[row])
        {
            int rows = m_airportDataFreqRows[row];
            if (!m_metar[row].isEmpty()) {
                rows += 1 + m_metar[row].count("\n");
            }
            return QVariant::fromValue(rows);
        }
        else
            return 1;
    }
    else if (role == AirportModel::airportImageRole)
    {
        // Select an image to use for the airport
        return QVariant::fromValue(m_airports[row]->getImageName());
    }
    else if (role == AirportModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the airport
        return QVariant::fromValue(QColor("lightyellow"));
    }
    else if (role == AirportModel::showFreqRole)
    {
        return QVariant::fromValue(m_showFreq[row]);
    }
    return QVariant();
}

bool AirportModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_airports.count()))
        return false;
    if (role == AirportModel::showFreqRole)
    {
        bool showFreq = value.toBool();
        if (showFreq != m_showFreq[row])
        {
            m_showFreq[row] = showFreq;
            emit dataChanged(index, index);
            if (showFreq) {
                emit requestMetar(m_airports[row]->m_ident);
            }
        }
        return true;
    }
    else if (role == AirportModel::selectedFreqRole)
    {
        int idx = value.toInt();
        if ((idx >= 0) && (idx < m_airports[row]->m_frequencies.size()))
        {
            // Do in two steps to avoid rounding errors
            qint64 freqkHz = (qint64) std::round(m_airports[row]->m_frequencies[idx]->m_frequency*1000.0);
            qint64 freqHz = freqkHz * 1000;
            m_gui->setFrequency(freqHz);
        }
        else if (idx == m_airports[row]->m_frequencies.size())
        {
            // Set airport as target
            m_gui->target(m_airports[row]->m_name, m_azimuth[row], m_elevation[row], m_range[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    return true;
}

QVariant AirspaceModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_airspaces.count())) {
        return QVariant();
    }
    if (role == AirspaceModel::nameRole)
    {
        // Airspace name
        return QVariant::fromValue(m_airspaces[row]->m_name);
    }
    else if (role == AirspaceModel::detailsRole)
    {
        // Airspace name and altitudes
        QString details;
        details.append(m_airspaces[row]->m_name);
        if (m_airspaces[row]->m_bottom.m_alt != -1)
        {
            details.append(QString("\n%1 - %2")
                .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_bottom))
                .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_top)));
        }
        return QVariant::fromValue(details);
    }
    else if (role == AirspaceModel::positionRole)
    {
        // Coordinates to display the airspace name at
        QGeoCoordinate coords;
        coords.setLatitude(m_airspaces[row]->m_position.y());
        coords.setLongitude(m_airspaces[row]->m_position.x());
        coords.setAltitude(m_airspaces[row]->topHeightInMetres());
        return QVariant::fromValue(coords);
    }
    else if (role == AirspaceModel::airspaceBorderColorRole)
    {
        if (m_airspaces[row]->m_name == "Coverage Map Low") {
            return QVariant::fromValue(QColor(0x00, 0xff, 0x00, 0x00));
        } else if (m_airspaces[row]->m_name == "Coverage Map High") {
            return QVariant::fromValue(QColor(0xff, 0xff, 0x00, 0x00));
        } else if ((m_airspaces[row]->m_category == "D")
            || (m_airspaces[row]->m_category == "G")
            || (m_airspaces[row]->m_category == "CTR")) {
            return QVariant::fromValue(QColor(0x00, 0x00, 0xff, 0x00));
        } else {
            return QVariant::fromValue(QColor(0xff, 0x00, 0x00, 0x00));
        }
    }
    else if (role == AirspaceModel::airspaceFillColorRole)
    {
        if (m_airspaces[row]->m_name.startsWith("IC")) {
            int ic = m_airspaces[row]->m_name.mid(3).toInt();
            int i = (ic & 0x3f) * 3;

            return QVariant::fromValue(QColor(colors[i], colors[i+1], colors[i+2], 0x40));
        } else if (m_airspaces[row]->m_name == "Coverage Map Low") {
            return QVariant::fromValue(QColor(0x00, 0xff, 0x00, 0x40));
        } else if (m_airspaces[row]->m_name == "Coverage Map High") {
            return QVariant::fromValue(QColor(0xff, 0xff, 0x00, 0x40));
        } else if ((m_airspaces[row]->m_category == "D")
            || (m_airspaces[row]->m_category == "G")
            || (m_airspaces[row]->m_category == "CTR")) {
            return QVariant::fromValue(QColor(0x00, 0x00, 0xff, 0x10));
        } else {
            return QVariant::fromValue(QColor(0xff, 0x00, 0x00, 0x10));
        }
    }
    else if (role == AirspaceModel::airspacePolygonRole)
    {
       return m_polygons[row];
    }
    return QVariant();
}

bool AirspaceModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    (void) value;
    (void) role;

    int row = index.row();
    if ((row < 0) || (row >= m_airspaces.count())) {
        return false;
    }
    return true;
}

QVariant NavAidModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_navAids.count())) {
        return QVariant();
    }
    if (role == NavAidModel::positionRole)
    {
        // Coordinates to display the VOR icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_navAids[row]->m_latitude);
        coords.setLongitude(m_navAids[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_navAids[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == NavAidModel::navAidDataRole)
    {
        // Create the text to go in the bubble next to the VOR
        if (m_selected[row])
        {
            QStringList list;
            list.append(QString("Name: %1").arg(m_navAids[row]->m_name));
            if (m_navAids[row]->m_type == "NDB") {
                list.append(QString("Frequency: %1 kHz").arg(m_navAids[row]->m_frequencykHz, 0, 'f', 1));
            } else {
                list.append(QString("Frequency: %1 MHz").arg(m_navAids[row]->m_frequencykHz / 1000.0f, 0, 'f', 2));
            }
            if (m_navAids[row]->m_channel != "") {
                list.append(QString("Channel: %1").arg(m_navAids[row]->m_channel));
            }
            list.append(QString("Ident: %1 %2").arg(m_navAids[row]->m_ident).arg(Morse::toSpacedUnicodeMorse(m_navAids[row]->m_ident)));
            list.append(QString("Range: %1 nm").arg(m_navAids[row]->m_range));
            if (m_navAids[row]->m_alignedTrueNorth) {
                list.append(QString("Magnetic declination: Aligned to true North"));
            } else if (m_navAids[row]->m_magneticDeclination != 0.0f) {
                list.append(QString("Magnetic declination: %1%2").arg(std::round(m_navAids[row]->m_magneticDeclination)).arg(QChar(0x00b0)));
            }
            QString data = list.join("\n");
            return QVariant::fromValue(data);
        }
        else
        {
            return QVariant::fromValue(m_navAids[row]->m_name);
        }
    }
    else if (role == NavAidModel::navAidImageRole)
    {
        // Select an image to use for the NavAid
        return QVariant::fromValue(QString("%1.png").arg(m_navAids[row]->m_type));
    }
    else if (role == NavAidModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the NavAid
        return QVariant::fromValue(QColor("lightgreen"));
    }
    else if (role == NavAidModel::selectedRole)
    {
        return QVariant::fromValue(m_selected[row]);
    }
    return QVariant();
}

bool NavAidModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_navAids.count())) {
        return false;
    }
    if (role == NavAidModel::selectedRole)
    {
        bool selected = value.toBool();
        m_selected[row] = selected;
        emit dataChanged(index, index);
        return true;
    }
    return true;
}

// Set selected AM Demod to the given frequency (used to tune to ATC selected from airports on map)
bool ADSBDemodGUI::setFrequency(qint64 targetFrequencyHz)
{
    unsigned int deviceSet, channelIndex;

    if (MainCore::getDeviceAndChannelIndexFromId(m_settings.m_amDemod, deviceSet, channelIndex))
    {
        const int halfChannelBW = 20000/2;
        int dcOffset = halfChannelBW;

        double centerFrequency;
        int sampleRate;

        if (ChannelWebAPIUtils::getCenterFrequency(deviceSet, centerFrequency))
        {
            if (ChannelWebAPIUtils::getDevSampleRate(deviceSet, sampleRate))
            {
                sampleRate *= 0.75; // Don't use guard bands

                // Adjust device center frequency if not in range
                if (   ((targetFrequencyHz - halfChannelBW) < (centerFrequency - sampleRate / 2))
                    || ((targetFrequencyHz + halfChannelBW) >= (centerFrequency + sampleRate / 2))
                   )
                {
                    ChannelWebAPIUtils::setCenterFrequency(deviceSet, targetFrequencyHz - dcOffset);
                    ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, dcOffset);
                }
                else
                {
                    qint64 offset = targetFrequencyHz - centerFrequency;
                    // Also adjust center frequency if channel would cross DC
                    if (std::abs(offset) < halfChannelBW)
                    {
                        ChannelWebAPIUtils::setCenterFrequency(deviceSet, targetFrequencyHz - dcOffset);
                        ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, dcOffset);
                    }
                    else
                    {
                        // Just tune channel
                        ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, offset);
                    }
                }

                return true;
            }
        }
    }
    return false;
}









void ADSBDemodGUI::Interogator::update(int ic, Aircraft *aircraft, AirspaceModel *airspaceModel, CheckList *checkList, bool display)
{
    if (aircraft->m_positionValid)
    {
        if (m_valid)
        {
            bool changed = false;

            if (aircraft->m_latitude < m_minLatitude)
            {
                m_minLatitude = aircraft->m_latitude;
                changed = true;
            }
            if (aircraft->m_latitude > m_maxLatitude)
            {
                m_maxLatitude = aircraft->m_latitude;
                changed = true;
            }
            if (aircraft->m_longitude < m_minLongitude)
            {
                m_minLongitude = aircraft->m_longitude;
                changed = true;
            }
            if (aircraft->m_longitude > m_maxLongitude)
            {
                m_maxLongitude = aircraft->m_longitude;
                changed = true;
            }
            if (changed)
            {
                calcPoly();
                if (display) {
                    airspaceModel->airspaceUpdated(&m_airspace);
                }
            }
        }
        else
        {
            const double minSize = 0.02;
            QBrush brush(QColor(colors[ic*3], colors[ic*3+1], colors[ic*3+2], 0xa0));

            m_valid = true;
            m_minLatitude = aircraft->m_latitude - minSize;
            m_maxLatitude = aircraft->m_latitude + minSize;
            m_minLongitude = aircraft->m_longitude - minSize;
            m_maxLongitude = aircraft->m_longitude + minSize;
            m_airspace.m_name = QString("IC %1").arg(ic);
            m_airspace.m_bottom.m_alt = -1;
            m_airspace.m_top.m_alt = -1;
            calcPoly();
            if (display)
            {
                airspaceModel->addAirspace(&m_airspace);
                checkList->addCheckItem(QString::number(ic), ic, Qt::Checked)->setBackground(brush);
            }
            else
            {
                checkList->addCheckItem(QString::number(ic), ic, Qt::Unchecked)->setBackground(brush);
            }
            checkList->setSortRole(Qt::UserRole + 1); // Sort via data rather than label
            checkList->model()->sort(0, Qt::AscendingOrder);
        }
    }
}

void ADSBDemodGUI::Interogator::calcPoly()
{
    double w = (m_maxLongitude - m_minLongitude) / 2.0;
    double h = (m_maxLatitude - m_minLatitude) / 2.0;
    double x = m_minLongitude + w;
    double y = m_minLatitude + h;
    m_airspace.m_center.setX(x);
    m_airspace.m_center.setY(y);
    m_airspace.m_position = m_airspace.m_center;

    const int s = 15;
    m_airspace.m_polygon.resize(360/s+1);

    for (int d = 0, i = 0; d <= 360; d += s, i++)
    {
        double a = Units::degreesToRadians((double) d);
        double r1 = cos(a) * w;
        double r2 = sin(a) * h;

        m_airspace.m_polygon[i] = QPointF(x + r1, y + r2);
    }
}





















void ADSBDemodGUI::enableSpeechIfNeeded()
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        return;
    }
    for (const auto& notification : m_settings.m_notificationSettings)
    {
        if (!notification->m_speech.isEmpty())
        {
            qDebug() << "ADSBDemodGUI: Enabling text to speech";
            m_speech = new QTextToSpeech(this);
            return;
        }
    }
#endif
}

void ADSBDemodGUI::speechRequest(const QString &speech)
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        m_speech->say(speech);
    } else {
        qWarning() << "ADSBDemodGUI::speechNotification: Unable to say " << speech;
    }
#else
    qWarning() << "ADSBDemodGUI::speechNotification: TextToSpeech not supported. Unable to say " << speech;
#endif
}



bool ADSBDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        int sr = notif.getSampleRate();
        bool srTooLow = sr < 2000000;
        ui->warning->setVisible(srTooLow);
        if (srTooLow) {
            ui->warning->setText(QString("Sample rate must be >= 2000000. Currently %1").arg(sr));
        } else {
            ui->warning->setText("");
        }
        getRollupContents()->arrangeRollups();
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = sr;
        ui->deltaFrequency->setValueRange(false, 7, -sr/2, sr/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(sr/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (ADSBDemodReport::MsgReportDemodStats::match(message))
    {
        ADSBDemodReport::MsgReportDemodStats& report = (ADSBDemodReport::MsgReportDemodStats&) message;
        ADSBDemodStats stats = report.getDemodStats();

        ui->statsTable->item(ADSB_FRAMES, 0)->setData(Qt::DisplayRole, stats.m_adsbFrames);
        ui->statsTable->item(MODE_S_FRAMES, 0)->setData(Qt::DisplayRole, stats.m_modesFrames);
        int totalFrames = stats.m_adsbFrames + stats.m_modesFrames;
        ui->statsTable->item(TOTAL_FRAMES, 0)->setData(Qt::DisplayRole, totalFrames);
        ui->statsTable->item(CORRELATOR_MATCHES, 0)->setData(Qt::DisplayRole, stats.m_correlatorMatches);
        float percentValid = 100.0f * totalFrames / (float) (stats.m_correlatorMatches - stats.m_preambleFails);
        ui->statsTable->item(PERCENT_VALID, 0)->setData(Qt::DisplayRole, QString::number(percentValid, 'f', 2));
        ui->statsTable->item(PREAMBLE_FAILS, 0)->setData(Qt::DisplayRole, stats.m_preambleFails);
        ui->statsTable->item(CRC_FAILS, 0)->setData(Qt::DisplayRole, stats.m_crcFails);
        ui->statsTable->item(TYPE_FAILS, 0)->setData(Qt::DisplayRole, stats.m_typeFails);
        ui->statsTable->item(INVALID_FAILS, 0)->setData(Qt::DisplayRole, stats.m_invalidFails);
        ui->statsTable->item(ICAO_FAILS, 0)->setData(Qt::DisplayRole, stats.m_icaoFails);
        return true;
    }
    else if (ADSBDemod::MsgConfigureADSBDemod::match(message))
    {
        qDebug("ADSBDemodGUI::handleMessage: ADSBDemod::MsgConfigureADSBDemod");
        const ADSBDemod::MsgConfigureADSBDemod& cfg = (ADSBDemod::MsgConfigureADSBDemod&) message;
        const ADSBDemodSettings settings = cfg.getSettings();
        const QStringList settingsKeys = cfg.getSettingsKeys();
        bool force = cfg.getForce();

        if (force) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(settingsKeys, settings);
        }

        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings(settingsKeys, force);
        blockApplySettings(false);
        return true;
    }

    return false;
}

void ADSBDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void ADSBDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void ADSBDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ADSBDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void ADSBDemodGUI::on_rfBW_valueChanged(int value)
{
    Real bw = (Real)value;
    ui->rfBWText->setText(QString("%1M").arg(bw / 1000000.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySetting("rfBandwidth");
}

void ADSBDemodGUI::on_threshold_valueChanged(int value)
{
    Real thresholddB = ((Real)value)/10.0f;
    ui->thresholdText->setText(QString("%1").arg(thresholddB, 0, 'f', 1));
    m_settings.m_correlationThreshold = thresholddB;
    applySetting("correlationThreshold");
}

void ADSBDemodGUI::on_chipsThreshold_valueChanged(int value)
{
    ui->chipsThresholdText->setText(QString("%1").arg(value));
    m_settings.m_chipsThreshold = value;
    applySetting("chipsThreshold");
}

void ADSBDemodGUI::on_phaseSteps_valueChanged(int value)
{
    ui->phaseStepsText->setText(QString("%1").arg(value));
    m_settings.m_interpolatorPhaseSteps = value;
    applySetting("interpolatorPhaseSteps");
}

void ADSBDemodGUI::on_tapsPerPhase_valueChanged(int value)
{
    Real tapsPerPhase = ((Real)value)/10.0f;
    ui->tapsPerPhaseText->setText(QString("%1").arg(tapsPerPhase, 0, 'f', 1));
    m_settings.m_interpolatorTapsPerPhase = tapsPerPhase;
    applySetting("interpolatorTapsPerPhase");
}

void ADSBDemodGUI::on_feed_clicked(bool checked)
{
    m_settings.m_feedEnabled = checked;
    // Don't disable host/port - so they can be entered before connecting
    applySetting("feedEnabled");
}

void ADSBDemodGUI::on_notifications_clicked()
{
    ADSBDemodNotificationDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        enableSpeechIfNeeded();
        applySetting("notificationSettings");
    }
}

void ADSBDemodGUI::on_flightInfo_clicked()
{
    if (!m_settings.m_aviationstackAPIKey.isEmpty())
    {
        // Selection mode is single, so only a single row should be returned
        QModelIndexList indexList = ui->adsbData->selectionModel()->selectedRows();
        if (!indexList.isEmpty())
        {
            Aircraft *aircraft = aircraftForViewRow(indexList.at(0).row());
            if (aircraft)
            {
                if (!aircraft->m_flight.isEmpty())
                {
                    // Download flight information
                    QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "requestFlightInformation",
                                              Qt::QueuedConnection, Q_ARG(QString, aircraft->m_flight));
                }
                else
                {
                    qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No flight number for selected aircraft";
                }
            }
            else
            {
                qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No aircraft for the selected row";
            }
        }
        else
        {
            qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No aircraft selected";
        }
    }
    else
    {
        qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No flight information service - have you set an API key?";
    }
}

// Find highlighted aircraft on Map Feature
void ADSBDemodGUI::on_findOnMapFeature_clicked()
{
    QModelIndexList indexList = ui->adsbData->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        Aircraft *aircraft = aircraftForViewRow(indexList.at(0).row());
        if (aircraft) {
            FeatureWebAPIUtils::mapFind(mapItemName(aircraft));
        }
    }
}

void ADSBDemodGUI::on_deleteAircraft_clicked()
{
    QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "deleteAllAircraft", Qt::QueuedConnection);
}

// Take an aircraft out of the table and off the map. The tracker decides when
void ADSBDemodGUI::removeAircraft(QHash<int, Aircraft *>::iterator& i, Aircraft *aircraft)
{
    if (m_trackAircraft == aircraft)
    {
        m_adsbDemod->clearTarget();
        m_trackAircraft = nullptr;
    }
    if (m_highlightAircraft == aircraft) {
        m_highlightAircraft = nullptr;
    }

    m_aircraftModel.removeAircraft(aircraft);
    m_aircraftTableModel.removeAircraft(aircraft);
    i = m_aircraft.erase(i);
    delete aircraft;
}

// Everything the tracker decoded since the last flush
void ADSBDemodGUI::aircraftUpdated(const QList<ADSBAircraftUpdate>& updates)
{
    PROFILER_START();
    ui->adsbData->setUpdatesEnabled(false);

    for (const auto& update : updates)
    {
        const int icao = update.m_state.m_icao;
        Aircraft *aircraft = m_aircraft.value(icao, nullptr);
        bool isNew = false;

        if (!aircraft)
        {
            aircraft = new Aircraft(this);
            m_aircraft.insert(icao, aircraft);
            isNew = true;
        }

        const bool hadPosition = aircraft->m_positionValid;
        const bool trailCleared = update.m_state.m_trailCleared;
        const bool positionChanged = update.m_state.m_positionChanged;

        // Copy the decoded state over, leaving the display state alone
        *static_cast<ADSBAircraft *>(aircraft) = update.m_state;

        if (isNew)
        {
            m_aircraftTableModel.addAircraft(aircraft);
            if (m_settings.m_autoResizeTableColumns) {
                ui->adsbData->resizeColumnsToContents();
            }
        }

        if (trailCleared) {
            aircraft->clearCoordinates(&m_aircraftModel);
        }
        if (!aircraft->m_positionUpdates.isEmpty())
        {
            for (const ADSBAircraftPosition& position : aircraft->m_positionUpdates) {
                aircraft->addCoordinate(position, &m_aircraftModel);
            }
            aircraft->m_positionUpdates.clear();
        }
        else if (positionChanged)
        {
            // Compatibility fallback for a position update without a sample.
            aircraft->addCoordinate(
                ADSBAircraftPosition(
                    aircraft->m_latitude,
                    aircraft->m_longitude,
                    aircraft->m_altitude,
                    aircraft->m_positionDateTime),
                &m_aircraftModel);
        }

        if (aircraft->m_positionValid)
        {
            if (!hadPosition) {
                m_aircraftModel.addAircraft(aircraft);
            } else {
                m_aircraftModel.aircraftUpdated(aircraft);
            }
        }

        // The interrogator coverage boxes are drawn from the aircraft positions
        const QVariant ic = aircraft->cell(ADSB_COL_IC);
        if (!ic.isNull() && (ic.toInt() > 0)) {
            m_interogators[ic.toInt() - 1].update(ic.toInt(), aircraft, &m_airspaceModel, ui->ic, m_settings.m_displayIC[ic.toInt() - 1]);
        }
    }

    m_aircraftTableModel.flushDirty();
    ui->adsbData->setUpdatesEnabled(true);
    PROFILER_STOP("ADS-B table update");
}

void ADSBDemodGUI::aircraftRemoved(const QList<quint32>& icaos)
{
    for (quint32 icao : icaos)
    {
        QHash<int, Aircraft *>::iterator i = m_aircraft.find((int) icao);
        if (i != m_aircraft.end()) {
            removeAircraft(i, i.value());
        }
    }
}

void ADSBDemodGUI::statsUpdated(const QHash<int, QVariant>& stats)
{
    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        if ((i.key() >= 0) && (i.key() < ui->statsTable->rowCount())) {
            ui->statsTable->item(i.key(), 0)->setData(Qt::DisplayRole, i.value());
        }
    }
}

void ADSBDemodGUI::coverageUpdated(int band, const QVector<QPointF>& polygon)
{
    if ((band >= 0) && (band < 2))
    {
        m_coverageAirspace[band].m_polygon = polygon;
        m_airspaceModel.airspaceUpdated(&m_coverageAirspace[band]);
    }
}

void ADSBDemodGUI::qnhEstimate(int qnh)
{
    if (!m_settings.m_manualQNH) {
        ui->qnh->setValue(qnh);
    }
}

void ADSBDemodGUI::highlightRequest(quint32 icao)
{
    Aircraft *aircraft = findAircraft(icao);
    if (aircraft) {
        highlightAircraft(aircraft);
    }
}

void ADSBDemodGUI::targetRequest(quint32 icao)
{
    Aircraft *aircraft = findAircraft(icao);
    if (aircraft) {
        targetAircraft(aircraft);
    }
}

void ADSBDemodGUI::photoRefresh(quint32 icao)
{
    Aircraft *aircraft = findAircraft(icao);
    if (aircraft && (aircraft == m_highlightAircraft))
    {
        updatePhotoText(aircraft);
        updatePhotoFlightInformation(aircraft);
    }
}

// Find aircraft on channel map
void ADSBDemodGUI::findOnChannelMap(Aircraft *aircraft)
{
#ifdef QT_LOCATION_FOUND
    if (aircraft->m_positionValid)
    {
        QQuickItem *item = ui->map->rootObject();
        QObject *object = item->findChild<QObject*>("map");
        if(object != NULL)
        {
            QGeoCoordinate geocoord = object->property("center").value<QGeoCoordinate>();
            geocoord.setLatitude(aircraft->m_latitude);
            geocoord.setLongitude(aircraft->m_longitude);
            object->setProperty("center", QVariant::fromValue(geocoord));
        }
    }
#endif
}

void ADSBDemodGUI::statsTable_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->statsTable->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(ui->statsTable);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
            });
        tableContextMenu->addAction(copyAction);
    }
}

void ADSBDemodGUI::adsbData_customContextMenuRequested(QPoint pos)
{
    QModelIndex index = ui->adsbData->indexAt(pos);
    if (index.isValid())
    {
        Aircraft *aircraft = aircraftForViewRow(index.row());
        if (!aircraft) {
            return;
        }
        const int icao = aircraft->m_icao;
        const QString icaoHex = aircraft->m_icaoHex;

        QMenu* tableContextMenu = new QMenu(ui->adsbData);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = ui->adsbData->model()->data(index, Qt::DisplayRole).toString();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->addSeparator();

        // View aircraft on various websites

        QAction* planeSpottersAction = new QAction("View aircraft on planespotters.net...", tableContextMenu);
        connect(planeSpottersAction, &QAction::triggered, this, [icao]()->void {
            QString icaoUpper = QString("%1").arg(icao, 1, 16).toUpper();
            QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
        });
        tableContextMenu->addAction(planeSpottersAction);

        QAction* adsbExchangeAction = new QAction("View aircraft on adsbexchange.com...", tableContextMenu);
        connect(adsbExchangeAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://globe.adsbexchange.com/?icao=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(adsbExchangeAction);

        QAction* viewOpenSkyAction = new QAction("View aircraft on opensky-network.org...", tableContextMenu);
        connect(viewOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://map.opensky-network.org/?icao=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(viewOpenSkyAction);

        if (!aircraft->m_callsign.isEmpty())
        {
            QAction* flightRadarAction = new QAction("View flight on flightradar24.com...", tableContextMenu);
            connect(flightRadarAction, &QAction::triggered, this, [aircraft]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_callsign)));
            });
            tableContextMenu->addAction(flightRadarAction);
        }

        tableContextMenu->addSeparator();

        // Edit aircraft

        /*if (!aircraft->m_aircraftInfo)
        {
            QAction* addOpenSkyAction = new QAction("Add aircraft to opensky-network.org...", tableContextMenu);
            connect(addOpenSkyAction, &QAction::triggered, this, []()->void {
                QDesktopServices::openUrl(QUrl(QString("https://old.opensky-network.org/edit-aircraft-profile")));
            });
            tableContextMenu->addAction(addOpenSkyAction);
        }
        else
        {

            QAction* editOpenSkyAction = new QAction("Edit aircraft on opensky-network.org...", tableContextMenu);
            connect(editOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://old.opensky-network.org/edit-aircraft-profile?icao24=%1").arg(icaoHex)));
            });
            tableContextMenu->addAction(editOpenSkyAction);
        }*/

        QAction* editSDMAction = new QAction("Edit aircraft on sdm.virtualradarserver.co.uk...", tableContextMenu);
        connect(editSDMAction, &QAction::triggered, this, []()->void {
            QDesktopServices::openUrl(QUrl(QString("https://sdm.virtualradarserver.co.uk/Edit/Aircraft")));
            });
        tableContextMenu->addAction(editSDMAction);

        // Find on Map
        if (aircraft->m_positionValid)
        {
            tableContextMenu->addSeparator();

            QAction* findChannelMapAction = new QAction("Find on ADS-B map", tableContextMenu);
            connect(findChannelMapAction, &QAction::triggered, this, [this, aircraft]()->void {
                findOnChannelMap(aircraft);
            });
            tableContextMenu->addAction(findChannelMapAction);

            QAction* findMapFeatureAction = new QAction("Find on feature map", tableContextMenu);
            connect(findMapFeatureAction, &QAction::triggered, this, [this, aircraft]()->void {
                FeatureWebAPIUtils::mapFind(mapItemName(aircraft));
            });
            tableContextMenu->addAction(findMapFeatureAction);
        }

        tableContextMenu->popup(ui->adsbData->viewport()->mapToGlobal(pos));
    }
}

void ADSBDemodGUI::adsbData_clicked(const QModelIndex& index)
{
    Aircraft *aircraft = aircraftForViewRow(index.row());
    if (aircraft) {
        highlightAircraft(aircraft);
    }
}

// The aircraft shown in a row of the table. Rows are in the proxy's order, which is
// what the user sees and what the view's signals report
Aircraft *ADSBDemodGUI::aircraftForViewRow(int viewRow) const
{
    QModelIndex index = m_aircraftProxy->mapToSource(m_aircraftProxy->index(viewRow, 0));
    return index.isValid() ? m_aircraftTableModel.aircraftAt(index.row()) : nullptr;
}

void ADSBDemodGUI::adsbData_doubleClicked(const QModelIndex& modelIndex)
{
    Aircraft *aircraft = aircraftForViewRow(modelIndex.row());
    if (!aircraft) {
        return;
    }
    const int icao = aircraft->m_icao;
    const int column = modelIndex.column();

    if (column == ADSB_COL_ICAO)
    {
        // Search for aircraft on planespotters.net
        QString icaoUpper = QString("%1").arg(icao, 1, 16).toUpper();
        QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
    }
    else if (m_aircraft.contains(icao))
    {
        Aircraft *aircraft = m_aircraft.value(icao);

        if (column == ADSB_COL_CALLSIGN)
        {
            if (!aircraft->m_callsign.isEmpty())
            {
                // Search for flight on flightradar24
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_callsign)));
            }
        }
        else
        {
            if (column == ADSB_COL_AZEL)
            {
                targetAircraft(aircraft);
            }
            // Center map view on aircraft if it has a valid position
            if (aircraft->m_positionValid)
            {
                findOnChannelMap(aircraft);
            }
        }
    }
}

// Columns in table reordered
void ADSBDemodGUI::adsbData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;
    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void ADSBDemodGUI::adsbData_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;
    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in ADSB table header - show column select menu
void ADSBDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->adsbData->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void ADSBDemodGUI::columnSelectMenuChecked(bool checked)
{
    (void) checked;
    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->adsbData->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *ADSBDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

void ADSBDemodGUI::on_spb_currentIndexChanged(int value)
{
    m_settings.m_samplesPerBit = (value + 1) * 2;
    applySetting("samplesPerBi");
}

void ADSBDemodGUI::on_demodModeS_clicked(bool checked)
{
    m_settings.m_demodModeS = checked;
    applySetting("demodModeS");
}

void ADSBDemodGUI::on_getAircraftDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_osnDB.downloadAircraftInformation();
    }
}

void ADSBDemodGUI::on_getAirportDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_ourAirportsDB.downloadAirportInformation();
    }
}

void ADSBDemodGUI::on_getAirspacesDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setMaximum(OpenAIP::m_countryCodes.size());
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_openAIP.downloadAirspaces();
    }
}

void ADSBDemodGUI::on_coverage_clicked(bool checked)
{
    m_settings.m_displayCoverage = checked;
    applySetting("displayCoverage");
    updateAirspaces();
}

void ADSBDemodGUI::on_displayChart_clicked(bool checked)
{
    m_settings.m_displayChart = checked;
    applySetting("displayChart");
    ui->chart->setVisible(m_settings.m_displayChart);
}

void ADSBDemodGUI::on_stats_clicked(bool checked)
{
    m_settings.m_displayDemodStats = checked;
    ui->statsTable->setVisible(m_settings.m_displayDemodStats);
    applySetting("displayDemodStats");
}

void ADSBDemodGUI::on_ic_globalCheckStateChanged(int state)
{
    (void) state;

    for (int i = 0; i < ui->ic->count(); i++)
    {
        int ic = ui->ic->itemText(i).toInt();
        int idx = ic - 1;
        bool checked = ui->ic->isChecked(i);
        m_settings.m_displayIC[idx] = checked;
        bool visible = m_airspaceModel.contains(&m_interogators[idx].m_airspace);
        if (checked && !visible) {
            m_airspaceModel.addAirspace(&m_interogators[idx].m_airspace);
        } else if (!checked && visible) {
            m_airspaceModel.removeAirspace(&m_interogators[idx].m_airspace);
        }
    }
}

void ADSBDemodGUI::on_flightPaths_clicked(bool checked)
{
    m_settings.m_flightPaths = checked;
    m_aircraftModel.setSettings(&m_settings);
}

void ADSBDemodGUI::on_allFlightPaths_clicked(bool checked)
{
    m_settings.m_allFlightPaths = checked;
    m_aircraftModel.setSettings(&m_settings);
}

void ADSBDemodGUI::on_atcLabels_clicked(bool checked)
{
    m_settings.m_atcLabels = checked;
    m_aircraftModel.setSettings(&m_settings);
    applySetting("atcLabels");
}

void ADSBDemodGUI::on_displayOrientation_clicked(bool checked)
{
    m_settings.m_displayOrientation = checked;
    applySetting("displayOrientation");
    ui->splitter->setOrientation(m_settings.m_displayOrientation ? Qt::Horizontal : Qt::Vertical);
}

void ADSBDemodGUI::on_displayRadius_clicked(bool checked)
{
    m_settings.m_displayRadius = checked;
    setShowContainmentRadius(m_settings.m_displayRadius);
    applySetting("displayRadius");
}

QString ADSBDemodGUI::getDataDir()
{
    // Get directory to store app data in (aircraft & airport databases and user-definable icons)
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

void ADSBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

void ADSBDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_adsbDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        QStringList settingsKeys({
           "rgbColor",
           "title",
           "useReverseAPI",
           "reverseAPIAddress",
           "reverseAPIPort",
           "reverseAPIDeviceIndex",
           "reverseAPIChannelIndex"
            });

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

void ADSBDemodGUI::updateChannelList()
{
    std::vector<ChannelAPI*> channels = MainCore::instance()->getChannels("sdrangel.channel.amdemod");

    ui->amDemod->blockSignals(true);
    ui->amDemod->clear();

    for (const auto channel : channels) {
        ui->amDemod->addItem(QString("R%1:%2").arg(channel->getDeviceSetIndex()).arg(channel->getIndexInDeviceSet()));
    }

    // Select current setting, if exists
    // If not, make sure nothing selected, as channel may be created later on
    int idx = ui->amDemod->findText(m_settings.m_amDemod);
    if (idx >= 0) {
        ui->amDemod->setCurrentIndex(idx);
    } else {
        ui->amDemod->setCurrentIndex(-1);
    }

    ui->amDemod->blockSignals(false);

    // If no current setting, select first channel
    if (m_settings.m_amDemod.isEmpty())
    {
        ui->amDemod->setCurrentIndex(0);
        on_amDemod_currentIndexChanged(0);
    }
}

void ADSBDemodGUI::on_amDemod_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_amDemod = ui->amDemod->currentText();
        applySetting("amDemod");
    }
}






void ADSBDemodGUI::updateAirports()
{
    if (!m_airportInfo) {
        return;
    }

    m_airportModel.removeAllAirports();
    QHash<int, AirportInformation *>::const_iterator i = m_airportInfo->begin();
    AzEl azEl = m_azEl;

    while (i != m_airportInfo->end())
    {
        const AirportInformation *airportInfo = i.value();

        // Calculate distance and az/el to airport from My Position
        azEl.setTarget(airportInfo->m_latitude, airportInfo->m_longitude, Units::feetToMetres(airportInfo->m_elevation));
        azEl.calculate();

        // Only display airport if in range
        if (azEl.getDistance() <= m_settings.m_airportRange*1000.0f)
        {
            // Only display the airport if it's large enough
            if (airportInfo->m_type >= (AirportInformation::AirportType)m_settings.m_airportMinimumSize)
            {
                // Only display heliports if enabled
                if (m_settings.m_displayHeliports || (airportInfo->m_type != AirportInformation::AirportType::Heliport))
                {
                    m_airportModel.addAirport(airportInfo, azEl.getAzimuth(), azEl.getElevation(), azEl.getDistance());
                }
            }
        }
        ++i;
    }
    // Save settings we've just used so we know if they've changed
    m_currentAirportMinimumSize = m_settings.m_airportMinimumSize;
    m_currentDisplayHeliports = m_settings.m_displayHeliports;
}

void ADSBDemodGUI::updateAirspaces()
{
    AzEl azEl = m_azEl;
    m_airspaceModel.removeAllAirspaces();
    for (const auto airspace: *m_airspaces)
    {
        if (m_settings.m_airspaces.contains(airspace->m_category))
        {
            // Calculate distance to airspace from My Position
            azEl.setTarget(airspace->m_center.y(), airspace->m_center.x(), 0);
            azEl.calculate();

            // Only display airport if in range
            if (azEl.getDistance() <= m_settings.m_airspaceRange*1000.0f) {
                m_airspaceModel.addAirspace(airspace);
            }
        }
    }
    if (m_settings.m_displayCoverage)
    {
        m_airspaceModel.addAirspace(&m_coverageAirspace[0]);
        m_airspaceModel.addAirspace(&m_coverageAirspace[1]);
    }
    for (int i = 0; i < ADSB_IC_MAX; i++) {
        if (m_interogators[i].m_valid && m_settings.m_displayIC[i]) {
            m_airspaceModel.addAirspace(&m_interogators[i].m_airspace);
        }
    }
}

void ADSBDemodGUI::updateNavAids()
{
    AzEl azEl = m_azEl;
    m_navAidModel.removeAllNavAids();
    if (m_settings.m_displayNavAids)
    {
        for (const auto navAid: *m_navAids)
        {
            // Calculate distance to NavAid from My Position
            azEl.setTarget(navAid->m_latitude, navAid->m_longitude, Units::feetToMetres(navAid->m_elevation));
            azEl.calculate();

            // Only display NavAid if in range
            if (azEl.getDistance() <= m_settings.m_airspaceRange*1000.0f) {
                m_navAidModel.addNavAid(navAid);
            }
        }
    }
}

// Set a static target, such as an airport
void ADSBDemodGUI::target(const QString& name, float az, float el, float range)
{
    if (m_trackAircraft)
    {
        // Restore colour of current target
        m_trackAircraft->m_isTarget = false;
        m_aircraftModel.aircraftUpdated(m_trackAircraft);
        m_trackAircraft = nullptr;
    }
    m_adsbDemod->setTarget(name, az, el, range);
}

void ADSBDemodGUI::targetAircraft(Aircraft *aircraft)
{
    if (aircraft != m_trackAircraft)
    {
        if (m_trackAircraft)
        {
            // Restore colour of current target
            m_trackAircraft->m_isTarget = false;
            m_aircraftModel.aircraftUpdated(m_trackAircraft);
        }
        // Track this aircraft. The tracker sends the rotator updated positions
        m_trackAircraft = aircraft;
        QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "setTrackAircraft", Qt::QueuedConnection,
                                  Q_ARG(quint32, (quint32) aircraft->m_icao));
        if (aircraft->m_positionValid)
            m_adsbDemod->setTarget(aircraft->targetName(), aircraft->m_azimuth, aircraft->m_elevation, aircraft->m_range);
        // Change colour of new target
        aircraft->m_isTarget = true;
        m_aircraftModel.aircraftUpdated(aircraft);
    }
}

void ADSBDemodGUI::updatePhotoText(Aircraft *aircraft)
{
    if (m_settings.m_displayPhotos)
    {
        QString callsign = aircraft->cellText(ADSB_COL_CALLSIGN).trimmed();
        QString reg = aircraft->cellText(ADSB_COL_REGISTRATION).trimmed();
        if (!callsign.isEmpty() && !reg.isEmpty()) {
            ui->photoHeader->setText(QString("%1 - %2").arg(callsign).arg(reg));
        } else if (!callsign.isEmpty()) {
            ui->photoHeader->setText(QString("%1").arg(callsign));
        } else if (!reg.isEmpty()) {
            ui->photoHeader->setText(QString("%1").arg(reg));
        }

        QIcon icon = ADSBAircraftTableModel::icon(aircraft->m_flagIconPath, QSize(40, 20)).value<QIcon>();
        QList<QSize> sizes = icon.availableSizes();
        if (sizes.size() > 0) {
            ui->photoFlag->setPixmap(icon.pixmap(sizes[0]));
        } else {
            ui->photoFlag->setPixmap(QPixmap());
        }

        updatePhotoFlightInformation(aircraft);

        QString aircraftDetails = "<table width=200>"; // Note, Qt seems to make the table bigger than this so text is cropped, not wrapped
        QString manufacturer = aircraft->cellText(ADSB_COL_MANUFACTURER);
        if (!manufacturer.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Manufacturer:<td>%1").arg(manufacturer));
        }
        QString model = aircraft->cellText(ADSB_COL_MODEL);
        if (!model.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Aircraft:<td>%1").arg(model));
        }
        QString owner = aircraft->cellText(ADSB_COL_OWNER);
        if (!owner.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Owner:<td>%1").arg(owner));
        }
        QString operatorICAO = aircraft->cellText(ADSB_COL_OPERATOR_ICAO);
        if (!operatorICAO.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Operator:<td>%1").arg(operatorICAO));
        }
        QString registered = aircraft->cellText(ADSB_COL_REGISTERED);
        if (!registered.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Registered:<td>%1").arg(registered));
        }
        aircraftDetails.append("</table>");
        ui->aircraftDetails->setText(aircraftDetails);
    }
}

void ADSBDemodGUI::updatePhotoFlightInformation(Aircraft *aircraft)
{
    if (m_settings.m_displayPhotos)
    {
        QString dep = aircraft->cellText(ADSB_COL_DEP);
        QString arr = aircraft->cellText(ADSB_COL_ARR);
        QString std = aircraft->cellText(ADSB_COL_STD);
        QString etd = aircraft->cellText(ADSB_COL_ETD);
        QString atd = aircraft->cellText(ADSB_COL_ATD);
        QString sta = aircraft->cellText(ADSB_COL_STA);
        QString eta = aircraft->cellText(ADSB_COL_ETA);
        QString ata = aircraft->cellText(ADSB_COL_ATA);
        QString flightDetails;
        if (!dep.isEmpty() && !arr.isEmpty())
        {
            flightDetails = QString("<center><table width=200><tr><th colspan=4>%1 - %2").arg(dep).arg(arr);
            if (!std.isEmpty() && !sta.isEmpty()) {
                flightDetails.append(QString("<tr><td>STD<td>%1<td>STA<td>%2").arg(std).arg(sta));
            }
            if ((!atd.isEmpty() || !etd.isEmpty()) && (!ata.isEmpty() || !eta.isEmpty()))
            {
                if (!atd.isEmpty()) {
                    flightDetails.append(QString("<tr><td>Actual<td>%1").arg(atd));
                } else if (!etd.isEmpty()) {
                    flightDetails.append(QString("<tr><td>Estimated<td>%1").arg(etd));
                }
                if (!ata.isEmpty()) {
                    flightDetails.append(QString("<td>Actual<td>%1").arg(ata));
                } else if (!eta.isEmpty()) {
                    flightDetails.append(QString("<td>Estimated<td>%1").arg(eta));
                }
            }
            flightDetails.append("</center>");
        }
        ui->flightDetails->setText(flightDetails);
    }
}

void ADSBDemodGUI::highlightAircraft(Aircraft *aircraft)
{
    if (aircraft != m_highlightAircraft)
    {
        // Hide photo of old aircraft
        ui->photoHeader->setVisible(false);
        ui->photoFlag->setVisible(false);
        ui->photo->setVisible(false);
        ui->flightDetails->setVisible(false);
        ui->aircraftDetails->setVisible(false);
        if (m_highlightAircraft)
        {
            // Restore colour
            m_highlightAircraft->m_isHighlighted = false;
            m_aircraftModel.highlightChanged(m_highlightAircraft);
        }
        // Highlight this aircraft
        m_highlightAircraft = aircraft;
        if (aircraft)
        {
            aircraft->m_isHighlighted = true;
            m_aircraftModel.highlightChanged(aircraft);
            if (m_settings.m_displayPhotos)
            {
                // Download photo
                updatePhotoText(aircraft);
                m_planeSpotters.getAircraftPhoto(aircraft->m_icaoHex);
            }
        }
    }
    if (aircraft)
    {
        // Highlight the row in the table - always do this, as it can become
        // unselected
        int row = m_aircraftTableModel.rowForAircraft(aircraft);
        if (row >= 0) {
            ui->adsbData->selectRow(m_aircraftProxy->mapFromSource(m_aircraftTableModel.index(row, 0)).row());
        }
    }
    else
    {
        ui->adsbData->clearSelection();
    }
}

// Show feed dialog
void ADSBDemodGUI::feedSelect(const QPoint& p)
{
    ADSBDemodFeedDialog dialog(&m_settings);
    dialog.move(p);
    new DialogPositioner(&dialog, false);

    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings({
            "exportClientEnabled",
            "exportClientHost",
            "exportClientPort",
            "exportClientFormat",
            "exportServerEnabled",
            "exportServerPort",
            "importEnabled",
            "importHost",
            "importUsername",
            "importPassword",
            "importParameters",
            "importPeriod",
            "importMinLatitude",
            "importMaxLatitude",
            "importMinLongitude",
            "importMaxLongitude"
            });
    }
}

// Show display settings dialog
void ADSBDemodGUI::on_displaySettings_clicked()
{
    bool oldSiUnits = m_settings.m_siUnits;
    bool oldUseLiveryIcons = m_settings.m_useLiveryIcons;
    ADSBDemodDisplayDialog dialog(&m_settings);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        bool unitsChanged = m_settings.m_siUnits != oldSiUnits;
        if (unitsChanged || (m_settings.m_useLiveryIcons != oldUseLiveryIcons)) {
            m_aircraftModel.allAircraftUpdated();
        }
        displaySettings(dialog.getSettingsKeys(), false);
        applySettings(dialog.getSettingsKeys());
    }
}

void ADSBDemodGUI::setShowContainmentRadius(bool show)
{
    QQuickItem *item = ui->map->rootObject();
    if (!item)
    {
        qCritical("ADSBDemodGUI::setShowContainmentRadius: Map not found. Are all required Qt plugins installed?");
        return;
    }
    else
    {
        QQmlProperty::write(item, "showContainmentRadius", show);
    }
}

void ADSBDemodGUI::clearStaleTileCache(const QString& cachePath)
{
    const int currentVersion = 1;
    // Per installation rather than per channel: every ADS-B demod shares this cache, and
    // QSettings is written immediately, where channel settings are only stored when the
    // configuration is saved
    QSettings settings;

    if (settings.value("ADSBDemod/tileCacheVersion", 0).toInt() >= currentVersion) {
        return;
    }

    qDebug() << "ADSBDemodGUI::clearStaleTileCache: clearing the map tile cache once, to"
             << "discard tiles fetched before the CartoDB API key became a setting";

    QDir dir(cachePath);

    if (dir.exists())
    {
        const QStringList filenames = dir.entryList({"osm_100-l-*"});

        for (const auto& filename : filenames)
        {
            QFile file(dir.filePath(filename));

            if (!file.remove()) {
                qDebug() << "ADSBDemodGUI::clearStaleTileCache: Failed to remove" << file.fileName();
            }
        }
    }

    settings.setValue("ADSBDemod/tileCacheVersion", currentVersion);
}

void ADSBDemodGUI::applyMapSettings()
{
#ifdef QT_LOCATION_FOUND
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();

    QQuickItem *item = ui->map->rootObject();
    if (!item)
    {
        qCritical("ADSBDemodGUI::applyMapSettings: Map not found. Are all required Qt plugins installed?");
        return;
    }

    QObject *object = item->findChild<QObject*>("map");
    QGeoCoordinate coords;
    double zoom;
    if (object != nullptr)
    {
        // Save existing position of map
        coords = object->property("center").value<QGeoCoordinate>();
        zoom = object->property("zoomLevel").value<double>();
    }
    else
    {
        // Center on my location when map is first opened
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        zoom = 10.0;
    }

    // Check requested map provider is available - if not, try the other
    QString mapProvider = m_settings.m_mapProvider;
    QStringList mapProviders = QGeoServiceProvider::availableServiceProviders();
    if ((mapProvider == "osm") && (!mapProviders.contains(mapProvider))) {
        mapProvider = "mapboxgl";
    }
    if ((mapProvider == "mapboxgl") && (!mapProviders.contains(mapProvider))) {
        mapProvider = "osm";
    }

    // Create the map using the specified provider
    QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
    QQmlProperty::write(item, "aircraftMinZoomLevel", m_settings.m_aircraftMinZoom);
    QQmlProperty::write(item, "showContainmentRadius", m_settings.m_displayRadius);
    QQmlProperty::write(item, "mapProvider", mapProvider);
    QVariantMap parameters;
    QString mapType;

    if (mapProvider == "osm")
    {
#ifdef __EMSCRIPTEN__
        // Default is http://maps-redirect.qt.io/osm/5.8/ and Emscripten needs https
        parameters["osm.mapping.providersrepository.address"] = QString("https://sdrangel.beniston.com/sdrangel/maps/");
#else
        // Use our repo, so we can append API key and redefine transmit maps
        parameters["osm.mapping.providersrepository.address"] = QString("http://127.0.0.1:%1/").arg(m_osmPort);
#endif
        // Use ADS-B specific cache, as we use different transmit maps
        QString cachePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/QtLocation/5.8/tiles/osm/sdrangel_adsb";
        parameters["osm.mapping.cache.directory"] = cachePath;
        // On Linux, we need to create the directory
        QDir dir(cachePath);
        if (!dir.exists()) {
            dir.mkpath(cachePath);
        }
        clearStaleTileCache(cachePath);
        switch (m_settings.m_mapType)
        {
        case ADSBDemodSettings::AVIATION_LIGHT:
            mapType = "Transit Map";
            break;
        case ADSBDemodSettings::AVIATION_DARK:
            mapType = "Night Transit Map";
            break;
        case ADSBDemodSettings::STREET:
            mapType = "Street Map";
            break;
        case ADSBDemodSettings::SATELLITE:
            mapType = "Satellite Map";
            break;
        }
    }
    else if (mapProvider == "mapboxgl")
    {
        switch (m_settings.m_mapType)
        {
        case ADSBDemodSettings::AVIATION_LIGHT:
            mapType = "mapbox://styles/mapbox/light-v9";
            break;
        case ADSBDemodSettings::AVIATION_DARK:
            mapType = "mapbox://styles/mapbox/dark-v9";
            break;
        case ADSBDemodSettings::STREET:
            mapType = "mapbox://styles/mapbox/streets-v10";
            break;
        case ADSBDemodSettings::SATELLITE:
            mapType = "mapbox://styles/mapbox/satellite-v9";
            break;
        }
    }

    QVariant retVal;
    if (!QMetaObject::invokeMethod(item, "createMap", Qt::DirectConnection,
                                Q_RETURN_ARG(QVariant, retVal),
                                Q_ARG(QVariant, QVariant::fromValue(parameters)),
                                Q_ARG(QVariant, mapType),
                                Q_ARG(QVariant, QVariant::fromValue(this))))
    {
        qCritical() << "ADSBDemodGUI::applyMapSettings - Failed to invoke createMap";
    }
    QObject *newMap = retVal.value<QObject *>();

    // Restore position of map
    if (newMap != nullptr)
    {
        // Move antenna icon to My Position
        QObject *stationObject = newMap->findChild<QObject*>("station");
        if(stationObject != NULL)
        {
            QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
            coords.setLatitude(stationLatitude);
            coords.setLongitude(stationLongitude);
            coords.setAltitude(stationAltitude);
            stationObject->setProperty("coordinate", QVariant::fromValue(coords));
            stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
        }
        else
        {
            qDebug() << "ADSBDemodGUI::applyMapSettings - Couldn't find station";
        }

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        newMap = newMap->findChild<QObject*>("map");
#endif
        if (coords.isValid())
        {
            newMap->setProperty("zoomLevel", QVariant::fromValue(zoom));
            newMap->setProperty("center", QVariant::fromValue(coords));
        }
    }
    else
    {
        qDebug() << "ADSBDemodGUI::applyMapSettings - createMap returned a nullptr";
    }
#endif // QT_LOCATION_FOUND
}

// Called from QML when empty space clicked
void ADSBDemodGUI::clearHighlighted()
{
    highlightAircraft(nullptr);
}

ADSBDemodGUI::ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::ADSBDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_basicSettingsShown(false),
    m_doApplySettings(true),
    m_tickCount(0),
    m_aircraftInfo(nullptr),
    m_airportModel(this),
    m_airspaceModel(this),
    m_trackAircraft(nullptr),
    m_highlightAircraft(nullptr),
    m_chart(nullptr),
    m_adsbFrameRateSeries(nullptr),
    m_modesFrameRateSeries(nullptr),
    m_aircraftSeries(nullptr),
    m_xAxis(nullptr),
    m_fpsYAxis(nullptr),
    m_aircraftYAxis(nullptr),
    m_speech(nullptr),
    m_progressDialog(nullptr),
    m_networkManager(nullptr),
    m_loadingData(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodadsb/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);

    ui->statsTable->setItemDelegate(new FastItemDelegate(ui->statsTable));
    ui->statsTable->setHorizontalHeader(new FastHeaderView(Qt::Horizontal, ui->statsTable));
    ui->adsbData->setHorizontalHeader(new FastHeaderView(Qt::Horizontal, ui->adsbData));

    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    // Enable MSAA antialiasing on 2D map
    // This can be much faster than using layer.smooth in the QML, when there are many items
    // However, only seems to work when set to 16, and doesn't seem to be supported on all graphics cards
    int multisamples = MainCore::instance()->getSettings().getMapMultisampling();
    if (multisamples > 0)
    {
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setSamples(multisamples);
#ifdef QT_LOCATION_FOUND
        ui->map->setFormat(format);
#endif
    }

    m_osmPort = 0; // Pick a free port
    m_templateServer = new ADSBOSMTemplateServer(maptilerAPIKey(), cartoDBAPIKey(), m_osmPort);

#ifdef QT_LOCATION_FOUND
    ui->map->setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->map->rootContext()->setContextProperty("aircraftModel", &m_aircraftModel);
    ui->map->rootContext()->setContextProperty("airportModel", &m_airportModel);
    ui->map->rootContext()->setContextProperty("airspaceModel", &m_airspaceModel);
    ui->map->rootContext()->setContextProperty("navAidModel", &m_navAidModel);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map.qml")));
#elif defined(__EMSCRIPTEN__)
    // No Qt5Compat.GraphicalEffects
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map_6_strict.qml")));
#else
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map_6_strict.qml")));
#endif
    ui->map->installEventFilter(this);
#else
    ui->map->hide();
#endif

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_adsbDemod = reinterpret_cast<ADSBDemod*>(rxChannel);
    m_adsbDemod->setMessageQueueToGUI(getInputMessageQueue());

    // The decoding happens in the channel's tracker, on its own thread; the GUI shows
    // what it sends
    ADSBDemodTracker *tracker = m_adsbDemod->getTracker();
    connect(tracker, &ADSBDemodTracker::aircraftUpdated, this, &ADSBDemodGUI::aircraftUpdated);
    connect(tracker, &ADSBDemodTracker::aircraftRemoved, this, &ADSBDemodGUI::aircraftRemoved);
    connect(tracker, &ADSBDemodTracker::statsUpdated, this, &ADSBDemodGUI::statsUpdated);
    connect(tracker, &ADSBDemodTracker::coverageUpdated, this, &ADSBDemodGUI::coverageUpdated);
    connect(tracker, &ADSBDemodTracker::qnhEstimate, this, &ADSBDemodGUI::qnhEstimate);
    connect(tracker, &ADSBDemodTracker::speechRequest, this, &ADSBDemodGUI::speechRequest);
    connect(tracker, &ADSBDemodTracker::highlightRequest, this, &ADSBDemodGUI::highlightRequest);
    connect(tracker, &ADSBDemodTracker::targetRequest, this, &ADSBDemodGUI::targetRequest);
    connect(tracker, &ADSBDemodTracker::photoRefresh, this, &ADSBDemodGUI::photoRefresh);
    connect(tracker, &ADSBDemodTracker::rates, this, &ADSBDemodGUI::rates);

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *feedRightClickEnabler = new CRightClickEnabler(ui->feed);
    connect(feedRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(feedSelect(const QPoint &)));

    CRightClickEnabler *coverageClickEnabler = new CRightClickEnabler(ui->coverage);
    connect(coverageClickEnabler, &CRightClickEnabler::rightClick, this, [this] (const QPoint&) {
        QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "clearCoverage", Qt::QueuedConnection);
    });

    CRightClickEnabler *statsClickEnabler = new CRightClickEnabler(ui->stats);
    connect(statsClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(clearStats(const QPoint &)));

    CRightClickEnabler *displayChartClickEnabler = new CRightClickEnabler(ui->displayChart);
    connect(displayChartClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(clearChart(const QPoint &)));

    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    ui->warning->setVisible(false);
    ui->warning->setStyleSheet("QLabel { background-color: red; }");

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("ADS-B Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->ic->setText("IC");

    // The table is a view of the aircraft; sorting and filtering happen in the proxy.
    // Sorting uses the raw cell values, so formatted numbers still sort numerically
    m_aircraftTableModel.setSettings(&m_settings);
    m_aircraftProxy = new ADSBAircraftSortProxy(this);
    m_aircraftProxy->setSourceModel(&m_aircraftTableModel);
    m_aircraftProxy->setSortRole(ADSBAircraftTableModel::SortRole);
    m_aircraftProxy->setDynamicSortFilter(true);
    ui->adsbData->setModel(m_aircraftProxy);
    ui->adsbData->setItemDelegate(new FastItemDelegate(ui->adsbData));
    ui->channelPower->setFixedSize(
        ui->channelPower->fontMetrics().horizontalAdvance("-100.0 dB") + 2,
        ui->channelPower->sizeHint().height());

    connect(ui->adsbData, &QTableView::clicked, this, &ADSBDemodGUI::adsbData_clicked);
    connect(ui->adsbData, &QTableView::doubleClicked, this, &ADSBDemodGUI::adsbData_doubleClicked);

    // Set size of airline icons
    ui->adsbData->setIconSize(QSize(85, 20));
    ADSBAircraftTableModel::setDevicePixelRatio(ui->adsbData->devicePixelRatioF());
    // Size the columns to fit representative data
    resizeTable();
    // Allow user to reorder columns
    ui->adsbData->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->adsbData->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    menu = new QMenu(ui->adsbData);
    for (int i = 0; i < ui->adsbData->horizontalHeader()->count(); i++)
    {
        QString text = ADSBAircraftTableModel::columnNames()[i];
        menu->addAction(createCheckableItem(text, i, true));
    }
    ui->adsbData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->adsbData->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->adsbData->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(adsbData_sectionMoved(int, int, int)));
    connect(ui->adsbData->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(adsbData_sectionResized(int, int, int)));
    // Context menu
    ui->adsbData->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->adsbData, SIGNAL(customContextMenuRequested(QPoint)), SLOT(adsbData_customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->adsbData);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &ADSBDemodGUI::adsbData_customContextMenuRequested);

    ui->photoHeader->setVisible(false);
    ui->photoFlag->setVisible(false);
    ui->photo->setVisible(false);
    ui->flightDetails->setVisible(false);
    ui->aircraftDetails->setVisible(false);

    ui->statsTable->horizontalHeader()->setStretchLastSection(true);
    ui->statsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->statsTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(statsTable_customContextMenuRequested(QPoint)));
    TableTapAndHold *statsTapAndHold = new TableTapAndHold(ui->statsTable);
    connect(statsTapAndHold, &TableTapAndHold::tapAndHold, this, &ADSBDemodGUI::statsTable_customContextMenuRequested);

    plotChart();

    // Read aircraft information database, if it has previously been downloaded
    AircraftInformation::init();
    connect(&m_osnDB, &OsnDB::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_osnDB, &OsnDB::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_osnDB, &OsnDB::downloadProgress, this, &ADSBDemodGUI::downloadProgress);
    connect(&m_osnDB, &OsnDB::downloadAircraftInformationFinished, this, &ADSBDemodGUI::downloadAircraftInformationFinished);
    m_aircraftInfo = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();
    // The tracker loads its own references to the same read-only data
    QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "reloadDatabases", Qt::QueuedConnection);

    // Read airport information database, if it has previously been downloaded
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadProgress, this, &ADSBDemodGUI::downloadProgress);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadAirportInformationFinished, this, &ADSBDemodGUI::downloadAirportInformationFinished);
    m_airportInfo = OurAirportsDB::getAirportsById();

    // Read airspaces and NAVAIDs
    connect(&m_openAIP, &OpenAIP::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_openAIP, &OpenAIP::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_openAIP, &OpenAIP::downloadAirspaceFinished, this, &ADSBDemodGUI::downloadAirspaceFinished);
    connect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &ADSBDemodGUI::downloadNavAidsFinished);
    m_airspaces = OpenAIP::getAirspaces();
    m_navAids = OpenAIP::getNavAids();

    // Get station position
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);
    QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "setPosition", Qt::QueuedConnection,
                              Q_ARG(float, stationLatitude), Q_ARG(float, stationLongitude), Q_ARG(float, stationAltitude));

    // These are the default values in sdrbase/settings/preferences.cpp
    if ((stationLatitude == 49.012423f) && (stationLongitude == 8.418125f)) {
        ui->warning->setText("Please set your antenna location under Preferences > My Position");
    }

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &ADSBDemodGUI::preferenceChanged);
    if (m_deviceUISet->m_deviceAPI->getSampleSource()) {
        connect(m_deviceUISet->m_deviceAPI->getSampleSource(), &DeviceSampleSource::positionChanged, this, &ADSBDemodGUI::devicePositionChanged);
    }

    // Get airport weather when requested
    connect(&m_airportModel, &AirportModel::requestMetar, this, &ADSBDemodGUI::requestMetar);

    initCoverageDisplay();

    // Add airports within range of My Position
    updateAirports();
    updateAirspaces();
    updateNavAids();

    m_aviationWeather = nullptr;

    connect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &ADSBDemodGUI::aircraftPhoto);
    connect(ui->photo, &ClickableLabel::clicked, this, &ADSBDemodGUI::photoClicked);

    // Update demod list when channels are added or removed
    connect(MainCore::instance(), &MainCore::channelAdded, this, &ADSBDemodGUI::updateChannelList);
    connect(MainCore::instance(), &MainCore::channelRemoved, this, &ADSBDemodGUI::updateChannelList);

    updateChannelList();
    displaySettings(QStringList(), true);
    makeUIConnections();
    applyAllSettings();

    m_networkManager = new QNetworkAccessManager();

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &ADSBDemodGUI::redrawMap);
    m_redrawMapTimer.setSingleShot(true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

ADSBDemodGUI::~ADSBDemodGUI()
{
    m_adsbDemod->setMessageQueueToGUI(nullptr);
    disconnect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &ADSBDemodGUI::preferenceChanged);
    if (m_templateServer)
    {
        m_templateServer->close();
        delete m_templateServer;
    }
    disconnect(&m_openAIP, &OpenAIP::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    disconnect(&m_openAIP, &OpenAIP::downloadError, this, &ADSBDemodGUI::downloadError);
    disconnect(&m_openAIP, &OpenAIP::downloadAirspaceFinished, this, &ADSBDemodGUI::downloadAirspaceFinished);
    disconnect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &ADSBDemodGUI::downloadNavAidsFinished);
    disconnect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &ADSBDemodGUI::aircraftPhoto);
    disconnect(&m_redrawMapTimer, &QTimer::timeout, this, &ADSBDemodGUI::redrawMap);
    disconnect(&MainCore::instance()->getMasterTimer(), &QTimer::timeout, this, &ADSBDemodGUI::tick);
    m_redrawMapTimer.stop();
    delete ui;
    qDeleteAll(m_aircraft);
    delete m_aviationWeather;
    delete m_networkManager;
}

// Free keys, so no point in stealing them :)

QString ADSBDemodGUI::maptilerAPIKey() const
{
    return m_settings.m_maptilerAPIKey.isEmpty() ? "Nzl2cSyOnewxUc9VWg4n" : m_settings.m_maptilerAPIKey;
}

QString ADSBDemodGUI::cartoDBAPIKey() const
{
    return m_settings.m_cartoDBAPIKey.isEmpty() ? "cb1_27a9_1_036ae52d2884fda8713b961c" : m_settings.m_cartoDBAPIKey;
}

void ADSBDemodGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void ADSBDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        qDebug() << "ADSBDemodGUI::applySettings";

        ADSBDemod::MsgConfigureADSBDemod* message = ADSBDemod::MsgConfigureADSBDemod::create(m_settings, m_settingsKeys, force);
        m_adsbDemod->getInputMessageQueue()->push(message);

        m_settingsKeys.clear();
    }
}

void ADSBDemodGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

// All settings are valid - we just use settingsKeys here to avoid updating things that are slow to update
void ADSBDemodGUI::displaySettings(const QStringList& settingsKeys, bool force)
{
    if (settingsKeys.contains("maptilerAPIKey") || settingsKeys.contains("cartoDBAPIKey"))
    {
        m_templateServer->close();
        delete m_templateServer;
        m_templateServer = new ADSBOSMTemplateServer(maptilerAPIKey(), cartoDBAPIKey(), m_osmPort);
    }

    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1000000.0, 0, 'f', 1));
    ui->rfBW->setValue((int)m_settings.m_rfBandwidth);

    ui->spb->setCurrentIndex(m_settings.m_samplesPerBit/2-1);
    ui->demodModeS->setChecked(m_settings.m_demodModeS);

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold, 0, 'f', 1));
    ui->threshold->setValue((int)(m_settings.m_correlationThreshold*10.0f));

    ui->chipsThresholdText->setText(QString("%1").arg(m_settings.m_chipsThreshold));
    ui->chipsThreshold->setValue((int)(m_settings.m_chipsThreshold));

    ui->phaseStepsText->setText(QString("%1").arg(m_settings.m_interpolatorPhaseSteps));
    ui->phaseSteps->setValue(m_settings.m_interpolatorPhaseSteps);
    ui->tapsPerPhaseText->setText(QString("%1").arg(m_settings.m_interpolatorTapsPerPhase, 0, 'f', 1));
    ui->tapsPerPhase->setValue((int)(m_settings.m_interpolatorTapsPerPhase*10.0f));
    // Enable these controls only for developers
    if (1)
    {
        ui->phaseStepsText->setVisible(false);
        ui->phaseSteps->setVisible(false);
        ui->tapsPerPhaseText->setVisible(false);
        ui->tapsPerPhase->setVisible(false);
    }
    ui->feed->setChecked(m_settings.m_feedEnabled);

    ui->flightPaths->setChecked(m_settings.m_flightPaths);
    ui->allFlightPaths->setChecked(m_settings.m_allFlightPaths);
    m_aircraftModel.setSettings(&m_settings);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->manualQNH->setChecked(m_settings.m_manualQNH);
    ui->qnh->setEnabled(m_settings.m_manualQNH);
    ui->qnh->setValue(m_settings.m_qnh);

    ui->atcLabels->setChecked(m_settings.m_atcLabels);
    ui->coverage->setChecked(m_settings.m_displayCoverage);
    ui->displayChart->setChecked(m_settings.m_displayChart);
    ui->chart->setVisible(m_settings.m_displayChart);
    ui->stats->setChecked(m_settings.m_displayDemodStats);
    ui->statsTable->setVisible(m_settings.m_displayDemodStats);
    ui->displayOrientation->setChecked(m_settings.m_displayOrientation);
    ui->splitter->setOrientation(m_settings.m_displayOrientation ? Qt::Horizontal : Qt::Vertical);
    ui->displayRadius->setChecked(m_settings.m_displayRadius);

    updateIndexLabel();

    if (settingsKeys.contains("tableFontName") || settingsKeys.contains("tableFontSize") || force)
    {
        QFont font(m_settings.m_tableFontName, m_settings.m_tableFontSize);
        ui->adsbData->setFont(font);
        ui->statsTable->setFont(font);
    }

    // The column headings for these carry the units, which the model picks from the
    // settings - it just needs to know they have changed
    if (settingsKeys.contains("siUnits") || force)
    {
        emit ui->adsbData->model()->headerDataChanged(Qt::Horizontal, 0, ADSBDEMOD_COLUMNS - 1);
        m_aircraftTableModel.allDataChanged();
    }

    // Order and size columns
    if (settingsKeys.contains("columnSizes") || settingsKeys.contains("columnIndexes") || force)
    {
        QHeaderView* header = ui->adsbData->horizontalHeader();
        for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
        {
            bool hidden = m_settings.m_columnSizes[i] == 0;
            header->setSectionHidden(i, hidden);
            menu->actions().at(i)->setChecked(!hidden);
            if (m_settings.m_columnSizes[i] > 0)
                ui->adsbData->setColumnWidth(i, m_settings.m_columnSizes[i]);
            header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
        }
    }

    // Only update airports on map if settings have changed
    if ((m_airportInfo != nullptr)
        && (((settingsKeys.contains("airportRange") || force) && (m_settings.m_airportRange != m_currentAirportRange))
            || ((settingsKeys.contains("airportMinimumSize") || force) && (m_settings.m_airportMinimumSize != m_currentAirportMinimumSize))
            || ((settingsKeys.contains("displayHeliports") || force) && (m_settings.m_displayHeliports != m_currentDisplayHeliports)))) {
        updateAirports();
    }

    if (settingsKeys.contains("airspaces") || force) {
        updateAirspaces();
    }
    if (settingsKeys.contains("displayNavAids") || settingsKeys.contains("airspaceRange") || force) {
        updateNavAids();
    }

    if (!m_settings.m_displayDemodStats)
        ui->stats->setText("");

    if (settingsKeys.contains("aviationstackAPIKey") || force) {
    }
    if (settingsKeys.contains("checkWXAPIKey") || force) {
        initAviationWeather();
    }

    if (settingsKeys.contains("importPeriod")
        || settingsKeys.contains("feedEnabled")
        || settingsKeys.contains("importEnabled")
        || settingsKeys.contains("importHost")
        || settingsKeys.contains("importUsername")
        || settingsKeys.contains("importPassword")
        || settingsKeys.contains("importClientId")
        || settingsKeys.contains("importClientSecret")
        || force) {
    }

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    enableSpeechIfNeeded();

    if (settingsKeys.contains("mapProvider")
        || settingsKeys.contains("aircraftMinZoom")
        || settingsKeys.contains("mapType")
        || force)
    {
#ifdef __EMSCRIPTEN__
        // FIXME: If we don't have this delay, tile server requests get deleted
        QTimer::singleShot(250, [this] {
            applyMapSettings();
            });
#else
        applyMapSettings();
#endif
    }
}

void ADSBDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ADSBDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ADSBDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

static double roundUpNearestHundred(double x)
{
    return 100 * (int) floor(((int) (x + 99.0)) / 100.0);
}


// Tell the map model about the aircraft that have moved since the last flush
void ADSBDemodGUI::flushMapUpdates()
{
    for (Aircraft *aircraft : m_dirtyMapAircraft) {
        m_aircraftModel.aircraftUpdated(aircraft);
    }
    m_dirtyMapAircraft.clear();
}


void ADSBDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_adsbDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    m_tickCount++;

    // Tick is called 20x a second
    const int ticksPerSecond = 20;

    // Repaint the table cells and map markers that have changed. Coalescing these
    // rather than redrawing on every frame is what keeps a busy sky affordable
    if (m_tickCount % (ticksPerSecond / 4) == 0)
    {
        m_aircraftTableModel.flushDirty();
        flushMapUpdates();
    }

    // Averaging old chart data happens once a minute
    if ((m_tickCount % (ticksPerSecond * 60)) == 0)
    {
        // Average data 10 minutes old over 1 minute, so we don't have too many points
        const int ageMins = 10;
        const int averagePeriodMins = 1;
        if ((m_tickCount % (ticksPerSecond*60*averagePeriodMins)) == 0)
        {
            QDateTime endTime, startTime;

            if (m_averageTime.isValid())
            {
                startTime = m_averageTime;
                endTime = startTime.addSecs(averagePeriodMins*60);
            }
            else
            {
                endTime = QDateTime::currentDateTime().addSecs(-ageMins*60);
                startTime = endTime.addSecs(-averagePeriodMins*60);
            }

            if (m_aircraftSeries) {
                averageSeries(m_aircraftSeries, startTime, endTime);
            }
            if (m_modesFrameRateSeries) {
                averageSeries(m_modesFrameRateSeries, startTime, endTime);
            }
            if (m_adsbFrameRateSeries) {
                averageSeries(m_adsbFrameRateSeries, startTime, endTime);
            }

            m_averageTime = endTime;
        }
    }
}

// The coverage outlines the tracker sends points for
void ADSBDemodGUI::initCoverageDisplay()
{
    float lat = m_azEl.getLocationSpherical().m_latitude;
    float lon = m_azEl.getLocationSpherical().m_longitude;

    for (int i = 0; i < 2; i++)
    {
        m_coverageAirspace[i].m_polygon.resize(2 * 360 / 5);
        m_coverageAirspace[i].m_center.setX(lon);
        m_coverageAirspace[i].m_center.setY(lat);
        m_coverageAirspace[i].m_position.setX(lon);
        m_coverageAirspace[i].m_position.setY(i == 0 ? lat : lat + 0.01);
        m_coverageAirspace[i].m_bottom.m_alt = (i == 0) ? 0 : 10000;
        m_coverageAirspace[i].m_top.m_alt = (i == 0) ? 10000 : 66000;
        m_coverageAirspace[i].m_name = (i == 0) ? "Coverage Map Low" : "Coverage Map High";
    }
}

void ADSBDemodGUI::rates(double adsbRate, double modesRate, int activeAircraft)
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    bool xAtMax = true;

    if (m_adsbFrameRateSeries)
    {
        if (m_adsbFrameRateSeries->count() > 0) {
            xAtMax = m_adsbFrameRateSeries->at(m_adsbFrameRateSeries->count() - 1).x() == m_xAxis->max().toMSecsSinceEpoch();
        }
        m_adsbFrameRateSeries->append(currentDateTime.toMSecsSinceEpoch(), adsbRate);
    }
    if (m_modesFrameRateSeries) {
        m_modesFrameRateSeries->append(currentDateTime.toMSecsSinceEpoch(), modesRate);
    }
    if (m_xAxis && xAtMax) {
        m_xAxis->setMax(currentDateTime);
    }
    if (m_fpsYAxis)
    {
        if (m_fpsYAxis->max() < adsbRate) {
            m_fpsYAxis->setMax(roundUpNearestHundred(adsbRate));
        }
        if (m_fpsYAxis->max() < modesRate) {
            m_fpsYAxis->setMax(roundUpNearestHundred(modesRate));
        }
    }
    if (m_aircraftSeries)
    {
        m_aircraftSeries->append(currentDateTime.toMSecsSinceEpoch(), activeAircraft);
        if (m_aircraftYAxis->max() < activeAircraft + 1) {
            m_aircraftYAxis->setMax(activeAircraft + 1);
        }
    }
}

// Replace data in series between specified times with its average
void ADSBDemodGUI::averageSeries(QLineSeries *series, const QDateTime& startTime, const QDateTime& endTime)
{
    int startIdx = 0;
    int endIdx = -1;

    for (int i = series->count() - 1; i >= 0; i--)
    {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(series->at(i).x());

        if ((endIdx == -1) && (dt <= endTime))
        {
            endIdx = i;
        }
        else if (dt < startTime)
        {
            startIdx = i + 1;
            break;
        }
    }
    int count = (endIdx - startIdx) + 1;

    if ((endIdx != -1) && (count > 1))
    {
        double sum = 0.0;
        for (int i = startIdx; i <= endIdx; i++) {
            sum += series->at(i).y();
        }
        double avg = sum / count;
        series->removePoints(startIdx, count);
        qint64 midPoint = startTime.toMSecsSinceEpoch() + (endTime.toMSecsSinceEpoch() - startTime.toMSecsSinceEpoch()) / 2;
        series->insert(startIdx, QPointF(midPoint, avg));
    }
}


void ADSBDemodGUI::resizeTable()
{
    // Size the columns to fit representative data, so they don't have to be resized
    // when an aircraft with a long value turns up. The table used to be filled with a
    // dummy row for this; with a model the widths are measured directly instead.
    static const char *sampleData[ADSBDEMOD_COLUMNS] = {
    "ICAO ID",                   // ICAO
    "WWW88WW",                   // CALLSIGN
    "ATC Callsign-",             // ATC_CALLSIGN
    "Aircraft12345",             // MODEL
    "WWWW",                      // TYPE
    "sideview",                  // SIDEVIEW
    "airbrigdecargo1",           // AIRLINE
    "Country",                   // COUNTRY
    "GS (kn)",                   // GROUND_SPEED
    "TAS (kn)",                  // TRUE_AIRSPEED
    "IAS (kn)",                  // INDICATED_AIRSPEED
    "0.999",                     // MACH
    "Sel Alt (ft)",              // SEL_ALTITUDE
    "Surface",                   // ALTITUDE
    "VR (ft/m)",                 // VERTICALRATE
    "Sel Hd (o)",                // SEL_HEADING
    "Hd (o)",                    // HEADING
    "Trk (o)",                   // TRACK
    "TR (o/s)",                  // TURNRATE
    "Roll (o)",                  // ROLL
    "D (km)",                    // RANGE
    "Az/El (o)",                 // AZEL
    "Rotorcraft",                // CATEGORY
    "No emergency",              // STATUS
    "Squawk",                    // SQUAWK
    "Ident",                     // IDENT
    "G-888888",                  // REGISTRATION
    "8888-88-88",                // REGISTERED
    "The Boeing Company",        // MANUFACTURER
    "British Airways",           // OWNER
    "Operator",                  // OPERATOR_ICAO
    "AP",                        // AP
    "V Mode",                    // V_MODE
    "L Mode",                    // L_MODE
    "TCAS",                      // TCAS
    "No ACAS",                   // ACAS
    "RA",                        // RA
    "600-1200",                  // MAX_SPEED
    "Version",                   // VERSION
    "85",                        // LENGTH
    "72.5",                      // WIDTH
    "Baro (mb)",                 // BARO
    "H Wnd (kn)",                // HEADWIND
    "OAT (C)",                   // EST_AIR_TEMP
    "Wnd (kn)",                  // WIND_SPEED
    "Wnd (o)",                   // WIND_DIR
    "P (hPa)",                   // STATIC_PRESSURE
    "T (C)",                     // STATIC_AIR_TEMP
    "U (%)",                     // HUMIDITY
    "-90.00000",                 // LATITUDE
    "-180.000000",               // LONGITUDE
    "63",                        // IC
    "99:99:99",                  // TIME
    "Frames",                    // FRAMECOUNT
    "ADS-B FC",                  // ADSB_FRAMECOUNT
    "Mode S FC",                 // MODES_FRAMECOUNT
    "Non-transponder",           // NON_TRANSPONDER
    "TIS-B FC",                  // TIS_B_FRAMECOUNT
    "ADS-R FC",                  // ADSR_FRAMECOUNT
    "< 0.1 NM",                  // RADIUS
    "< 0.05 NM",                 // NACP
    "< 0.3 m/s",                 // NACV
    ">= 150 m",                  // GVA
    "< 0.05 NM",                 // NIC
    "",                          // NIC_BARO
    "<= 1e-7 ph",                // SIL
    "0.001/0.001/0.001",         // CORRELATION
    "-100.0",                    // RSSI
    "scheduled",                 // FLIGHT_STATUS
    "WWWW",                      // DEP
    "WWWW",                      // ARR
    "WWWW",                      // STOPS
    "12:00 -1",                  // STD
    "12:00 -1",                  // ETD
    "12:00 -1",                  // ATD
    "12:00 +1",                  // STA
    "12:00 +1",                  // ETA
    "12:00 +1",                  // ATA
    };

    QFontMetrics metrics(ui->adsbData->font());
    const QString *names = ADSBAircraftTableModel::columnNames();
    // Room for the sort indicator and cell margins
    const int padding = 4 * ui->adsbData->style()->pixelMetric(QStyle::PM_HeaderMargin) + 12;

    for (int col = 0; col < ADSBDEMOD_COLUMNS; col++)
    {
        int width = metrics.horizontalAdvance(names[col]);
        const QString sample = QString(sampleData[col]);

        if (!sample.isEmpty()) {
            width = std::max(width, metrics.horizontalAdvance(sample));
        }
        // The airline, sideview and country columns show icons
        if ((col == ADSB_COL_AIRLINE) || (col == ADSB_COL_SIDEVIEW)) {
            width = std::max(width, 85);
        } else if (col == ADSB_COL_COUNTRY) {
            width = std::max(width, 40);
        }
        ui->adsbData->setColumnWidth(col, width + padding);
    }
}


// Convert to hh:mm (+/-days)

void ADSBDemodGUI::aircraftPhoto(const PlaneSpottersPhoto *photo)
{
    // Make sure the photo is for the currently highlighted aircraft, as it may
    // have taken a while to download
    if (!photo->m_pixmap.isNull() && m_highlightAircraft && (m_highlightAircraft->cellText(ADSB_COL_ICAO) == photo->m_id))
    {
        ui->photo->setPixmap(photo->m_pixmap);
        ui->photo->setToolTip(QString("Photographer: %1").arg(photo->m_photographer)); // Required by terms of use
        ui->photoHeader->setVisible(true);
        ui->photoFlag->setVisible(true);
        ui->photo->setVisible(true);
        ui->flightDetails->setVisible(true);
        ui->aircraftDetails->setVisible(true);
        m_photoLink = photo->m_link;
    }
}

void ADSBDemodGUI::photoClicked()
{
    // Photo needs to link back to PlaneSpotters, as per terms of use
    if (m_highlightAircraft)
    {
        if (m_photoLink.isEmpty())
        {
            QString icaoUpper = QString("%1").arg(m_highlightAircraft->m_icao, 1, 16).toUpper();
            QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
        }
        else
        {
            QDesktopServices::openUrl(QUrl(m_photoLink));
        }
    }
}

void ADSBDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySetting("logEnabled");
}

void ADSBDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received frames to", "", "*.csv");
    fileDialog.setDefaultSuffix("csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySetting("logFilename");
        }
    }
}

// Read .csv log and process as received frames
void ADSBDemodGUI::on_logOpen_clicked()
{
    QFileDialog fileDialog(nullptr, "Select .csv log file to read", "", "*.csv");
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QDateTime startTime = QDateTime::currentDateTime();
                m_loadingData = true;
                QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "setLoadingData", Qt::QueuedConnection, Q_ARG(bool, true));
                ui->adsbData->blockSignals(true);
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Data", "Correlation"}, error);
                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int dataCol = colIndexes.value("Data");
                    int correlationCol = colIndexes.value("Correlation");
                    int maxCol = std::max(dataCol, correlationCol);

                    QMessageBox dialog(this);
                    dialog.setText("Reading ADS-B data");
                    dialog.addButton(QMessageBox::Cancel);
                    dialog.show();
                    QApplication::processEvents();
                    int count = 0;
                    int countOtherDF = 0;
                    bool cancelled = false;
                    QStringList cols;
                    crcadsb crc;
                    while (!cancelled && CSV::readRow(in, &cols))
                    {
                        if (cols.size() > maxCol)
                        {
                            QTime time = QTime::fromString(cols[timeCol], QStringLiteral("hh:mm:ss.zzz"));
                            if (!time.isValid()) {
                                time = QTime::fromString(cols[timeCol], QStringLiteral("hh:mm:ss"));
                            }
                            QDateTime dateTime = QDateTime(QDate::fromString(cols[dateCol]), time);
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());
                            float correlation = cols[correlationCol].toFloat();
                            int df = (bytes[0] >> 3) & ADS_B_DF_MASK; // Downlink format
                            if (   (m_settings.m_demodModeS && ((df == 0) || (df == 4) || (df == 5) || (df == 11) || (df == 16) || (df == 19) || (df == 20) || (df == 21) || (df == 22) || (df == 24)))
                                || (df == 17) || (df == 18))
                            {
                                int crcCalc = 0;
                                if ((df == 0) || (df == 4) || (df == 5) || (df == 16) || (df == 20) || (df == 21))  // handleADSB requires calculated CRC for Mode-S frames
                                {
                                    crc.init();
                                    crc.calculate((const uint8_t *)bytes.data(), bytes.size()-3);
                                    crcCalc = crc.get();
                                }
                                m_adsbDemod->getTracker()->getInputMessageQueue()->push(
                                    ADSBDemodReport::MsgReportADSB::create(bytes, correlation, correlation, dateTime, crcCalc));
                                if ((count > 0) && (count % 100000 == 0))
                                {
                                    dialog.setText(QString("Reading ADS-B data\n%1 (Skipped %2)").arg(count).arg(countOtherDF));
                                    QApplication::processEvents();
                                    if (dialog.clickedButton()) {
                                        cancelled = true;
                                    }
                                }
                                count++;
                            }
                            else
                            {
                                countOtherDF++;
                            }
                        }
                    }
                    m_aircraftModel.allAircraftUpdated();
                    dialog.close();
                }
                else
                {
                    MessageDialog::critical(this, "ADS-B", error);
                }
                ui->adsbData->blockSignals(false);
                m_loadingData = false;
                QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "setLoadingData", Qt::QueuedConnection, Q_ARG(bool, false));
                if (m_settings.m_autoResizeTableColumns)
                    ui->adsbData->resizeColumnsToContents();
                ui->adsbData->setSortingEnabled(true);
                QDateTime finishTime = QDateTime::currentDateTime();
                qDebug() << "Read CSV in " << startTime.secsTo(finishTime);
            }
            else
            {
                MessageDialog::critical(this, "ADS-B", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void ADSBDemodGUI::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void ADSBDemodGUI::downloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void ADSBDemodGUI::downloadError(const QString& error)
{
    MessageDialog::critical(this, "ADS-B", error);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::downloadAirspaceFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading airspaces.");
    }
    m_airspaces = OpenAIP::getAirspaces();
    updateAirspaces();
    m_openAIP.downloadNavAids();
}

void ADSBDemodGUI::downloadNavAidsFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading NAVAIDs.");
    }
    m_navAids = OpenAIP::getNavAids();
    updateNavAids();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::downloadAircraftInformationFinished()
{
    if (m_progressDialog)
    {
        delete m_progressDialog;
        m_progressDialog = new QProgressDialog("Reading Aircraft Information.", "", 0, 1, this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->show();
        QApplication::processEvents();
    }
    m_aircraftInfo = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();
    // The tracker loads its own references to the same read-only data
    QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "reloadDatabases", Qt::QueuedConnection);
    m_aircraftModel.updateAircraftInformation(m_aircraftInfo);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::downloadAirportInformationFinished()
{
    if (m_progressDialog)
    {
        delete m_progressDialog;
        m_progressDialog = new QProgressDialog("Reading Airport Information.", "", 0, 1, this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->show();
        QApplication::processEvents();
    }
    m_airportInfo = OurAirportsDB::getAirportsById();
    updateAirports();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}





void ADSBDemodGUI::redrawMap()
{
#ifdef QT_LOCATION_FOUND
    // An awful workaround for https://bugreports.qt.io/browse/QTBUG-100333
    // Also used in Map feature
    QQuickItem *item = ui->map->rootObject();
    if (item)
    {
        QObject *object = item->findChild<QObject*>("map");
        if (object)
        {
            double zoom = object->property("zoomLevel").value<double>();
            object->setProperty("zoomLevel", QVariant::fromValue(zoom+1));
            object->setProperty("zoomLevel", QVariant::fromValue(zoom));
        }
    }
#endif
}

void ADSBDemodGUI::showEvent(QShowEvent *event)
{
    if (!event->spontaneous())
    {
        // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
        // MapQuickItems can be in wrong position when window is first displayed
        m_redrawMapTimer.start(500);
    }
    ChannelGUI::showEvent(event);
}

static void scale(qint64& start, qint64& end, qint64 min, int delta, qreal centre)
{
    qint64 diff = end - start;
    double scale = pow(0.50, abs(delta) / 120.0);
    qint64 newRange;

    if (delta < 0) {
        newRange = diff / scale;
    } else {
        newRange = diff * scale;
    }

    diff = std::max(min/2, diff);
    newRange = std::max(min, newRange);
    if (delta < 0) {
        start = start - centre * diff;
    } else {
        start = start + centre * newRange;
    }
    end = start + newRange;
}

bool ADSBDemodGUI::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->map)
    {
        if (event->type() == QEvent::Resize)
        {
            // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
            // MapQuickItems can be in wrong position after vertical resize
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
            QSize oldSize = resizeEvent->oldSize();
            QSize size = resizeEvent->size();
            if (oldSize.height() != size.height()) {
                redrawMap();
            }
        }
    }
    else if (obj == ui->chart)
    {
        if (event->type() == QEvent::Wheel)
        {
            // Use wheel to zoom in / out of X axis or Y axis if shift held
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

            int delta = wheelEvent->angleDelta().y(); // delta is typically 120 for one click of wheel

            if (m_adsbFrameRateSeries)
            {
                QPointF point = wheelEvent->position();
                QRectF plotArea = m_chart->plotArea();

                if (wheelEvent->modifiers() & Qt::ShiftModifier)
                {
                    // Center scaling on cursor location
                    qreal y = (point.y() - plotArea.y()) / plotArea.height();
                    y = 1.0 - std::min(1.0, std::max(0.0, y));

                    qint64 min, max;

                    if (m_adsbFrameRateSeries->isVisible() || m_modesFrameRateSeries->isVisible())
                    {
                        min = (qint64) m_fpsYAxis->min();
                        max = (qint64) m_fpsYAxis->max();

                        scale(min, max, 2LL, delta / 2, y);

                        min = std::max(0LL, min);
                        max = std::min(5000LL, max);

                        m_fpsYAxis->setMin((qreal) min);
                        m_fpsYAxis->setMax((qreal) max);
                    }

                    if (m_aircraftSeries->isVisible())
                    {
                        min = (qint64) m_aircraftYAxis->min();
                        max = (qint64) m_aircraftYAxis->max();

                        scale(min, max, 2LL, delta / 2, y);

                        min = std::max(0LL, min);
                        max = std::min(5000LL, max);

                        m_aircraftYAxis->setMin((qreal) min);
                        m_aircraftYAxis->setMax((qreal) max);
                    }
                }
                else
                {
                    if (m_adsbFrameRateSeries->count() > 1)
                    {
                        // Center scaling on cursor location
                        qreal x = (point.x() - plotArea.x()) / plotArea.width();
                        x = std::min(1.0, std::max(0.0, x));

                        qint64 startMS = m_xAxis->min().toMSecsSinceEpoch();
                        qint64 endMS = m_xAxis->max().toMSecsSinceEpoch();

                        scale(startMS, endMS, 10000LL, delta, x);

                        // Don't let range exceed available data
                        startMS = std::max((qint64) m_adsbFrameRateSeries->at(0).x(), startMS);
                        endMS = std::min((qint64) m_adsbFrameRateSeries->at(m_adsbFrameRateSeries->count() - 1).x(), endMS);
                        QDateTime start = QDateTime::fromMSecsSinceEpoch(startMS);
                        QDateTime end = QDateTime::fromMSecsSinceEpoch(endMS);
                        m_xAxis->setMin(start);
                        m_xAxis->setMax(end);
                    }
                }
            }
            wheelEvent->accept();
            return true;
        }
    }
    return ChannelGUI::eventFilter(obj, event);
}













void ADSBDemodGUI::updatePosition(float latitude, float longitude, float altitude)
{
    // Use device postion in preference to My Position
    ChannelWebAPIUtils::getDevicePosition(getDeviceSetIndex(), latitude, longitude, altitude);

    QGeoCoordinate stationPosition(latitude, longitude, altitude);
    QGeoCoordinate previousPosition(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, m_azEl.getLocationSpherical().m_altitude);

    if (stationPosition != previousPosition)
    {
        m_azEl.setLocation(latitude, longitude, altitude);
        QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "setPosition", Qt::QueuedConnection,
                                  Q_ARG(float, latitude), Q_ARG(float, longitude), Q_ARG(float, altitude));

        // Update distances and what is visible, but only do it if position has changed significantly
        if (!m_lastFullUpdatePosition.isValid() || (stationPosition.distanceTo(m_lastFullUpdatePosition) >= 1000))
        {
            updateAirports();
            updateAirspaces();
            updateNavAids();
            m_lastFullUpdatePosition = stationPosition;
        }

#ifdef QT_LOCATION_FOUND
        // Update icon position on Map
        QQuickItem *item = ui->map->rootObject();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QObject *map = item->findChild<QObject*>("map");
#else
        QObject *map = item->findChild<QObject*>("mapView");
#endif
        if (map != nullptr)
        {
            QObject *stationObject = map->findChild<QObject*>("station");
            if(stationObject != NULL)
            {
                QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
                coords.setLatitude(latitude);
                coords.setLongitude(longitude);
                coords.setAltitude(altitude);
                stationObject->setProperty("coordinate", QVariant::fromValue(coords));
            }
        }
#endif
    }
}

void ADSBDemodGUI::devicePositionChanged(float latitude, float longitude, float altitude)
{
    updatePosition(latitude, longitude, altitude);
}

void ADSBDemodGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        Real myLatitude = MainCore::instance()->getSettings().getLatitude();
        Real myLongitude = MainCore::instance()->getSettings().getLongitude();
        Real myAltitude = MainCore::instance()->getSettings().getAltitude();

        updatePosition(myLatitude, myLongitude, myAltitude);
    }
    else if (pref == Preferences::StationName)
    {
#ifdef QT_LOCATION_FOUND
        // Update icon label on Map
        QQuickItem *item = ui->map->rootObject();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QObject *map = item->findChild<QObject*>("map");
#else
        QObject *map = item->findChild<QObject*>("mapView");
#endif
        if (map != nullptr)
        {
            QObject *stationObject = map->findChild<QObject*>("station");
            if(stationObject != NULL) {
                stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
            }
        }
#endif
    }
    else if (pref == Preferences::MapSmoothing)
    {
#ifdef QT_LOCATION_FOUND
        QQuickItem *item = ui->map->rootObject();
        QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
#endif
    }
}

void ADSBDemodGUI::initAviationWeather()
{
    if (m_aviationWeather)
    {
        disconnect(m_aviationWeather, &AviationWeather::weatherUpdated, this, &ADSBDemodGUI::weatherUpdated);
        delete m_aviationWeather;
        m_aviationWeather = nullptr;
    }
    if (!m_settings.m_checkWXAPIKey.isEmpty())
    {
        m_aviationWeather = AviationWeather::create(m_settings.m_checkWXAPIKey);
        if (m_aviationWeather) {
            connect(m_aviationWeather, &AviationWeather::weatherUpdated, this, &ADSBDemodGUI::weatherUpdated);
        }
    }
}

void ADSBDemodGUI::requestMetar(const QString& icao)
{
    if (m_aviationWeather) {
        m_aviationWeather->getWeather(icao);
    }
}

void ADSBDemodGUI::weatherUpdated(const AviationWeather::METAR &metar)
{
    m_airportModel.updateWeather(metar.m_icao, metar.m_text, metar.decoded());
}

void ADSBDemodGUI::on_manualQNH_clicked(bool checked)
{
    m_settings.m_manualQNH = checked;
    applySetting("manualQNH");
    ui->qnh->setEnabled(m_settings.m_manualQNH);
}

void ADSBDemodGUI::on_qnh_valueChanged(int value)
{
    m_settings.m_qnh = value;
    applySetting("qnh");
}


void ADSBDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ADSBDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &ADSBDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &ADSBDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->chipsThreshold, &QDial::valueChanged, this, &ADSBDemodGUI::on_chipsThreshold_valueChanged);
    QObject::connect(ui->phaseSteps, &QDial::valueChanged, this, &ADSBDemodGUI::on_phaseSteps_valueChanged);
    QObject::connect(ui->tapsPerPhase, &QDial::valueChanged, this, &ADSBDemodGUI::on_tapsPerPhase_valueChanged);
    QObject::connect(ui->adsbData, &QTableView::clicked, this, &ADSBDemodGUI::adsbData_clicked);
    QObject::connect(ui->adsbData, &QTableView::doubleClicked, this, &ADSBDemodGUI::adsbData_doubleClicked);
    QObject::connect(ui->spb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_spb_currentIndexChanged);
    QObject::connect(ui->demodModeS, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_demodModeS_clicked);
    QObject::connect(ui->feed, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_feed_clicked);
    QObject::connect(ui->notifications, &QToolButton::clicked, this, &ADSBDemodGUI::on_notifications_clicked);
    QObject::connect(ui->flightInfo, &QToolButton::clicked, this, &ADSBDemodGUI::on_flightInfo_clicked);
    QObject::connect(ui->findOnMapFeature, &QToolButton::clicked, this, &ADSBDemodGUI::on_findOnMapFeature_clicked);
    QObject::connect(ui->deleteAircraft, &QToolButton::clicked, this, &ADSBDemodGUI::on_deleteAircraft_clicked);
    QObject::connect(ui->getAircraftDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAircraftDB_clicked);
    QObject::connect(ui->getAirportDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirportDB_clicked);
    QObject::connect(ui->getAirspacesDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirspacesDB_clicked);
    QObject::connect(ui->flightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_flightPaths_clicked);
    QObject::connect(ui->allFlightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_allFlightPaths_clicked);
    QObject::connect(ui->atcLabels, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_atcLabels_clicked);
    QObject::connect(ui->coverage, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_coverage_clicked);
    QObject::connect(ui->displayChart, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_displayChart_clicked);
    QObject::connect(ui->displayOrientation, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_displayOrientation_clicked);
    QObject::connect(ui->displayRadius, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_displayRadius_clicked);
    QObject::connect(ui->stats, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_stats_clicked);
    QObject::connect(ui->ic, &CheckList::globalCheckStateChanged, this, &ADSBDemodGUI::on_ic_globalCheckStateChanged);
    QObject::connect(ui->amDemod, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_amDemod_currentIndexChanged);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &ADSBDemodGUI::on_displaySettings_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &ADSBDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &ADSBDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->manualQNH, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_manualQNH_clicked);
    QObject::connect(ui->qnh, QOverload<int>::of(&QSpinBox::valueChanged), this, &ADSBDemodGUI::on_qnh_valueChanged);

}

void ADSBDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}



// Lats and longs in decimal degrees. Distance in metres. Bearing in degrees.
// https://www.movable-type.co.uk/scripts/latlong.html
void ADSBDemodGUI::clearStats(const QPoint& p)
{
    (void) p;

    for (int i = 0; i < ui->statsTable->rowCount(); i++) {
        ui->statsTable->item(i, 0)->setData(Qt::DisplayRole, 0);
    }
    QMetaObject::invokeMethod(m_adsbDemod->getTracker(), "resetStats", Qt::QueuedConnection);
}

void ADSBDemodGUI::clearChart(const QPoint& p)
{
    (void) p;

    if (m_adsbFrameRateSeries) {
        m_adsbFrameRateSeries->clear();
    }
    if (m_modesFrameRateSeries) {
        m_modesFrameRateSeries->clear();
    }
    if (m_aircraftSeries) {
        m_aircraftSeries->clear();
    }
    resetChartAxes();
    m_averageTime = QDateTime();
}

void ADSBDemodGUI::resetChartAxes()
{
    m_xAxis->setMin(QDateTime::currentDateTime());
    m_xAxis->setMax(QDateTime::currentDateTime().addSecs(60*60));
    m_fpsYAxis->setMin(0);
    m_fpsYAxis->setMax(100);
    m_aircraftYAxis->setMin(0);
    m_aircraftYAxis->setMax(10);
}

void ADSBDemodGUI::plotChart()
{
    QChart *oldChart = m_chart;

    m_chart = new QChart();

    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setMargins(QMargins(1, 1, 1, 1));
    m_chart->setTheme(QChart::ChartThemeDark);
    m_chart->legend()->setAlignment(Qt::AlignRight);

    m_adsbFrameRateSeries = new QLineSeries();
    m_adsbFrameRateSeries->setName("ADS-B");

    m_modesFrameRateSeries = new QLineSeries();
    m_modesFrameRateSeries->setName("Mode S");

    m_aircraftSeries = new QLineSeries();
    m_aircraftSeries->setName("Aircraft");

    m_xAxis = new QDateTimeAxis();
    m_fpsYAxis = new QValueAxis();
    m_aircraftYAxis = new QValueAxis();
    resetChartAxes();

    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_fpsYAxis, Qt::AlignLeft);
    m_chart->addAxis(m_aircraftYAxis, Qt::AlignRight);

    m_fpsYAxis->setTitleText("FPS");
    m_aircraftYAxis->setTitleText("Total");

    m_chart->addSeries(m_adsbFrameRateSeries);
    m_chart->addSeries(m_modesFrameRateSeries);
    m_chart->addSeries(m_aircraftSeries);

    m_adsbFrameRateSeries->attachAxis(m_xAxis);
    m_adsbFrameRateSeries->attachAxis(m_fpsYAxis);

    m_modesFrameRateSeries->attachAxis(m_xAxis);
    m_modesFrameRateSeries->attachAxis(m_fpsYAxis);

    m_aircraftSeries->attachAxis(m_xAxis);
    m_aircraftSeries->attachAxis(m_aircraftYAxis);

    ui->chart->setChart(m_chart);
    ui->chart->installEventFilter(this);

    const auto markers = m_chart->legend()->markers();
    for (QLegendMarker *marker : markers) {
        connect(marker, &QLegendMarker::clicked, this, &ADSBDemodGUI::legendMarkerClicked);
    }

    delete oldChart;
}

void ADSBDemodGUI::legendMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*>(sender());
    marker->series()->setVisible(!marker->series()->isVisible());
    marker->setVisible(true);

    // Dim the marker, if series is not visible
    qreal alpha = 1.0;

    if (!marker->series()->isVisible()) {
        alpha = 0.5;
    }

    QColor color;
    QBrush brush = marker->labelBrush();
    color = brush.color();
    color.setAlphaF(alpha);
    brush.setColor(color);
    marker->setLabelBrush(brush);

    brush = marker->brush();
    color = brush.color();
    color.setAlphaF(alpha);
    brush.setColor(color);
    marker->setBrush(brush);

    QPen pen = marker->pen();
    color = pen.color();
    color.setAlphaF(alpha);
    pen.setColor(color);
    marker->setPen(pen);
}
