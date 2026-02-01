///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "aptdemodsettings.h"

APTDemodSettings::APTDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void APTDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 40000.0f;
    m_fmDeviation = 17000.0f;
    m_cropNoise = false;
    m_denoise = true;
    m_linearEqualise = false;
    m_histogramEqualise = false;
    m_precipitationOverlay = false;
    m_flip = false;
    m_channels = BOTH_CHANNELS;
    m_decodeEnabled = true;
    m_satelliteTrackerControl = true;
    m_satelliteName = "All";
    m_autoSave = false;
    m_autoSavePath = "";
    m_autoSaveMinScanLines = 200;
    m_saveCombined = true;
    m_saveSeparate = false;
    m_saveProjection = false;
    m_scanlinesPerImageUpdate = 20;
    m_transparencyThreshold = 100;
    m_opacityThreshold = 200;
    m_palettes.clear();
    m_palette = 0;
    m_horizontalPixelsPerDegree = 10;
    m_verticalPixelsPerDegree = 20;
    m_satTimeOffset = 0.0f;
    m_satYaw = 0.0f;

    m_rgbColor = QColor(216, 112, 169).rgb();
    m_title = "APT Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray APTDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);
    s.writeReal(3, m_rfBandwidth);
    s.writeReal(4, m_fmDeviation);
    s.writeBool(5, m_cropNoise);
    s.writeBool(6, m_denoise);
    s.writeBool(7, m_linearEqualise);
    s.writeBool(8, m_histogramEqualise);
    s.writeBool(9, m_precipitationOverlay);
    s.writeBool(10, m_flip);
    s.writeS32(11, (int)m_channels);
    s.writeBool(12, m_decodeEnabled);
    s.writeBool(13, m_satelliteTrackerControl);
    s.writeString(14, m_satelliteName);
    s.writeBool(15, m_autoSave);
    s.writeString(16, m_autoSavePath);
    s.writeS32(17, m_autoSaveMinScanLines);
    s.writeBool(18, m_saveProjection);
    s.writeS32(19, m_scanlinesPerImageUpdate);

    if (m_channelMarker) {
        s.writeBlob(20, m_channelMarker->serialize());
    }

    s.writeU32(21, m_rgbColor);
    s.writeString(22, m_title);
    s.writeBool(23, m_useReverseAPI);
    s.writeString(24, m_reverseAPIAddress);
    s.writeU32(25, m_reverseAPIPort);
    s.writeU32(26, m_reverseAPIDeviceIndex);
    s.writeU32(27, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(28, m_rollupState->serialize());
    }

    s.writeBool(29, m_saveCombined);
    s.writeBool(30, m_saveSeparate);
    s.writeS32(31, m_transparencyThreshold);
    s.writeS32(32, m_opacityThreshold);
    s.writeString(33, m_palettes.join(";"));
    s.writeS32(34, m_palette);
    s.writeS32(35, m_horizontalPixelsPerDegree);
    s.writeS32(36, m_verticalPixelsPerDegree);
    s.writeFloat(37, m_satTimeOffset);
    s.writeFloat(38, m_satYaw);
    s.writeS32(39, m_workspaceIndex);
    s.writeBlob(40, m_geometryBytes);
    s.writeBool(41, m_hidden);

    return s.final();
}

bool APTDemodSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_streamIndex, 0);
        d.readReal(3, &m_rfBandwidth, 40000.0f);
        d.readReal(4, &m_fmDeviation, 17000.0f);
        d.readBool(5, &m_cropNoise, false);
        d.readBool(6, &m_denoise, true);
        d.readBool(7, &m_linearEqualise, false);
        d.readBool(8, &m_histogramEqualise, false);
        d.readBool(9, &m_precipitationOverlay, false);
        d.readBool(10, &m_flip, false);
        d.readS32(11, (int *)&m_channels, (int)BOTH_CHANNELS);
        d.readBool(12, &m_decodeEnabled, true);
        d.readBool(13, &m_satelliteTrackerControl, true);
        d.readString(14, &m_satelliteName, "All");
        d.readBool(15, &m_autoSave, false);
        d.readString(16, &m_autoSavePath, "");
        d.readS32(17, &m_autoSaveMinScanLines, 200);
        d.readBool(18, &m_saveProjection, false);
        d.readS32(19, &m_scanlinesPerImageUpdate, 20);

        if (m_channelMarker)
        {
            d.readBlob(20, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(21, &m_rgbColor, QColor(216, 112, 169).rgb());
        d.readString(22, &m_title, "APT Demodulator");
        d.readBool(23, &m_useReverseAPI, false);
        d.readString(24, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(25, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(26, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(27, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(28, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readBool(29, &m_saveCombined, true);
        d.readBool(30, &m_saveSeparate, false);
        d.readS32(31, &m_transparencyThreshold, 100);
        d.readS32(32, &m_opacityThreshold, 200);
        d.readString(33, &strtmp);
        m_palettes = strtmp.split(";");
        m_palettes.removeAll("");
        d.readS32(34, &m_palette, 0);
        d.readS32(35, &m_horizontalPixelsPerDegree, 10);
        d.readS32(36, &m_verticalPixelsPerDegree, 20);
        d.readFloat(37, &m_satTimeOffset, 0.0f);
        d.readFloat(38, &m_satYaw, 0.0f);
        d.readS32(39, &m_workspaceIndex, 0);
        d.readBlob(40, &m_geometryBytes);
        d.readBool(41, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void APTDemodSettings::applySettings(const QStringList& settingsKeys, const APTDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (settingsKeys.contains("cropNoise")) {
        m_cropNoise = settings.m_cropNoise;
    }
    if (settingsKeys.contains("denoise")) {
        m_denoise = settings.m_denoise;
    }
    if (settingsKeys.contains("linearEqualise")) {
        m_linearEqualise = settings.m_linearEqualise;
    }
    if (settingsKeys.contains("histogramEqualise")) {
        m_histogramEqualise = settings.m_histogramEqualise;
    }
    if (settingsKeys.contains("precipitationOverlay")) {
        m_precipitationOverlay = settings.m_precipitationOverlay;
    }
    if (settingsKeys.contains("flip")) {
        m_flip = settings.m_flip;
    }
    if (settingsKeys.contains("channels")) {
        m_channels = settings.m_channels;
    }
    if (settingsKeys.contains("decodeEnabled")) {
        m_decodeEnabled = settings.m_decodeEnabled;
    }
    if (settingsKeys.contains("satelliteTrackerControl")) {
        m_satelliteTrackerControl = settings.m_satelliteTrackerControl;
    }
    if (settingsKeys.contains("satelliteName")) {
        m_satelliteName = settings.m_satelliteName;
    }
    if (settingsKeys.contains("autoSave")) {
        m_autoSave = settings.m_autoSave;
    }
    if (settingsKeys.contains("autoSavePath")) {
        m_autoSavePath = settings.m_autoSavePath;
    }
    if (settingsKeys.contains("autoSaveMinScanLines")) {
        m_autoSaveMinScanLines = settings.m_autoSaveMinScanLines;
    }
    if (settingsKeys.contains("saveCombined")) {
        m_saveCombined = settings.m_saveCombined;
    }
    if (settingsKeys.contains("saveSeparate")) {
        m_saveSeparate = settings.m_saveSeparate;
    }
    if (settingsKeys.contains("saveProjection")) {
        m_saveProjection = settings.m_saveProjection;
    }
    if (settingsKeys.contains("scanlinesPerImageUpdate")) {
        m_scanlinesPerImageUpdate = settings.m_scanlinesPerImageUpdate;
    }
    if (settingsKeys.contains("transparencyThreshold")) {
        m_transparencyThreshold = settings.m_transparencyThreshold;
    }
    if (settingsKeys.contains("opacityThreshold")) {
        m_opacityThreshold = settings.m_opacityThreshold;
    }
    if (settingsKeys.contains("palettes")) {
        m_palettes = settings.m_palettes;
    }
    if (settingsKeys.contains("palette")) {
        m_palette = settings.m_palette;
    }
    if (settingsKeys.contains("horizontalPixelsPerDegree")) {
        m_horizontalPixelsPerDegree = settings.m_horizontalPixelsPerDegree;
    }
    if (settingsKeys.contains("verticalPixelsPerDegree")) {
        m_verticalPixelsPerDegree = settings.m_verticalPixelsPerDegree;
    }
    if (settingsKeys.contains("satTimeOffset")) {
        m_satTimeOffset = settings.m_satTimeOffset;
    }
    if (settingsKeys.contains("satYaw")) {
        m_satYaw = settings.m_satYaw;
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
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
    if (settingsKeys.contains("tle")) {
        m_tle = settings.m_tle;
    }
    if (settingsKeys.contains("aosDateTime")) {
        m_aosDateTime = settings.m_aosDateTime;
    }
    if (settingsKeys.contains("northToSouth")) {
        m_northToSouth = settings.m_northToSouth;
    }
}

QString APTDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("fmDeviation") || force) {
        ostr << " m_fmDeviation: " << m_fmDeviation;
    }
    if (settingsKeys.contains("cropNoise") || force) {
        ostr << " m_cropNoise: " << m_cropNoise;
    }
    if (settingsKeys.contains("denoise") || force) {
        ostr << " m_denoise: " << m_denoise;
    }
    if (settingsKeys.contains("linearEqualise") || force) {
        ostr << " m_linearEqualise: " << m_linearEqualise;
    }
    if (settingsKeys.contains("histogramEqualise") || force) {
        ostr << " m_histogramEqualise: " << m_histogramEqualise;
    }
    if (settingsKeys.contains("precipitationOverlay") || force) {
        ostr << " m_precipitationOverlay: " << m_precipitationOverlay;
    }
    if (settingsKeys.contains("flip") || force) {
        ostr << " m_flip: " << m_flip;
    }
    if (settingsKeys.contains("channels") || force) {
        ostr << " m_channels: " << m_channels;
    }
    if (settingsKeys.contains("decodeEnabled") || force) {
        ostr << " m_decodeEnabled: " << m_decodeEnabled;
    }
    if (settingsKeys.contains("satelliteTrackerControl") || force) {
        ostr << " m_satelliteTrackerControl: " << m_satelliteTrackerControl;
    }
    if (settingsKeys.contains("satelliteName") || force) {
        ostr << " m_satelliteName: " << m_satelliteName.toStdString();
    }
    if (settingsKeys.contains("autoSave") || force) {
        ostr << " m_autoSave: " << m_autoSave;
    }
    if (settingsKeys.contains("autoSavePath") || force) {
        ostr << " m_autoSavePath: " << m_autoSavePath.toStdString();
    }
    if (settingsKeys.contains("autoSaveMinScanLines") || force) {
        ostr << " m_autoSaveMinScanLines: " << m_autoSaveMinScanLines;
    }
    if (settingsKeys.contains("saveCombined") || force) {
        ostr << " m_saveCombined: " << m_saveCombined;
    }
    if (settingsKeys.contains("saveSeparate") || force) {
        ostr << " m_saveSeparate: " << m_saveSeparate;
    }
    if (settingsKeys.contains("saveProjection") || force) {
        ostr << " m_saveProjection: " << m_saveProjection;
    }
    if (settingsKeys.contains("scanlinesPerImageUpdate") || force) {
        ostr << " m_scanlinesPerImageUpdate: " << m_scanlinesPerImageUpdate;
    }
    if (settingsKeys.contains("transparencyThreshold") || force) {
        ostr << " m_transparencyThreshold: " << m_transparencyThreshold;
    }
    if (settingsKeys.contains("opacityThreshold") || force) {
        ostr << " m_opacityThreshold: " << m_opacityThreshold;
    }
    if (settingsKeys.contains("palettes") || force) {
        ostr << " m_palettes: " << m_palettes.join(";").toStdString();
    }
    if (settingsKeys.contains("palette") || force) {
        ostr << " m_palette: " << m_palette;
    }
    if (settingsKeys.contains("horizontalPixelsPerDegree") || force) {
        ostr << " m_horizontalPixelsPerDegree: " << m_horizontalPixelsPerDegree;
    }
    if (settingsKeys.contains("verticalPixelsPerDegree") || force) {
        ostr << " m_verticalPixelsPerDegree: " << m_verticalPixelsPerDegree;
    }
    if (settingsKeys.contains("satTimeOffset") || force) {
        ostr << " m_satTimeOffset: " << m_satTimeOffset;
    }
    if (settingsKeys.contains("satYaw") || force) {
        ostr << " m_satYaw: " << m_satYaw;
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
    if (settingsKeys.contains("tle") || force) {
        ostr << " m_tle: " << m_tle.toStdString();
    }
    if (settingsKeys.contains("aosDateTime") || force) {
        ostr << " m_aosDateTime: " << m_aosDateTime.toString().toStdString();
    }
    if (settingsKeys.contains("northToSouth") || force) {
        ostr << " m_northToSouth: " << m_northToSouth;
    }

    return QString(ostr.str().c_str());
}
