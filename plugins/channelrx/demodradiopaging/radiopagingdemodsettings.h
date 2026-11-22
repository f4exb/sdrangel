///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef INCLUDE_RADIOPAGINGDEMODSETTINGS_H
#define INCLUDE_RADIOPAGINGDEMODSETTINGS_H

#include <QByteArray>
#include <QList>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include "dsp/dsptypes.h"
#include "radiopagingcharset.h"

class Serializable;

// Settings of the radio paging demodulator: POCSAG, FLEX and GSC are decoded at the same
// time, each protocol can be switched off. A protocol that is on is decoded at every
// speed, mode and polarity; every message received is output with its protocol, speed
// (or mode) and polarity, and selecting networks is done downstream
struct RadioPagingDemodSettings
{
    static const int m_nbPocsagSpeeds = 3;
    static const int m_pocsagSpeeds[m_nbPocsagSpeeds];     //!< 512, 1200, 2400

    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;                         //!< Channel filter, Hz. The legs filter further
    bool m_pocsagEnabled;                       //!< Decode POCSAG
    Real m_pocsagDeviation;                     //!< Hz
    bool m_flexEnabled;                         //!< Decode FLEX
    Real m_flexDeviation;                       //!< Outer symbol, Hz
    bool m_gscEnabled;                          //!< Decode GSC
    Real m_gscDeviation;                        //!< Hz
    int m_gscVoiceTimeout;                      //!< Longest voice message, seconds
    bool m_gscAudio;                            //!< Play the voice of GSC voice pages
    bool m_gscVoiceRecord;                      //!< Record the voice of GSC voice pages to a WAV file
    QString m_gscVoiceDirectory;                //!< Directory of the voice files, empty for the default (see voiceDirectory)
    int m_gscVoiceKeepDays;                     //!< Voice files older than this many days may be deleted, 0 to keep them all
    int m_gscVoiceKeepMin;                      //!< The newest voice files this many are not deleted
    int m_flexMaxMessageNumber;                 //!< FLEX: largest message number of the network, 0-63: skipped numbers are reported
    Real m_volume;                              //!< Voice audio gain
    QString m_audioDeviceName;                  //!< Voice audio output
    RadioPagingCharset::Key m_pocsagCharset;    //!< Codepage of the alphanumeric messages, in the protocol
    RadioPagingCharset::Key m_flexCharset;
    RadioPagingCharset::Key m_gscCharset;

    //!< POCSAG: what the fill at the end of an alphanumeric message is (1 or 2 characters of the same code)
    enum PocsagFill {
        FillMinimal,                            //!< NUL
        FillConservative,                       //!< NUL, ETX, EOT
        FillCodepage,                           //!< The codes without a glyph in the codepage
        FillCustom                              //!< The codes of m_pocsagFillCodes
    };
    int m_pocsagFill;                           //!< PocsagFill
    // DC offset correction of each protocol: the leg is moved onto the carrier offset measured as the DC offset of the
    // discriminator on the known patterns, and narrowed. Off: the leg stays on the channel centre
    bool m_pocsagDcCorrection;
    bool m_flexDcCorrection;
    bool m_gscDcCorrection;
    QString m_pocsagFillCodes;                  //!< FillCustom: hex codes separated by spaces or commas, e.g. "00 03 04"
    //!< The custom codepage of each protocol (RadioPagingCustomCodepage): the default table with codes mapped to other
    //!< characters and the direction, "rtl 60=05D0 61=05D1" (hex code = hex Unicode code point; rtl: right-to-left)
    QString m_pocsagCustomCodes;
    QString m_flexCustomCodes;
    QString m_gscCustomCodes;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;                          //!< MIMO channel. Not relevant when connected to SI (single Rx)
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    QByteArray m_messageColumns;            //!< The columns of the table of messages: order, sizes, hidden

    // Output of each message received, besides the Message Center: UDP datagrams and a CSV log file
    enum UdpFormat {
        UdpPagerDemod,                      //!< The fields of the Pager demodulator, each ended by a NUL: date, time, address,
                                            //!< function bits, alphanumeric, numeric, protocol, type
        UdpJson                             //!< The message record as one line of JSON (MessageRecord::toJsonLine())
    };
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    int m_udpFormat;                        //!< UdpFormat
    bool m_logEnabled;
    QString m_logFilename;                  //!< CSV of the message records (MessageRecord::toCSV()), header first

    //! A rule run on each message received: a regular expression matched on a field of the message; when it
    //! matches the message is highlighted, spoken, given to a command and plotted on the Map
    struct NotificationSettings
    {
        enum MatchField {
            MatchAddress,
            MatchMessage,
            MatchProtocol,                  //!< pocsag, flex, gsc
            MatchType,                      //!< alphanumeric, numeric, tone, voice...
            MatchChannel,                   //!< The channel alias
            MatchAny                        //!< Address, message, protocol, type and channel, separated by spaces
        };
        int m_matchField;
        QString m_regExp;
        QString m_speech;                   //!< Text to speak, with the substitutions of substitute()
        QString m_command;                  //!< Command to run, with the substitutions of substitute()
        bool m_highlight;
        qint32 m_highlightColor;
        bool m_plotOnMap;                   //!< A position in the message (latitude and longitude) is plotted on the Map

        QRegularExpression m_regularExpression;

        NotificationSettings();
        void updateRegularExpression();
        QByteArray serialize() const;
        bool deserialize(const QByteArray& data);
        static QString matchFieldName(int field);
    };
    QList<QSharedPointer<NotificationSettings>> m_notificationSettings;

    // Scope of the receiver signals (see RadioPagingDemodSink::ScopeSignal)
    int m_scopeCh1;
    int m_scopeCh2;
    Serializable *m_scopeGUI;

    static const int m_channelSampleRate = 38400;   //!< Multiple of 512, 1200, 2400, 1600, 3200 and 600 baud
    static double m_legOffsetAllowance;             //!< Offset room each side of a leg, Hz

    RadioPagingDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;

    //!< Carson bandwidths 2 * (deviation + highest modulation rate): the legs while receiving, Hz
    double pocsagCarson() const;
    double flexCarson() const;
    double gscCarson() const;
    //!< Leg bandwidths while hunting: Carson and the offset room each side, Hz
    double pocsagBandwidth() const;
    double flexBandwidth() const;
    double gscBandwidth() const;

    //!< The directory of the voice files: the one set, or radiopaging/voice in the application data
    QString voiceDirectory() const;
    //!< The name of the demodulator in the message records, the announcements and the voice file names: its title
    QString channelName() const;
    //!< The default codepage of a protocol (pocsag, flex, gsc): generic ASCII
    static RadioPagingCharset::Key defaultCharset(const char *protocol);
    //!< The POCSAG fill codes of the setting, the codepage the one of the messages
    std::vector<uint8_t> pocsagFillCodes(const RadioPagingCharset::Codepage& codepage) const;
    //!< The codes of a list of hex codes (00 to 7F, separated by spaces or commas); false if one is not
    static bool parseFillCodes(const QString& text, std::vector<uint8_t>& codes);
};

QDataStream& operator<<(QDataStream& out, const QSharedPointer<RadioPagingDemodSettings::NotificationSettings>& settings);
QDataStream& operator>>(QDataStream& in, QSharedPointer<RadioPagingDemodSettings::NotificationSettings>& settings);

#endif // INCLUDE_RADIOPAGINGDEMODSETTINGS_H
