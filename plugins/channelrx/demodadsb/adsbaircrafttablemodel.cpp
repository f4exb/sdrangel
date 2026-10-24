///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QIcon>
#include <QSize>
#include <QBrush>

#include "adsbaircrafttablemodel.h"
#include "adsbdemodgui.h"

static const QString g_columnNames[ADSBDEMOD_COLUMNS] = {
    QStringLiteral("ICAO ID"),
    QStringLiteral("Callsign"),
    QStringLiteral("ATC Callsign"),
    QStringLiteral("Aircraft"),
    QStringLiteral("Type"),
    QStringLiteral("Sideview"),
    QStringLiteral("Airline"),
    QStringLiteral("Country"),
    QStringLiteral("GS (kn)"),
    QStringLiteral("TAS (kn)"),
    QStringLiteral("IAS (kn)"),
    QStringLiteral("Mach"),
    QStringLiteral("Sel Alt (ft)"),
    QStringLiteral("Alt (ft)"),
    QStringLiteral("VR (ft/m)"),
    QStringLiteral("Sel Hd (\u00B0)"),
    QStringLiteral("Hd (\u00B0)"),
    QStringLiteral("Trk (\u00B0)"),
    QStringLiteral("TR (\u00B0/s)"),
    QStringLiteral("Roll (\u00B0)"),
    QStringLiteral("D (km)"),
    QStringLiteral("Az/El (\u00B0)"),
    QStringLiteral("Cat"),
    QStringLiteral("Status"),
    QStringLiteral("Squawk"),
    QStringLiteral("Ident"),
    QStringLiteral("Reg"),
    QStringLiteral("Registered"),
    QStringLiteral("Manufacturer"),
    QStringLiteral("Owner"),
    QStringLiteral("Operator"),
    QStringLiteral("AP"),
    QStringLiteral("V Mode"),
    QStringLiteral("L Mode"),
    QStringLiteral("TCAS"),
    QStringLiteral("ACAS"),
    QStringLiteral("RA"),
    QStringLiteral("Max AS (kn)"),
    QStringLiteral("Version"),
    QStringLiteral("L (m)"),
    QStringLiteral("W (m)"),
    QStringLiteral("QNH (mb)"),
    QStringLiteral("H Wnd (kn)"),
    QStringLiteral("OAT (C)"),
    QStringLiteral("Wnd (kn)"),
    QStringLiteral("Wnd (\u00B0)"),
    QStringLiteral("P (hPa)"),
    QStringLiteral("T (C)"),
    QStringLiteral("U (%)"),
    QStringLiteral("Lat (\u00B0)"),
    QStringLiteral("Lon (\u00B0)"),
    QStringLiteral("IC"),
    QStringLiteral("Updated"),
    QStringLiteral("Frames"),
    QStringLiteral("ADS-B FC"),
    QStringLiteral("Mode S FC"),
    QStringLiteral("Non-transponder"),
    QStringLiteral("TIS-B FC"),
    QStringLiteral("ADS-R FC"),
    QStringLiteral("Rc"),
    QStringLiteral("NACp"),
    QStringLiteral("NACv"),
    QStringLiteral("GVA"),
    QStringLiteral("NIC"),
    QStringLiteral("NICbaro"),
    QStringLiteral("SIL"),
    QStringLiteral("Correlation"),
    QStringLiteral("RSSI"),
    QStringLiteral("Flight Status"),
    QStringLiteral("Dep"),
    QStringLiteral("Arr"),
    QStringLiteral("Stops"),
    QStringLiteral("STD"),
    QStringLiteral("ETD"),
    QStringLiteral("ATD"),
    QStringLiteral("STA"),
    QStringLiteral("ETA"),
    QStringLiteral("ATA")
};

static const QString g_columnTips[ADSBDEMOD_COLUMNS] = {
    QStringLiteral(""),
    QStringLiteral("Callsign. Links to www.flightradar24.com"),
    QStringLiteral("Airline callsign used by ATC"),
    QStringLiteral("Aircraft model"),
    QStringLiteral("ICAO aircraft type designator"),
    QStringLiteral(""),
    QStringLiteral("Airline logo"),
    QStringLiteral("Country of registration"),
    QStringLiteral("Groundspeed in knots or kilometers per hour"),
    QStringLiteral("True airpeed in knots or kilometers per hour"),
    QStringLiteral("Indicated airspeed in knots or kilometers per hour"),
    QStringLiteral("Mach number"),
    QStringLiteral("Selected altitude in feet or metres (As set on MCP/FCU or by FMS)"),
    QStringLiteral("Altitude in feet or metres"),
    QStringLiteral("Vertical climb rate in feet per minute or metres per second"),
    QStringLiteral("Selected heading in degrees (As set on MCP/FCU or by FMS)"),
    QStringLiteral("Aircraft heading in degrees"),
    QStringLiteral("Aircraft track in degrees"),
    QStringLiteral("Turn rate in degrees per second"),
    QStringLiteral("Roll angle in degrees"),
    QStringLiteral("Range or distance of aircraft from home location"),
    QStringLiteral("Azimuth and elevation to aircraft from My Position. Double click to set as target."),
    QStringLiteral("Aircraft standard category"),
    QStringLiteral("Aircraft emergency status"),
    QStringLiteral("Mode-A transponder code"),
    QStringLiteral("Ident switch active"),
    QStringLiteral("Aircraft registration"),
    QStringLiteral("Date aircraft was registered"),
    QStringLiteral("Aircraft manufacturer"),
    QStringLiteral("Owner of the aircraft"),
    QStringLiteral("Aircraft operator ICAO code"),
    QStringLiteral("Autopilot enabled"),
    QStringLiteral("MCP/FCU vertical mode"),
    QStringLiteral("MCP/FCU lateral mode"),
    QStringLiteral("TCAS operational. RA in red if active"),
    QStringLiteral("Airborne collision avoidance system capability"),
    QStringLiteral("ACAS active resolition advisory"),
    QStringLiteral("Maximum speed in knots"),
    QStringLiteral("ADS-B version"),
    QStringLiteral("Aircraft length in metres"),
    QStringLiteral("Aircraft width in metres"),
    QStringLiteral("Barometer setting in millibars"),
    QStringLiteral("Headwind in knots or kilometers per hour"),
    QStringLiteral("Outside air temperature in degrees Celsius estimated from Mach and TAS"),
    QStringLiteral("Wind speed in knots"),
    QStringLiteral("Wind direction in degrees"),
    QStringLiteral("Static air pressure in hectopascals"),
    QStringLiteral("Static air temperature in degrees Celsius"),
    QStringLiteral("Humidity in percent"),
    QStringLiteral(""),
    QStringLiteral(""),
    QStringLiteral("Interrogator code (II/SI)"),
    QStringLiteral("Time when the last ADS-B message from this aircraft was received."),
    QStringLiteral("Number of ADS-B and Mode S frames received from this aircraft"),
    QStringLiteral("Number of ADS-B frames received from this aircraft"),
    QStringLiteral("Number of Mode S frames received from this aircraft"),
    QStringLiteral("Non transponder device frame count"),
    QStringLiteral("Number of TIS-B frames received with this aircraft's ICAO"),
    QStringLiteral("Number of ADS-R frames received with this aircraft's ICAO"),
    QStringLiteral("Horizontal containment radius limit"),
    QStringLiteral("Navigation accuracy category - 95% horizontal position"),
    QStringLiteral("Navigation accuracy category - velocity"),
    QStringLiteral("Geometric vertical accuracy"),
    QStringLiteral("Navigation integrity category"),
    QStringLiteral("Barometric altitude integrity code"),
    QStringLiteral("Surveillance integrity level"),
    QStringLiteral("Correlation values for received frames. min/avg/max"),
    QStringLiteral("Received Signal Strength Indicator (dBFS)"),
    QStringLiteral("Status of flight"),
    QStringLiteral("Departure airport"),
    QStringLiteral("Arrival airport"),
    QStringLiteral(""),
    QStringLiteral("Scheduled time of departure"),
    QStringLiteral("Estimated time of departure"),
    QStringLiteral("Actual time of departure"),
    QStringLiteral("Scheduled time of arrival"),
    QStringLiteral("Estimated time of arrival"),
    QStringLiteral("Actual time of arrival")
};

bool ADSBAircraftSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // The values are compared here rather than by the base class, which would fetch
    // them from the model a second time - and sorting runs on every update
    const QVariant l = sourceModel()->data(left, ADSBAircraftTableModel::SortRole);
    const QVariant r = sourceModel()->data(right, ADSBAircraftTableModel::SortRole);
    const bool lString = l.userType() == QMetaType::QString;
    const bool rString = r.userType() == QMetaType::QString;
    const bool lEmpty = l.isNull() || (lString && l.toString().isEmpty());
    const bool rEmpty = r.isNull() || (rString && r.toString().isEmpty());

    // Empty cells sort before values in every column, however the column is typed
    if (lEmpty || rEmpty) {
        return lEmpty && !rEmpty;
    }
    if (!lString && !rString) {
        return l.toDouble() < r.toDouble();
    }
    return QString::localeAwareCompare(l.toString(), r.toString()) < 0;
}

static qreal g_devicePixelRatio = 1.0;
static QHash<QString, QIcon> g_icons;

void ADSBAircraftTableModel::setDevicePixelRatio(qreal dpr)
{
    if (dpr != g_devicePixelRatio)
    {
        g_devicePixelRatio = dpr;
        g_icons.clear();
    }
}

// Icons are loaded on demand and kept, as the same few appear on many rows
QVariant ADSBAircraftTableModel::icon(const QString& path, const QSize& size)
{
    if (path.isEmpty()) {
        return QVariant();
    }

    auto i = g_icons.find(path);

    if (i == g_icons.end())
    {
        QPixmap pixmap(path);

        if (!pixmap.isNull() && (g_devicePixelRatio > 1.0))
        {
            // Draw at the full size the table asks for, rather than being limited to
            // the image's own pixel size on a scaled display
            pixmap = pixmap.scaled(size * g_devicePixelRatio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            pixmap.setDevicePixelRatio(g_devicePixelRatio);
        }
        i = g_icons.insert(path, QIcon(pixmap));
    }
    return QVariant(i.value());
}

// Row numbers are the position in the table, not in the underlying model, so they
// count up from one however the table is sorted
QVariant ADSBAircraftSortProxy::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Vertical) && (role == Qt::DisplayRole)) {
        return section + 1;
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

const QString *ADSBAircraftTableModel::columnNames()
{
    return g_columnNames;
}

ADSBAircraftTableModel::ADSBAircraftTableModel(QObject *parent) :
    QAbstractTableModel(parent),
    m_settings(nullptr)
{
}

void ADSBAircraftTableModel::addAircraft(Aircraft *aircraft)
{
    beginInsertRows(QModelIndex(), m_aircraft.size(), m_aircraft.size());
    m_aircraft.append(aircraft);
    endInsertRows();
}

void ADSBAircraftTableModel::removeAircraft(Aircraft *aircraft)
{
    int row = m_aircraft.indexOf(aircraft);

    if (row >= 0)
    {
        beginRemoveRows(QModelIndex(), row, row);
        m_aircraft.removeAt(row);
        endRemoveRows();
    }
}

void ADSBAircraftTableModel::clear()
{
    beginResetModel();
    m_aircraft.clear();
    endResetModel();
}

int ADSBAircraftTableModel::rowForAircraft(const Aircraft *aircraft) const
{
    return m_aircraft.indexOf(const_cast<Aircraft *>(aircraft));
}

void ADSBAircraftTableModel::flushDirty()
{
    for (int row = 0; row < m_aircraft.size(); row++)
    {
        Aircraft *aircraft = m_aircraft[row];

        if (!aircraft->cellsDirty()) {
            continue;
        }

        // One dataChanged spanning the changed columns, rather than one per cell
        int first = -1;
        int last = -1;
        for (int col = 0; col < ADSBDEMOD_COLUMNS; col++)
        {
            if (aircraft->cellDirty(col))
            {
                if (first < 0) {
                    first = col;
                }
                last = col;
            }
        }
        aircraft->clearDirtyCells();

        if (first >= 0) {
            emit dataChanged(index(row, first), index(row, last));
        }
    }
}

void ADSBAircraftTableModel::allDataChanged()
{
    if (!m_aircraft.isEmpty())
    {
        for (Aircraft *aircraft : m_aircraft) {
            aircraft->clearDirtyCells();
        }
        emit dataChanged(index(0, 0), index(m_aircraft.size() - 1, ADSBDEMOD_COLUMNS - 1));
    }
}

void ADSBAircraftTableModel::aircraftDataChanged(const Aircraft *aircraft)
{
    int row = rowForAircraft(aircraft);

    if (row >= 0) {
        emit dataChanged(index(row, 0), index(row, ADSBDEMOD_COLUMNS - 1));
    }
}

int ADSBAircraftTableModel::rowCount(const QModelIndex& parent) const
{
    (void) parent;
    return m_aircraft.size();
}

int ADSBAircraftTableModel::columnCount(const QModelIndex& parent) const
{
    (void) parent;
    return ADSBDEMOD_COLUMNS;
}

QVariant ADSBAircraftTableModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    int col = index.column();

    if ((row < 0) || (row >= m_aircraft.size()) || (col < 0) || (col >= ADSBDEMOD_COLUMNS)) {
        return QVariant();
    }

    const Aircraft *aircraft = m_aircraft[row];

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::EditRole:
        // Range is held as a number, so that it sorts numerically
        if (col == ADSB_COL_RANGE) {
            return ADSBAircraft::rangeText(aircraft->cell(col));
        }
        return aircraft->cell(col);

    case SortRole:
        return aircraft->cell(col);

    case Qt::DecorationRole:
        switch (col)
        {
        case ADSB_COL_AIRLINE:  return icon(aircraft->m_airlineIconPath, QSize(85, 20));
        case ADSB_COL_SIDEVIEW: return icon(aircraft->m_sideviewIconPath, QSize(85, 20));
        case ADSB_COL_COUNTRY:  return icon(aircraft->m_flagIconPath, QSize(40, 20));
        default: return QVariant();
        }

    case Qt::SizeHintRole:
        switch (col)
        {
        case ADSB_COL_AIRLINE:
        case ADSB_COL_SIDEVIEW:
            return QVariant(QSize(85, 20));
        case ADSB_COL_COUNTRY:
            return QVariant(QSize(40, 20));
        default:
            return QVariant();
        }

    case Qt::ForegroundRole:
        if ((col == ADSB_COL_TCAS) && aircraft->m_tcasAlert) {
            return QBrush(Qt::red);
        }
        return QVariant();

    default:
        return QVariant();
    }
}

QVariant ADSBAircraftTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
    {
        // Row numbers, which are also the count of aircraft being tracked
        if ((role == Qt::DisplayRole) && (section >= 0) && (section < m_aircraft.size())) {
            return section + 1;
        }
        return QVariant();
    }

    if ((section < 0) || (section >= ADSBDEMOD_COLUMNS)) {
        return QVariant();
    }

    if (role == Qt::DisplayRole)
    {
        // These headings carry the units, which the user can switch
        if (m_settings && m_settings->m_siUnits)
        {
            switch (section)
            {
            case ADSB_COL_ALTITUDE:           return QStringLiteral("Alt (m)");
            case ADSB_COL_VERTICALRATE:       return QStringLiteral("VR (m/s)");
            case ADSB_COL_SEL_ALTITUDE:       return QStringLiteral("Sel Alt (m)");
            case ADSB_COL_GROUND_SPEED:       return QStringLiteral("GS (kph)");
            case ADSB_COL_TRUE_AIRSPEED:      return QStringLiteral("TAS (kph)");
            case ADSB_COL_INDICATED_AIRSPEED: return QStringLiteral("IAS (kph)");
            case ADSB_COL_HEADWIND:           return QStringLiteral("H Wnd (kph)");
            case ADSB_COL_WIND_SPEED:         return QStringLiteral("Wnd (kph)");
            default: break;
            }
        }
        else
        {
            switch (section)
            {
            case ADSB_COL_ALTITUDE:           return QStringLiteral("Alt (ft)");
            case ADSB_COL_VERTICALRATE:       return QStringLiteral("VR (ft/m)");
            case ADSB_COL_SEL_ALTITUDE:       return QStringLiteral("Sel Alt (ft)");
            case ADSB_COL_GROUND_SPEED:       return QStringLiteral("GS (kn)");
            case ADSB_COL_TRUE_AIRSPEED:      return QStringLiteral("TAS (kn)");
            case ADSB_COL_INDICATED_AIRSPEED: return QStringLiteral("IAS (kn)");
            case ADSB_COL_HEADWIND:           return QStringLiteral("H Wnd (kn)");
            case ADSB_COL_WIND_SPEED:         return QStringLiteral("Wnd (kn)");
            default: break;
            }
        }
        return g_columnNames[section];
    }
    else if ((role == Qt::ToolTipRole) && !g_columnTips[section].isEmpty()) {
        return g_columnTips[section];
    }

    return QVariant();
}
