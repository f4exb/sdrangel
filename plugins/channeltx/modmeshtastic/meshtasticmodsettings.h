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

#ifndef PLUGINS_CHANNELTX_MODMESHTASTIC_MESHTASTICMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODMESHTASTIC_MESHTASTICMODSETTINGS_H_

#include <QByteArray>
#include <QString>

#include <stdint.h>

class Serializable;

struct MeshtasticModSettings
{
    enum CodingScheme
    {
        CodingLoRa,  //!< Standard LoRa
    };

    enum MessageType
    {
        MessageText
    };

    int m_inputFrequencyOffset;
    int m_bandwidthIndex;
    int m_spreadFactor;
    int m_deBits;                  //!< Low data rate optimize (DE) bits
    unsigned int m_preambleChirps; //!< Number of preamble chirps
    int m_quietMillis;             //!< Number of milliseconds to pause between transmissions
    int m_nbParityBits;            //!< Hamming parity bits (LoRa)
    static const bool m_hasCRC;    //!< Payload has CRC (LoRa)
    static const bool m_hasHeader; //!< Header present before actual payload (LoRa)
    unsigned char m_syncWord;
    bool m_channelMute;
    static const CodingScheme m_codingScheme;
    static const MessageType m_messageType;
    QString m_textMessage;
    QByteArray m_bytesMessage;
    int m_messageRepeat;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    bool m_invertRamps;            //!< Invert chirp ramps vs standard LoRa (up/down/up is standard)
    uint32_t m_rgbColor;
    QString m_title;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    Serializable *m_channelMarker;
    Serializable *m_rollupState;

    static const int bandwidths[];
    static const int nbBandwidths;
    static const int oversampling;

    MeshtasticModSettings();
    void resetToDefaults();
    unsigned int getNbSFDFourths() const; //!< Get the number of SFD period fourths (depends on coding scheme)
    bool hasSyncWord() const;             //!< Only LoRa has a syncword (for the moment)
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const MeshtasticModSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};



#endif /* PLUGINS_CHANNELTX_MODMESHTASTIC_MESHTASTICMODSETTINGS_H_ */
