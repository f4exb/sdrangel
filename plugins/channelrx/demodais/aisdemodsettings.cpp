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
#include "aisdemodsettings.h"

AISDemodSettings::AISDemodSettings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AISDemodSettings::resetToDefaults()
{
    m_baud = 9600; // Fixed
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 16000.0f;
    m_fmDeviation = 4800.0f;
    m_correlationThreshold = 30;
    m_filterMMSI = "";
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_udpFormat = Binary;
    m_logFilename = "ais_log.csv";
    m_logEnabled = false;
    m_showSlotMap = false;
    m_rgbColor = QColor(102, 0, 0).rgb();
    m_title = "AIS Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < AISDEMOD_MESSAGE_COLUMNS; i++)
    {
        m_messageColumnIndexes[i] = i;
        m_messageColumnSizes[i] = -1; // Autosize
    }
}

QByteArray AISDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeFloat(3, m_fmDeviation);
    s.writeFloat(4, m_correlationThreshold);
    s.writeString(5, m_filterMMSI);
    s.writeBool(6, m_udpEnabled);
    s.writeString(7, m_udpAddress);
    s.writeU32(8, m_udpPort);
    s.writeS32(9, (int)m_udpFormat);
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
    s.writeBlob(21, m_scopeGUI->serialize());
    s.writeString(22, m_logFilename);
    s.writeBool(23, m_logEnabled);
    s.writeS32(24, m_baud);

    if (m_rollupState) {
        s.writeBlob(25, m_rollupState->serialize());
    }

    s.writeS32(26, m_workspaceIndex);
    s.writeBlob(27, m_geometryBytes);
    s.writeBool(28, m_hidden);
    s.writeBool(29, m_showSlotMap);

    for (int i = 0; i < AISDEMOD_MESSAGE_COLUMNS; i++)
        s.writeS32(100 + i, m_messageColumnIndexes[i]);
    for (int i = 0; i < AISDEMOD_MESSAGE_COLUMNS; i++)
        s.writeS32(200 + i, m_messageColumnSizes[i]);

    return s.final();
}

bool AISDemodSettings::deserialize(const QByteArray& data)
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
        d.readFloat(2, &m_rfBandwidth, 16000.0f);
        d.readFloat(3, &m_fmDeviation, 4800.0f);
        d.readFloat(4, &m_correlationThreshold, 30);
        d.readString(5, &m_filterMMSI, "");
        d.readBool(6, &m_udpEnabled);
        d.readString(7, &m_udpAddress);
        d.readU32(8, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        d.readS32(9, (int *)&m_udpFormat, (int)Binary);
        d.readU32(12, &m_rgbColor, QColor(102, 0, 0).rgb());
        d.readString(13, &m_title, "AIS Demodulator");

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

        if (m_scopeGUI)
        {
            d.readBlob(21, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readString(22, &m_logFilename, "ais_log.csv");
        d.readBool(23, &m_logEnabled, false);
        d.readS32(24, &m_baud, 9600);

        if (m_rollupState)
        {
            d.readBlob(25, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(26, &m_workspaceIndex, 0);
        d.readBlob(27, &m_geometryBytes);
        d.readBool(28, &m_hidden, false);
        d.readBool(29, &m_showSlotMap, false);

        for (int i = 0; i < AISDEMOD_MESSAGE_COLUMNS; i++) {
            d.readS32(100 + i, &m_messageColumnIndexes[i], i);
        }

        for (int i = 0; i < AISDEMOD_MESSAGE_COLUMNS; i++) {
            d.readS32(200 + i, &m_messageColumnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}


