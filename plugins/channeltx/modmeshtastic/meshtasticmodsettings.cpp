///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "meshtasticmodsettings.h"

const int MeshtasticModSettings::bandwidths[] = {
    325,    // 384k / 1024
    488,    // 500k / 1024
    750,    // 384k / 512
    1500,   // 384k / 256
    2604,   // 333k / 128
    3125,   // 400k / 128
    3906,   // 500k / 128
    5208,   // 333k / 64
    6250,   // 400k / 64
    7813,   // 500k / 64
    10417,  // 333k / 32
    12500,  // 400k / 32
    15625,  // 500k / 32
    20833,  // 333k / 16
    25000,  // 400k / 16
    31250,  // 500k / 16
    41667,  // 333k / 8
    50000,  // 400k / 8
    62500,  // 500k / 8
    83333,  // 333k / 4
    100000, // 400k / 4
    125000, // 500k / 4
    166667, // 333k / 2
    200000, // 400k / 2
    250000, // 500k / 2
    333333, // 333k / 1
    400000, // 400k / 1
    500000  // 500k / 1
};

const MeshtasticModSettings::CodingScheme MeshtasticModSettings::m_codingScheme = MeshtasticModSettings::CodingLoRa;
const MeshtasticModSettings::MessageType MeshtasticModSettings::m_messageType = MeshtasticModSettings::MessageText;
const bool MeshtasticModSettings::m_hasCRC = true;
const bool MeshtasticModSettings::m_hasHeader = true;
const int MeshtasticModSettings::nbBandwidths = 3*8 + 4;
const int MeshtasticModSettings::oversampling = 4;

MeshtasticModSettings::MeshtasticModSettings() :
    m_inputFrequencyOffset(0),
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void MeshtasticModSettings::resetToDefaults()
{
    m_bandwidthIndex = 5;
    m_spreadFactor = 7;
    m_deBits = 0;
    m_preambleChirps = 17;
    m_quietMillis = 1000;
    m_nbParityBits = 1;
    m_textMessage = "Hello LoRa";
    m_syncWord = 0x34;
    m_channelMute = false;
    m_messageRepeat = 1;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9998;
    m_invertRamps = false;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "Meshtastic Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

unsigned int MeshtasticModSettings::getNbSFDFourths() const
{
    switch (m_codingScheme)
    {
    case CodingLoRa:
        return 9;
    default:
        return 8;
    }
}

bool MeshtasticModSettings::hasSyncWord() const
{
    return m_codingScheme == CodingLoRa;
}

QByteArray MeshtasticModSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_bandwidthIndex);
    s.writeS32(3, m_spreadFactor);
    s.writeS32(4, m_codingScheme);

    if (m_channelMarker) {
        s.writeBlob(5, m_channelMarker->serialize());
    }

    s.writeString(6, m_title);
    s.writeS32(7, m_deBits);
    s.writeBool(8, m_channelMute);
    s.writeU32(9, m_syncWord);
    s.writeU32(10, m_preambleChirps);
    s.writeS32(11, m_quietMillis);
    s.writeBool(12, m_invertRamps);
    s.writeString(28, m_textMessage);
    s.writeBlob(29, m_bytesMessage);
    s.writeS32(30, (int) m_messageType);
    s.writeS32(31, m_nbParityBits);
    s.writeBool(32, m_hasCRC);
    s.writeBool(33, m_hasHeader);
    s.writeS32(44, m_messageRepeat);
    s.writeBool(50, m_useReverseAPI);
    s.writeString(51, m_reverseAPIAddress);
    s.writeU32(52, m_reverseAPIPort);
    s.writeU32(53, m_reverseAPIDeviceIndex);
    s.writeU32(54, m_reverseAPIChannelIndex);
    s.writeS32(55, m_streamIndex);
    s.writeBool(56, m_udpEnabled);
    s.writeString(57, m_udpAddress);
    s.writeU32(58, m_udpPort);

    if (m_rollupState) {
        s.writeBlob(59, m_rollupState->serialize());
    }

    s.writeS32(60, m_workspaceIndex);
    s.writeBlob(61, m_geometryBytes);
    s.writeBool(62, m_hidden);

    return s.final();
}

bool MeshtasticModSettings::deserialize(const QByteArray& data)
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
        unsigned int utmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_bandwidthIndex, 0);
        d.readS32(3, &m_spreadFactor, 0);

        if (m_channelMarker)
        {
            d.readBlob(5, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(6, &m_title, "LoRa Demodulator");
        d.readS32(7, &m_deBits, 0);
        d.readBool(8, &m_channelMute, false);
        d.readU32(9, &utmp, 0x34);
        m_syncWord = utmp > 255 ? 0 : utmp;
        d.readU32(10, &m_preambleChirps, 17);
        d.readS32(11, &m_quietMillis, 1000);
        d.readBool(11, &m_useReverseAPI, false);
        d.readBool(12, &m_invertRamps, false);
        d.readString(28, &m_textMessage, "Hello LoRa");
        d.readBlob(29, &m_bytesMessage);
        d.readS32(31, &m_nbParityBits, 1);
        d.readS32(44, &m_messageRepeat, 1);
        d.readBool(50, &m_useReverseAPI, false);
        d.readString(51, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(52, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(53, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(54, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(55, &m_streamIndex, 0);

        d.readBool(56, &m_udpEnabled);
        d.readString(57, &m_udpAddress, "127.0.0.1");
        d.readU32(58, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9998;
        }

        if (m_rollupState)
        {
            d.readBlob(59, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(60, &m_workspaceIndex, 0);
        d.readBlob(61, &m_geometryBytes);
        d.readBool(62, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void MeshtasticModSettings::applySettings(const QStringList& settingsKeys, const MeshtasticModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset"))
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    if (settingsKeys.contains("bandwidthIndex"))
        m_bandwidthIndex = settings.m_bandwidthIndex;
    if (settingsKeys.contains("spreadFactor"))
        m_spreadFactor = settings.m_spreadFactor;
    if (settingsKeys.contains("deBits"))
        m_deBits = settings.m_deBits;
    if (settingsKeys.contains("preambleChirps"))
        m_preambleChirps = settings.m_preambleChirps;
    if (settingsKeys.contains("quietMillis"))
        m_quietMillis = settings.m_quietMillis;
    if (settingsKeys.contains("invertRamps"))
        m_invertRamps = settings.m_invertRamps;
    if (settingsKeys.contains("syncWord"))
        m_syncWord = settings.m_syncWord;
    if (settingsKeys.contains("channelMute"))
        m_channelMute = settings.m_channelMute;
    if (settingsKeys.contains("title"))
        m_title = settings.m_title;
    if (settingsKeys.contains("udpEnabled"))
        m_udpEnabled = settings.m_udpEnabled;
    if (settingsKeys.contains("udpAddress"))
        m_udpAddress = settings.m_udpAddress;
    if (settingsKeys.contains("udpPort"))
        m_udpPort = settings.m_udpPort;
    if (settingsKeys.contains("streamIndex"))
        m_streamIndex = settings.m_streamIndex;
    if (settingsKeys.contains("useReverseAPI"))
        m_useReverseAPI = settings.m_useReverseAPI;
    if (settingsKeys.contains("reverseAPIAddress"))
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    if (settingsKeys.contains("reverseAPIPort"))
        m_reverseAPIPort = settings.m_reverseAPIPort;
    if (settingsKeys.contains("reverseAPIDeviceIndex"))
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    if (settingsKeys.contains("reverseAPIChannelIndex"))
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    if (settingsKeys.contains("workspaceIndex"))
        m_workspaceIndex = settings.m_workspaceIndex;
    if (settingsKeys.contains("geometryBytes"))
        m_geometryBytes = settings.m_geometryBytes;
    if (settingsKeys.contains("hidden"))
        m_hidden = settings.m_hidden;
    if (settingsKeys.contains("channelMarker") && m_channelMarker && settings.m_channelMarker)
        m_channelMarker->deserialize(settings.m_channelMarker->serialize());
    if (settingsKeys.contains("rollupState") && m_rollupState && settings.m_rollupState)
        m_rollupState->deserialize(settings.m_rollupState->serialize());
    if (settingsKeys.contains("textMessage"))
        m_textMessage = settings.m_textMessage;
    if (settingsKeys.contains("bytesMessage"))
        m_bytesMessage = settings.m_bytesMessage;
    if (settingsKeys.contains("nbParityBits"))
        m_nbParityBits = settings.m_nbParityBits;
    if (settingsKeys.contains("messageRepeat"))
        m_messageRepeat = settings.m_messageRepeat;
}

QString MeshtasticModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    QString debug;
    if (settingsKeys.contains("inputFrequencyOffset") || force)
        debug += QString("Input Frequency Offset: %1\n").arg(m_inputFrequencyOffset);
    if (settingsKeys.contains("bandwidthIndex") || force)
        debug += QString("Bandwidth Index: %1\n").arg(m_bandwidthIndex);
    if (settingsKeys.contains("spreadFactor") || force)
        debug += QString("Spread Factor: %1\n").arg(m_spreadFactor);
    if (settingsKeys.contains("deBits") || force)
        debug += QString("DE Bits: %1\n").arg(m_deBits);
    if (settingsKeys.contains("codingScheme") || force)
        debug += QString("Coding Scheme: %1\n").arg(m_codingScheme);
    if (settingsKeys.contains("preambleChirps") || force)
        debug += QString("Preamble Chirps: %1\n").arg(m_preambleChirps);
    if (settingsKeys.contains("quietMillis") || force)
        debug += QString("Quiet Millis: %1\n").arg(m_quietMillis);
    if (settingsKeys.contains("invertRamps") || force)
        debug += QString("Invert Ramps: %1\n").arg(m_invertRamps);
    if (settingsKeys.contains("syncWord") || force)
        debug += QString("Sync Word: %1\n").arg(m_syncWord);
    if (settingsKeys.contains("channelMute") || force)
        debug += QString("Channel Mute: %1\n").arg(m_channelMute);
    if (settingsKeys.contains("title") || force)
        debug += QString("Title: %1\n").arg(m_title);
    if (settingsKeys.contains("udpEnabled") || force)
        debug += QString("UDP Enabled: %1\n").arg(m_udpEnabled);
    if (settingsKeys.contains("udpAddress") || force)
        debug += QString("UDP Address: %1\n").arg(m_udpAddress);
    if (settingsKeys.contains("udpPort") || force)
        debug += QString("UDP Port: %1\n").arg(m_udpPort);
    if (settingsKeys.contains("streamIndex") || force)
        debug += QString("Stream Index: %1\n").arg(m_streamIndex);
    if (settingsKeys.contains("useReverseAPI") || force)
        debug += QString("Use Reverse API: %1\n").arg(m_useReverseAPI);
    if (settingsKeys.contains("reverseAPIAddress") || force)
        debug += QString("Reverse API Address: %1\n").arg(m_reverseAPIAddress);
    if (settingsKeys.contains("reverseAPIPort") || force)
        debug += QString("Reverse API Port: %1\n").arg(m_reverseAPIPort);
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force)
        debug += QString("Reverse API Device Index: %1\n").arg(m_reverseAPIDeviceIndex);
    if (settingsKeys.contains("reverseAPIChannelIndex") || force)
        debug += QString("Reverse API Channel Index: %1\n").arg(m_reverseAPIChannelIndex);
    if (settingsKeys.contains("workspaceIndex") || force)
        debug += QString("Workspace Index: %1\n").arg(m_workspaceIndex);
    if (settingsKeys.contains("hidden") || force)
        debug += QString("Hidden: %1\n").arg(m_hidden);
    if (settingsKeys.contains("textMessage") || force)
        debug += QString("Text Message: %1\n").arg(m_textMessage);
    if (settingsKeys.contains("messageType") || force)
        debug += QString("Message Type: %1\n").arg(m_messageType);
    if (settingsKeys.contains("nbParityBits") || force)
        debug += QString("Number of Parity Bits: %1\n").arg(m_nbParityBits);
    if (settingsKeys.contains("hasCRC") || force)
        debug += QString("Has CRC: %1\n").arg(m_hasCRC);
    if (settingsKeys.contains("hasHeader") || force)
        debug += QString("Has Header: %1\n").arg(m_hasHeader);
    if (settingsKeys.contains("messageRepeat") || force)
        debug += QString("Message Repeat: %1\n").arg(m_messageRepeat);
    return debug;
}
