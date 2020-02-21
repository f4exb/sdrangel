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

#include "lorademodsettings.h"

#include <QColor>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "lorademodsettings.h"

const int LoRaDemodSettings::bandwidths[] = {2500, 7813, 10417, 15625, 20833, 31250, 41667, 62500, 125000, 250000, 500000};
const int LoRaDemodSettings::nbBandwidths = 11;
const int LoRaDemodSettings::oversampling = 2;

LoRaDemodSettings::LoRaDemodSettings() :
    m_inputFrequencyOffset(0),
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void LoRaDemodSettings::resetToDefaults()
{
    m_bandwidthIndex = 5;
    m_spreadFactor = 7;
    m_deBits = 0;
    m_codingScheme = CodingLoRa;
    m_decodeActive = true;
    m_eomSquelchTenths = 60;
    m_nbSymbolsMax = 255;
    m_preambleChirps = 8;
    m_packetLength = 32;
    m_nbParityBits = 1;
    m_hasCRC = true;
    m_hasHeader = true;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "LoRa Demodulator";
}

QByteArray LoRaDemodSettings::serialize() const
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
    s.writeS32(11, m_nbSymbolsMax);
    s.writeS32(12, m_packetLength);
    s.writeS32(13, m_nbParityBits);
    s.writeBool(14, m_hasCRC);
    s.writeBool(15, m_hasHeader);
    s.writeS32(17, m_preambleChirps);

    return s.final();
}

bool LoRaDemodSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_bandwidthIndex, 0);
        d.readS32(3, &m_spreadFactor, 0);

        if (m_spectrumGUI) {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        if (m_channelMarker) {
            d.readBlob(5, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(6, &m_title, "LoRa Demodulator");
        d.readS32(7, &m_deBits, 0);
        d.readS32(8, &tmp);
        m_codingScheme = (CodingScheme) tmp;
        d.readBool(9, &m_decodeActive, true);
        d.readS32(10, &m_eomSquelchTenths, 60);
        d.readS32(11, &m_nbSymbolsMax, 255);
        d.readS32(12, &m_packetLength, 32);
        d.readS32(13, &m_nbParityBits, 1);
        d.readBool(14, &m_hasCRC, true);
        d.readBool(15, &m_hasHeader, true);
        d.readS32(17, &m_preambleChirps, 8);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
