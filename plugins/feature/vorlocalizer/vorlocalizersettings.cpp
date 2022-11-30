///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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

#include <QColor>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "vorlocalizersettings.h"

VORLocalizerSettings::VORLocalizerSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void VORLocalizerSettings::resetToDefaults()
{
    m_rgbColor = QColor(255, 255, 0).rgb();
    m_title = "VOR Localizer";
    m_magDecAdjust = true;
    m_rrTime = 20;
    m_centerShift = 20000;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
#ifdef LINUX
    m_mapProvider = "mapboxgl"; // osm maps do not work in some versions of Linux https://github.com/f4exb/sdrangel/issues/1169 & 1369
#else
    m_mapProvider = "osm";
#endif

    for (int i = 0; i < VORDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray VORLocalizerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU32(7, m_rgbColor);
    s.writeString(9, m_title);
    s.writeBool(10, m_magDecAdjust);
    s.writeS32(11, m_rrTime);
    s.writeS32(12, m_centerShift);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIFeatureSetIndex);
    s.writeU32(18, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(19, m_rollupState->serialize());
    }

    s.writeS32(20, m_workspaceIndex);
    s.writeBlob(21, m_geometryBytes);
    s.writeString(22, m_mapProvider);

    for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
        s.writeS32(100 + i, m_columnIndexes[i]);
    }

    for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
        s.writeS32(200 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool VORLocalizerSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;

        d.readBlob(6, &bytetmp);
        d.readU32(7, &m_rgbColor);
        d.readString(9, &m_title, "VOR Localizer");
        d.readBool(10, &m_magDecAdjust, true);
        d.readS32(11, &m_rrTime, 20);
        d.readS32(12, &m_centerShift, 20000);
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(17, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(18, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(19, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(20, &m_workspaceIndex, 0);
        d.readBlob(21, &m_geometryBytes);
#ifdef LINUX
        d.readString(22, &m_mapProvider, "mapboxgl");
#else
        d.readString(22, &m_mapProvider, "osm");
#endif

        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
            d.readS32(100 + i, &m_columnIndexes[i], i);
        }

        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
            d.readS32(200 + i, &m_columnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool VORLocalizerSettings::VORChannel::operator<(const VORChannel& other) const
{
    if (m_frequency != other.m_frequency) {
        return m_frequency < other.m_frequency;
    }
    if (m_subChannelId != other.m_subChannelId) {
        return m_subChannelId < other.m_subChannelId;
    }

    return false;
}

void VORLocalizerSettings::applySettings(const QStringList& settingsKeys, const VORLocalizerSettings& settings)
{
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("magDecAdjust")) {
        m_magDecAdjust = settings.m_magDecAdjust;
    }
    if (settingsKeys.contains("rrTime")) {
        m_rrTime = settings.m_rrTime;
    }
    if (settingsKeys.contains("centerShift")) {
        m_centerShift = settings.m_centerShift;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("mapProvider")) {
        m_mapProvider = settings.m_mapProvider;
    }

    if (settingsKeys.contains("columnIndexes"))
    {
        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
            m_columnIndexes[i] = settings.m_columnIndexes[i];
        }
    }

    if (settingsKeys.contains("columnSizes"))
    {
        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
            m_columnSizes[i] = settings.m_columnSizes[i];
        }
    }
}

QString VORLocalizerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("magDecAdjust") || force) {
        ostr << " m_magDecAdjust: " << m_magDecAdjust;
    }
    if (settingsKeys.contains("rrTime") || force) {
        ostr << " m_rrTime: " << m_rrTime;
    }
    if (settingsKeys.contains("centerShift") || force) {
        ostr << " m_centerShift: " << m_centerShift;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        ostr << " m_reverseAPIFeatureSetIndex: " << m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex") || force) {
        ostr << " m_reverseAPIFeatureIndex: " << m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("mapProvider") || force) {
        ostr << " m_mapProvider: " << m_mapProvider.toStdString();
    }

    if (settingsKeys.contains("columnIndexes"))
    {
        ostr << "m_columnIndexes:";

        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
            ostr << " " << m_columnIndexes[i];
        }
    }

    if (settingsKeys.contains("columnSizes"))
    {
        ostr << "m_columnSizes:";

        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
            ostr << " " << m_columnSizes[i];
        }
    }

    return QString(ostr.str().c_str());
}
