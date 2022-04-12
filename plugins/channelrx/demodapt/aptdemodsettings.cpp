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

#include "dsp/dspengine.h"
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


