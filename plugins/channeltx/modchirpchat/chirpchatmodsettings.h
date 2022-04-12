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

#ifndef PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODSETTINGS_H_

#include <QByteArray>
#include <QString>

#include <stdint.h>

class Serializable;

struct ChirpChatModSettings
{
    enum CodingScheme
    {
        CodingLoRa,  //!< Standard LoRa
        CodingASCII, //!< plain ASCII (7 bits)
        CodingTTY    //!< plain TTY (5 bits)
    };

    enum MessageType
    {
        MessageNone,
        MessageBeacon,
        MessageCQ,
        MessageReply,
        MessageReport,
        MessageReplyReport,
        MessageRRR,
        Message73,
        MessageQSOText,
        MessageText,
        MessageBytes
    };

    int m_inputFrequencyOffset;
    int m_bandwidthIndex;
    int m_spreadFactor;
    int m_deBits;                  //!< Low data rate optmize (DE) bits
    unsigned int m_preambleChirps; //!< Number of preamble chirps
    int m_quietMillis;             //!< Number of milliseconds to pause between transmissions
    int m_nbParityBits;            //!< Hamming parity bits (LoRa)
    bool m_hasCRC;                 //!< Payload has CRC (LoRa)
    bool m_hasHeader;              //!< Header present before actual payload (LoRa)
    unsigned char m_syncWord;
    bool m_channelMute;
    CodingScheme m_codingScheme;
    QString m_myCall;     //!< QSO mode: my callsign
    QString m_urCall;     //!< QSO mode: your callsign
    QString m_myLoc;      //!< QSO mode: my locator
    QString m_myRpt;      //!< QSO mode: my report
    MessageType m_messageType;
    QString m_beaconMessage;
    QString m_cqMessage;
    QString m_replyMessage;
    QString m_reportMessage;
    QString m_replyReportMessage;
    QString m_rrrMessage;
    QString m_73Message;
    QString m_qsoTextMessage;
    QString m_textMessage;
    QByteArray m_bytesMessage;
    int m_messageRepeat;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
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

    ChirpChatModSettings();
    void resetToDefaults();
    void setDefaultTemplates();
    void generateMessages();
    unsigned int getNbSFDFourths() const; //!< Get the number of SFD period fourths (depends on coding scheme)
    bool hasSyncWord() const;             //!< Only LoRa has a syncword (for the moment)
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODSETTINGS_H_ */
