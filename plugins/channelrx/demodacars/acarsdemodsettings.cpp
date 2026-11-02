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
#include "acarsdemodsettings.h"
#include "acarsaero.h"

AcarsDemodSettings::AcarsDemodSettings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AcarsDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_mode = ACARS;
    m_aeroChannel = AcarsAeroReceiver::Submode600P;
    m_iridiumBandwidth = AcarsDemodSettings::IridiumScanWidths - 1;
    m_rfBandwidth = 12500.0f;
    // Pre-key detection threshold, as a fraction of the AM modulation index seen at 2400 Hz.
    // A real pre-key reads about 0.85 (ARINC 618 section 4.4.3 asks for at least 85 percent
    // modulation); noise reads near zero. Being a ratio it does not move with signal level,
    // so this is a false alarm setting rather than something to tune per station.
    m_correlationThreshold = 0.25f;
    m_filter = "";
    m_filterColumn = 0;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_scopeCh1 = 3;     // Envelope
    m_scopeCh2 = 4;     // Pre-key detector statistic
    m_displayPhotos = true;
    m_hideNoInfo = true;
    m_showDate = true;
    m_splitterState = QByteArray();
    m_displayChart = false;
    m_logFilename = "acars_log.csv";
    m_logEnabled = false;
    // The airframes.io ACARS (acarsdec) station type ingests UDP on 5550; only some
    // other station types (e.g. VDL2/vdlm2dec on 5555) are TCP

    m_rgbColor = QColor(25, 205, 2).rgb();
    m_title = "ACARS Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < ACARSDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray AcarsDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);
    s.writeString(3, m_filter);
    s.writeS32(52, m_filterColumn);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeString(9, m_title);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);

    s.writeFloat(20, m_rfBandwidth);
    s.writeBool(22, m_udpEnabled);
    s.writeString(23, m_udpAddress);
    s.writeU32(24, m_udpPort);
    s.writeS32(25, m_scopeCh1);
    s.writeS32(26, m_scopeCh2);
    s.writeFloat(27, m_correlationThreshold);
    if (m_scopeGUI) {
        s.writeBlob(28, m_scopeGUI->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(31, m_rollupState->serialize());
    }
    s.writeS32(33, m_workspaceIndex);
    s.writeBlob(34, m_geometryBytes);
    s.writeBool(35, m_hidden);
    s.writeBool(36, m_displayPhotos);
    s.writeString(37, m_logFilename);
    s.writeBool(38, m_logEnabled);
    s.writeS32(39, m_mode);
    s.writeS32(53, m_aeroChannel);
    s.writeS32(56, m_iridiumBandwidth);
    // Key 40 stored the opposite question, "show these frames", so it is written
    // inverted rather than moved to a new key - that way an existing preset keeps
    // behaving exactly as it did, whichever way it was set
    s.writeBool(40, !m_hideNoInfo);
    s.writeBool(54, m_showDate);
    s.writeBlob(55, m_splitterState);
    s.writeBool(41, m_displayChart);

    for (int i = 0; i < ACARSDEMOD_COLUMNS; i++)
        s.writeS32(100 + i, m_columnIndexes[i]);
    for (int i = 0; i < ACARSDEMOD_COLUMNS; i++)
        s.writeS32(200 + i, m_columnSizes[i]);

    return s.final();
}

bool AcarsDemodSettings::deserialize(const QByteArray& data)
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
        // Key 3 was previously the address filter, which migrates as the pattern with
        // the default Registration column; key 4 (the flight filter) is retired
        d.readString(3, &m_filter, "");
        d.readS32(52, &m_filterColumn, 0);
        if ((m_filterColumn < 0) || (m_filterColumn > 3)) {
            m_filterColumn = 0;
        }
        d.readBlob(6, &bytetmp);

        if (m_channelMarker) {
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(7, &m_rgbColor, QColor(25, 205, 2).rgb());
        d.readString(9, &m_title, "ACARS Demodulator");
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(17, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(18, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        d.readFloat(20, &m_rfBandwidth, 12500.0f);
        // The sink turns this straight into an interpolator cutoff (bandwidth / 2.2), so
        // a corrupt or hand-edited preset carrying zero or a negative would build a
        // degenerate filter. The GUI's slider covers 1 to 40 kHz; nothing else clamped it.
        if ((m_rfBandwidth < 1000.0f) || (m_rfBandwidth > 40000.0f)) {
            m_rfBandwidth = 12500.0f;
        }

        d.readBool(22, &m_udpEnabled);
        d.readString(23, &m_udpAddress);
        d.readU32(24, &utmp);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }
        d.readS32(25, &m_scopeCh1, 0);
        d.readS32(26, &m_scopeCh2, 0);
        d.readFloat(27, &m_correlationThreshold, 0.25f);

        // The threshold changed meaning when the demodulator became coherent: it used to be
        // a raw correlation value in the tens to hundreds, now it is a ratio in 0 to 1. Any
        // setting saved by an older version would disable detection entirely.
        if (m_correlationThreshold > 1.0f) {
            m_correlationThreshold = 0.25f;
        }


        if (m_scopeGUI)
        {
            d.readBlob(28, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }
        if (m_rollupState)
        {
            d.readBlob(31, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }
        d.readS32(33, &m_workspaceIndex, 0);
        d.readBlob(34, &m_geometryBytes);
        d.readBool(35, &m_hidden, false);
        d.readBool(36, &m_displayPhotos, true);
        d.readString(37, &m_logFilename, "acars_log.csv");
        d.readBool(38, &m_logEnabled, false);
        d.readS32(39, &m_mode, (int)ACARS);
        if ((m_mode < 0) || (m_mode >= (int)ModeCount)) {
            m_mode = (int)ACARS;
        }
        d.readS32(53, &m_aeroChannel, AcarsAeroReceiver::Submode600P);
        if ((m_aeroChannel < 0) || (m_aeroChannel >= AcarsAeroReceiver::SubmodeCount)) {
            m_aeroChannel = AcarsAeroReceiver::Submode600P;
        }

        d.readS32(56, &m_iridiumBandwidth, AcarsDemodSettings::IridiumScanWidths - 1);
        if ((m_iridiumBandwidth < 0) || (m_iridiumBandwidth >= AcarsDemodSettings::IridiumScanWidths)) {
            m_iridiumBandwidth = AcarsDemodSettings::IridiumScanWidths - 1;
        }
        bool displayNoInfo = false;
        d.readBool(40, &displayNoInfo, false);
        m_hideNoInfo = !displayNoInfo;
        d.readBool(54, &m_showDate, true);
        d.readBlob(55, &m_splitterState);
        d.readBool(41, &m_displayChart, false);

        // Both of these are fed to the table header - moveSection() and setColumnWidth() -
        // so a preset from a build with a different column count, or a corrupt one, would
        // otherwise reorder to an index that does not exist. A visual index must be a
        // column; a size of 0 means hidden and -1 means "leave it to the header", so only
        // negatives below -1 and absurd widths are rejected.
        for (int i = 0; i < ACARSDEMOD_COLUMNS; i++)
        {
            d.readS32(100 + i, &m_columnIndexes[i], i);
            if ((m_columnIndexes[i] < 0) || (m_columnIndexes[i] >= ACARSDEMOD_COLUMNS)) {
                m_columnIndexes[i] = i;
            }
        }
        for (int i = 0; i < ACARSDEMOD_COLUMNS; i++)
        {
            d.readS32(200 + i, &m_columnSizes[i], -1);
            if ((m_columnSizes[i] < -1) || (m_columnSizes[i] > 8192)) {
                m_columnSizes[i] = -1;
            }
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
