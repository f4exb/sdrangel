///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODLORA_LORAMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODLORA_LORAMODSETTINGS_H_

#include <QByteArray>
#include <QString>

#include <stdint.h>

class Serializable;

struct LoRaModSettings
{
    enum CodingScheme
    {
        CodingTTY,   //!< plain TTY (5 bits)
        CodingASCII, //!< plain ASCII (7 bits)
        CodingLoRa   //!< Standard LoRa
    };

    int m_inputFrequencyOffset;
    int m_bandwidthIndex;
    int m_spreadFactor;
    int m_deBits;         //!< Low data rate optmize (DE) bits
    int m_preambleChirps; //!< Number of preamble chirps
    int m_quietMillis;    //!< Number of milliseconds to pause between transmissions
    unsigned char m_syncWord;
    bool m_channelMute;
    CodingScheme m_codingScheme;
    QString m_message;    //!< Freeflow message
    QString m_myCall;     //!< QSO mode: my callsign
    QString m_urCall;     //!< QSO mode: your callsign
    QString m_myLoc;      //!< QSO mode: my locator
    QString m_myRpt;      //!< QSO mode: my report
    uint32_t m_rgbColor;
    QString m_title;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    Serializable *m_channelMarker;

    static const int bandwidths[];
    static const int nbBandwidths;
    static const int oversampling;

    LoRaModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELTX_MODLORA_LORAMODSETTINGS_H_ */
