///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "droneiddemodsettings.h"

const QList<qint64> DroneIDDemodSettings::m_frequencies2_4GHz = {
    2399500000LL, 2414500000LL, 2429500000LL, 2444500000LL, 2459500000LL, 2474500000LL
};

const QList<qint64> DroneIDDemodSettings::m_frequencies5_8GHz = {
    5721500000LL, 5731500000LL, 5741500000LL, 5756500000LL, 5761500000LL, 5771500000LL,
    5776500000LL, 5786500000LL, 5796500000LL, 5801500000LL, 5816500000LL, 5831500000LL
};

QList<qint64> DroneIDDemodSettings::getScanFrequencies(ScanBand band)
{
    switch (band)
    {
    case BAND_2_4_GHZ:
        return m_frequencies2_4GHz;
    case BAND_5_8_GHZ:
        return m_frequencies5_8GHz;
    default:
        return m_frequencies2_4GHz + m_frequencies5_8GHz;
    }
}

DroneIDDemodSettings::DroneIDDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DroneIDDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_detectionThreshold = 0.2f;
    m_turboIterations = 8;
    m_filterSerial = "";
    m_mapTimeout = 300;
    m_mapAltitude = MAP_ALTITUDE;
    m_scan = false;
    m_scanBand = BAND_2_4_GHZ;
    m_scanDwell = 1.5f;
    m_scanHold = 3.0f;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_logFilename = "dji_droneid_log.csv";
    m_logEnabled = false;
    m_useFileTime = false;
    m_rgbColor = QColor(255, 128, 0).rgb();
    m_title = "DJI DroneID Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    resetColumns();
}

void DroneIDDemodSettings::resetColumns()
{
    m_columnIndexes.clear();
    m_columnSizes.clear();

    for (int i = 0; i < DRONEIDDEMOD_COLUMNS; i++)
    {
        m_columnIndexes.append(i);
        m_columnSizes.append(-1); // Autosize
    }
}

QByteArray DroneIDDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_detectionThreshold);
    s.writeS32(3, m_turboIterations);
    s.writeString(4, m_filterSerial);
    s.writeS32(5, m_mapTimeout);
    s.writeBool(6, m_udpEnabled);
    s.writeString(7, m_udpAddress);
    s.writeU32(8, m_udpPort);
    s.writeString(9, m_logFilename);
    s.writeBool(10, m_logEnabled);
    s.writeBool(11, m_useFileTime);
    s.writeU32(12, m_rgbColor);
    s.writeString(13, m_title);

    if (m_channelMarker) {
        s.writeBlob(14, m_channelMarker->serialize());
    }

    s.writeS32(15, m_streamIndex);
    s.writeBool(16, m_useReverseAPI);
    s.writeString(17, m_reverseAPIAddress);
    s.writeU32(18, m_reverseAPIPort);
    s.writeU32(19, m_reverseAPIDeviceIndex);
    s.writeU32(20, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(21, m_rollupState->serialize());
    }

    s.writeS32(22, m_workspaceIndex);
    s.writeBlob(23, m_geometryBytes);
    s.writeBool(24, m_hidden);

    s.writeList(25, m_columnIndexes);
    s.writeList(26, m_columnSizes);

    s.writeBool(27, m_scan);
    s.writeS32(28, (int) m_scanBand);
    s.writeFloat(29, m_scanDwell);
    s.writeFloat(30, m_scanHold);
    s.writeS32(31, (int) m_mapAltitude);

    return s.final();
}

bool DroneIDDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        int32_t itmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readFloat(2, &m_detectionThreshold, 0.2f);
        d.readS32(3, &m_turboIterations, 8);
        d.readString(4, &m_filterSerial, "");
        d.readS32(5, &m_mapTimeout, 300);
        d.readBool(6, &m_udpEnabled, false);
        d.readString(7, &m_udpAddress, "127.0.0.1");
        d.readU32(8, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        d.readString(9, &m_logFilename, "dji_droneid_log.csv");
        d.readBool(10, &m_logEnabled, false);
        d.readBool(11, &m_useFileTime, false);
        d.readU32(12, &m_rgbColor, QColor(255, 128, 0).rgb());
        d.readString(13, &m_title, "DJI DroneID Demodulator");

        if (m_channelMarker)
        {
            d.readBlob(14, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(15, &m_streamIndex, 0);
        d.readBool(16, &m_useReverseAPI, false);
        d.readString(17, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(18, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(19, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(20, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(21, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(22, &m_workspaceIndex, 0);
        d.readBlob(23, &m_geometryBytes);
        d.readBool(24, &m_hidden, false);

        d.readList(25, &m_columnIndexes);
        d.readList(26, &m_columnSizes);

        // GUI expects an entry for every column
        if ((m_columnIndexes.size() != DRONEIDDEMOD_COLUMNS) || (m_columnSizes.size() != DRONEIDDEMOD_COLUMNS)) {
            resetColumns();
        }

        d.readBool(27, &m_scan, false);
        d.readS32(28, &itmp, (int) BAND_2_4_GHZ);
        m_scanBand = (ScanBand) std::clamp(itmp, (int32_t) BAND_2_4_GHZ, (int32_t) BAND_2_4_AND_5_8_GHZ);
        d.readFloat(29, &m_scanDwell, 1.5f);
        d.readFloat(30, &m_scanHold, 3.0f);
        d.readS32(31, &itmp, (int) MAP_ALTITUDE);
        m_mapAltitude = (MapAltitude) std::clamp(itmp, (int32_t) MAP_ALTITUDE, (int32_t) MAP_HEIGHT);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void DroneIDDemodSettings::applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("detectionThreshold")) {
        m_detectionThreshold = settings.m_detectionThreshold;
    }
    if (settingsKeys.contains("turboIterations")) {
        m_turboIterations = settings.m_turboIterations;
    }
    if (settingsKeys.contains("filterSerial")) {
        m_filterSerial = settings.m_filterSerial;
    }
    if (settingsKeys.contains("mapTimeout")) {
        m_mapTimeout = settings.m_mapTimeout;
    }
    if (settingsKeys.contains("mapAltitude")) {
        m_mapAltitude = settings.m_mapAltitude;
    }
    if (settingsKeys.contains("scan")) {
        m_scan = settings.m_scan;
    }
    if (settingsKeys.contains("scanBand")) {
        m_scanBand = settings.m_scanBand;
    }
    if (settingsKeys.contains("scanDwell")) {
        m_scanDwell = settings.m_scanDwell;
    }
    if (settingsKeys.contains("scanHold")) {
        m_scanHold = settings.m_scanHold;
    }
    if (settingsKeys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
    if (settingsKeys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (settingsKeys.contains("useFileTime")) {
        m_useFileTime = settings.m_useFileTime;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
    if (settingsKeys.contains("columnIndexes")) {
        m_columnIndexes = settings.m_columnIndexes;
    }
    if (settingsKeys.contains("columnSizes")) {
        m_columnSizes = settings.m_columnSizes;
    }
}

QString DroneIDDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("detectionThreshold") || force) {
        ostr << " m_detectionThreshold: " << m_detectionThreshold;
    }
    if (settingsKeys.contains("turboIterations") || force) {
        ostr << " m_turboIterations: " << m_turboIterations;
    }
    if (settingsKeys.contains("filterSerial") || force) {
        ostr << " m_filterSerial: " << m_filterSerial.toStdString();
    }
    if (settingsKeys.contains("mapTimeout") || force) {
        ostr << " m_mapTimeout: " << m_mapTimeout;
    }
    if (settingsKeys.contains("mapAltitude") || force) {
        ostr << " m_mapAltitude: " << m_mapAltitude;
    }
    if (settingsKeys.contains("scan") || force) {
        ostr << " m_scan: " << m_scan;
    }
    if (settingsKeys.contains("scanBand") || force) {
        ostr << " m_scanBand: " << m_scanBand;
    }
    if (settingsKeys.contains("scanDwell") || force) {
        ostr << " m_scanDwell: " << m_scanDwell;
    }
    if (settingsKeys.contains("scanHold") || force) {
        ostr << " m_scanHold: " << m_scanHold;
    }
    if (settingsKeys.contains("udpEnabled") || force) {
        ostr << " m_udpEnabled: " << m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress") || force) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort") || force) {
        ostr << " m_udpPort: " << m_udpPort;
    }
    if (settingsKeys.contains("logFilename") || force) {
        ostr << " m_logFilename: " << m_logFilename.toStdString();
    }
    if (settingsKeys.contains("logEnabled") || force) {
        ostr << " m_logEnabled: " << m_logEnabled;
    }
    if (settingsKeys.contains("useFileTime") || force) {
        ostr << " m_useFileTime: " << m_useFileTime;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex") || force) {
        ostr << " m_reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
