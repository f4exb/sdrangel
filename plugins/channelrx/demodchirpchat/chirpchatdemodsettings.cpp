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

#include "chirpchatdemodsettings.h"

const int ChirpChatDemodSettings::bandwidths[] = {
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
const int ChirpChatDemodSettings::nbBandwidths = 3*8 + 3;
const int ChirpChatDemodSettings::oversampling = 2;

ChirpChatDemodSettings::ChirpChatDemodSettings() :
    m_inputFrequencyOffset(0),
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void ChirpChatDemodSettings::resetToDefaults()
{
    m_bandwidthIndex = 5;
    m_spreadFactor = 7;
    m_deBits = 0;
    m_codingScheme = CodingLoRa;
    m_decodeActive = true;
    m_fftWindow = FFTWindow::Rectangle;
    m_eomSquelchTenths = 60;
    m_nbSymbolsMax = 255;
    m_preambleChirps = 8;
    m_packetLength = 32;
    m_nbParityBits = 1;
    m_hasCRC = true;
    m_hasHeader = true;
    m_sendViaUDP = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "ChirpChat Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray ChirpChatDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_bandwidthIndex);
    s.writeS32(3, m_spreadFactor);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(5, m_channelMarker->serialize());
    }

    s.writeString(6, m_title);
    s.writeS32(7, m_deBits);
    s.writeS32(8, m_codingScheme);
    s.writeBool(9, m_decodeActive);
    s.writeS32(10, m_eomSquelchTenths);
    s.writeU32(11, m_nbSymbolsMax);
    s.writeS32(12, m_packetLength);
    s.writeS32(13, m_nbParityBits);
    s.writeBool(14, m_hasCRC);
    s.writeBool(15, m_hasHeader);
    s.writeU32(17, m_preambleChirps);
    s.writeS32(18, (int) m_fftWindow);
    s.writeBool(20, m_useReverseAPI);
    s.writeString(21, m_reverseAPIAddress);
    s.writeU32(22, m_reverseAPIPort);
    s.writeU32(23, m_reverseAPIDeviceIndex);
    s.writeU32(24, m_reverseAPIChannelIndex);
    s.writeS32(25, m_streamIndex);
    s.writeBool(26, m_sendViaUDP);
    s.writeString(27, m_udpAddress);
    s.writeU32(28, m_udpPort);

    if (m_rollupState) {
        s.writeBlob(29, m_rollupState->serialize());
    }

    s.writeS32(30, m_workspaceIndex);
    s.writeBlob(31, m_geometryBytes);
    s.writeBool(32, m_hidden);

    return s.final();
}

bool ChirpChatDemodSettings::deserialize(const QByteArray& data)
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
        int tmp;
        unsigned int utmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_bandwidthIndex, 0);
        d.readS32(3, &m_spreadFactor, 0);

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(5, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(6, &m_title, "ChirpChat Demodulator");
        d.readS32(7, &m_deBits, 0);
        d.readS32(8, &tmp);
        m_codingScheme = (CodingScheme) tmp;
        d.readBool(9, &m_decodeActive, true);
        d.readS32(10, &m_eomSquelchTenths, 60);
        d.readU32(11, &m_nbSymbolsMax, 255);
        d.readS32(12, &m_packetLength, 32);
        d.readS32(13, &m_nbParityBits, 1);
        d.readBool(14, &m_hasCRC, true);
        d.readBool(15, &m_hasHeader, true);
        d.readU32(17, &m_preambleChirps, 8);
        d.readS32(18, &tmp, (int) FFTWindow::Rectangle);
        m_fftWindow = (FFTWindow::Function) tmp;
        d.readBool(20, &m_useReverseAPI, false);
        d.readString(21, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(22, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(23, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(24, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(25, &m_streamIndex, 0);
        d.readBool(26, &m_sendViaUDP, false);
        d.readString(27, &m_udpAddress, "127.0.0.1");
        d.readU32(28, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        if (m_rollupState)
        {
            d.readBlob(29, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(30, &m_workspaceIndex, 0);
        d.readBlob(31, &m_geometryBytes);
        d.readBool(32, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

unsigned int ChirpChatDemodSettings::getNbSFDFourths() const
{
    switch (m_codingScheme)
    {
    case CodingLoRa:
        return 9;
    default:
        return 8;
    }
}

bool ChirpChatDemodSettings::hasSyncWord() const
{
    return m_codingScheme == CodingLoRa;
}
