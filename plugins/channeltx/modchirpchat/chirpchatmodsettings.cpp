///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB                              //
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

#include "chirpchatmodsettings.h"

const int ChirpChatModSettings::bandwidths[] = {
    325,    // 384k / 1024
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
const int ChirpChatModSettings::nbBandwidths = 3*8 + 3;
const int ChirpChatModSettings::oversampling = 4;

ChirpChatModSettings::ChirpChatModSettings() :
    m_inputFrequencyOffset(0),
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ChirpChatModSettings::resetToDefaults()
{
    m_bandwidthIndex = 5;
    m_spreadFactor = 7;
    m_deBits = 0;
    m_preambleChirps = 8;
    m_quietMillis = 1000;
    m_codingScheme = CodingLoRa;
    m_nbParityBits = 1;
    m_hasCRC = true;
    m_hasHeader = true;
    m_textMessage = "Hello LoRa";
    m_myCall = "MYCALL";
    m_urCall = "URCALL";
    m_myLoc = "AA00AA";
    m_myRpt = "59";
    m_syncWord = 0x34;
    m_channelMute = false;
    m_messageRepeat = 1;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9998;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "ChirpChat Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    setDefaultTemplates();
}

void ChirpChatModSettings::setDefaultTemplates()
{
    // %1: myCall %2: urCall %3: myLoc %4: report
    m_beaconMessage = "VVV DE %1 %2";   // Beacon
    m_cqMessage = "CQ DE %1 %2";        // caller calls CQ
    m_replyMessage = "%1 %2 %3";        // Reply to CQ from caller
    m_reportMessage = "%1 %2 %3";       // Report to caller
    m_replyReportMessage = "%1 %2 R%3"; // Report to callee
    m_rrrMessage = "%1 %2 RRR";         // RRR to callee
    m_73Message = "%1 %2 73";           // 73 to caller
    m_qsoTextMessage = "%1 %2 %3";      // Freeflow message to caller - %3 is m_textMessage
}

void ChirpChatModSettings::generateMessages()
{
    m_beaconMessage = m_beaconMessage
        .arg(m_myCall).arg(m_myLoc);
    m_cqMessage = m_cqMessage
        .arg(m_myCall).arg(m_myLoc);
    m_replyMessage = m_replyMessage
        .arg(m_urCall).arg(m_myCall).arg(m_myLoc);
    m_reportMessage = m_reportMessage
        .arg(m_urCall).arg(m_myCall).arg(m_myRpt);
    m_replyReportMessage = m_replyReportMessage
        .arg(m_urCall).arg(m_myCall).arg(m_myRpt);
    m_rrrMessage = m_rrrMessage
        .arg(m_urCall).arg(m_myCall);
    m_73Message = m_73Message
        .arg(m_urCall).arg(m_myCall);
    m_qsoTextMessage = m_qsoTextMessage
        .arg(m_urCall).arg(m_myCall).arg(m_textMessage);
}

unsigned int ChirpChatModSettings::getNbSFDFourths() const
{
    switch (m_codingScheme)
    {
    case CodingLoRa:
        return 9;
    default:
        return 8;
    }
}

bool ChirpChatModSettings::hasSyncWord() const
{
    return m_codingScheme == CodingLoRa;
}

QByteArray ChirpChatModSettings::serialize() const
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
    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);
    s.writeU32(16, m_reverseAPIChannelIndex);
    s.writeString(20, m_beaconMessage);
    s.writeString(21, m_cqMessage);
    s.writeString(22, m_replyMessage);
    s.writeString(23, m_reportMessage);
    s.writeString(24, m_replyReportMessage);
    s.writeString(25, m_rrrMessage);
    s.writeString(26, m_73Message);
    s.writeString(27, m_qsoTextMessage);
    s.writeString(28, m_textMessage);
    s.writeBlob(29, m_bytesMessage);
    s.writeS32(30, (int) m_messageType);
    s.writeS32(31, m_nbParityBits);
    s.writeBool(32, m_hasCRC);
    s.writeBool(33, m_hasHeader);
    s.writeString(40, m_myCall);
    s.writeString(41, m_urCall);
    s.writeString(42, m_myLoc);
    s.writeString(43, m_myRpt);
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

bool ChirpChatModSettings::deserialize(const QByteArray& data)
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
        int tmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_bandwidthIndex, 0);
        d.readS32(3, &m_spreadFactor, 0);
        d.readS32(4, &tmp, 0);
        m_codingScheme = (CodingScheme) tmp;

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
        d.readU32(10, &m_preambleChirps, 8);
        d.readS32(11, &m_quietMillis, 1000);
        d.readBool(11, &m_useReverseAPI, false);
        d.readString(12, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(13, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(14, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(15, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(20, &m_beaconMessage, "VVV DE %1 %2");
        d.readString(21, &m_cqMessage, "CQ DE %1 %2");
        d.readString(22, &m_replyMessage, "%2 %1 %3");
        d.readString(23, &m_reportMessage, "%2 %1 %3");
        d.readString(24, &m_replyReportMessage, "%2 %1 R%3");
        d.readString(25, &m_rrrMessage, "%2 %1 RRR");
        d.readString(26, &m_73Message, "%2 %1 73");
        d.readString(27, &m_qsoTextMessage, "%2 %1 Hello LoRa");
        d.readString(28, &m_textMessage, "Hello LoRa");
        d.readBlob(29, &m_bytesMessage);
        d.readS32(30, &tmp, 0);
        m_messageType = (MessageType) tmp;
        d.readS32(31, &m_nbParityBits, 1);
        d.readBool(32, &m_hasCRC, true);
        d.readBool(33, &m_hasHeader, true);
        d.readString(40, &m_myCall, "MYCALL");
        d.readString(41, &m_urCall, "URCALL");
        d.readString(42, &m_myLoc, "AA00AA");
        d.readString(43, &m_myRpt, "59");
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
