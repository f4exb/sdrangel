
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

#ifndef PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_

#include <QByteArray>
#include <QString>

#include <stdint.h>

class Serializable;

struct LoRaDemodSettings
{
    enum CodingScheme
    {
        CodingLoRa,  //!< Standard LoRa
        CodingASCII, //!< plain ASCII (7 bits)
        CodingTTY    //!< plain TTY (5 bits)
    };

    int m_inputFrequencyOffset;
    int m_bandwidthIndex;
    int m_spreadFactor;
    int m_deBits;        //!< Low data rate optmize (DE) bits
    CodingScheme m_codingScheme;
    bool m_decodeActive;
    int m_eomSquelchTenths; //!< Squelch factor to trigger end of message (/10)
    int m_nbSymbolsMax;     //!< Maximum number of symbols in a payload
    int m_preambleChirps;   //!< Number of expected preamble chirps
    int m_nbParityBits;     //!< Hamming parity bits (LoRa)
    int m_packetLength;     //!< Payload packet length in bytes or characters (LoRa)
    bool m_hasCRC;          //!< Payload has CRC (LoRa)
    bool m_hasHeader;       //!< Header present before actual payload (LoRa)
    uint32_t m_rgbColor;
    QString m_title;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;

    static const int bandwidths[];
    static const int nbBandwidths;
    static const int oversampling;

    LoRaDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_ */
