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

#ifndef INCLUDE_RADIOPAGINGMODSETTINGS_H
#define INCLUDE_RADIOPAGINGMODSETTINGS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "dsp/dsptypes.h"
#include "radiopagingcharset.h"

class Serializable;

// Settings of the radio paging modulator. A channel transmits one protocol; the protocol
// settings are the defaults of the messages it sends
struct RadioPagingModSettings
{
    enum Protocol {
        Pocsag,
        Flex,
        Gsc
    };

    enum Mode {
        OneShot,                                //!< Carrier on for each transmission, off after it
        Network                                 //!< Carrier on all the time, idle fill between messages
    };

    static const int m_nbPocsagSpeeds = 3;
    static const int m_pocsagSpeeds[m_nbPocsagSpeeds];     //!< 512, 1200, 2400

    qint64 m_inputFrequencyOffset;
    Protocol m_protocol;
    Mode m_mode;
    Real m_deviation;                           //!< Of the outer symbol, Hz
    Real m_gain;                                //!< dB
    bool m_channelMute;
    Real m_rampUpMs;                            //!< Power ramp at the start of a transmission
    Real m_rampDownMs;                          //!< Power ramp at the end of a transmission
    int m_rampRange;                            //!< dB
    int m_pocsagSpeed;                          //!< Baud
    bool m_pocsagInverted;                      //!< Set: binary 1 is a positive deviation (normal: negative)
    int m_flexMode;                             //!< RadioPagingFlex::Mode: 1600/2, 3200/2, 3200/4, 6400/4
    bool m_flexInverted;                        //!< Set: symbol levels reversed (normal: symbol 3 is the highest frequency)
    int m_flexCollapse;                         //!< 0-7: re-synchronization of 2^collapse frames at least; network: frames scheduled with it
    bool m_flexRoaming;                         //!< Send the system ID words of the roaming frames (STD-43A 3.7.2, 6.1.1)
    int m_ssidLocalId;                          //!< 0-511
    int m_ssidCoverageZone;                     //!< 0-31
    int m_ssidCountryCode;                      //!< 0-1023
    int m_ssidTrafficFlags;                     //!< 0-15: traffic management flags announced in the second system ID word
    // FLEX date and time in block information words 001, 010, 101 (STD-43A 3.7.2), see RadioPagingFlexEncoder::TimeService
    bool m_flexTime;                            //!< Send the date and time
    bool m_flexTimeFrameZero;                   //!< Time of frame 0 of the cycle (the standard), else of the frame sent
    int m_flexTimeZoneMinutes;                  //!< Offset of the standard time of the zone from UTC, a zone of Table 3.7.2-3
    bool m_flexTimeSendZone;                    //!< Send the time zone word
    bool m_flexTimeDaylightSaving;              //!< The time is daylight saving time (one hour ahead)
    qint64 m_flexTimeOffsetSeconds;             //!< Added to the time of the frame grid (UTC): any date and time can be sent
    bool m_flexTimeEquivalentYear;              //!< A year after 2025 as the equivalent year of 1994-2025 (same calendar)
    int m_flexTimeExtendedSeconds;              //!< S5-S3 of the time zone word, 0-7 steps of 0.9375 s: time display correction
    int m_maxQueued;                            //!< Messages not sent yet before requests are refused
    static const int m_maxQueuedLimit = 100000; //!< Largest m_maxQueued
    bool m_flexNumbering;                       //!< The network numbers the messages of each address (STD-43A 8.4); off: N = 0, R = 0
    int m_flexMaxMessageNumber;                 //!< Largest message number of the network, 0-63: then 0 again
    int m_gscMode;                              //!< RadioPagingGsc::Mode: auto, single, batch, non battery saver. One-shot only
    bool m_gscInverted;                         //!< Set: binary 1 is a negative deviation (normal: positive)
    int m_gscVoiceSeconds;                      //!< Length of the voice of a GSC voice page in seconds, a longer sound file is cut
    QString m_audioInputDeviceName;             //!< Audio input of the voice recorded from the microphone
    QString m_gscVoiceDirectory;                //!< Directory of the files of the voice sent, empty for the default (see voiceDirectory)
    int m_gscVoiceKeepDays;                     //!< Files of the voice sent older than this many days may be deleted, 0 to keep them all
    int m_gscVoiceKeepMin;                      //!< The newest files of the voice sent this many are not deleted
    // FLEX network option: mixing with POCSAG (STD-43A Chapter 7). POCSAG messages are sent in
    // the time between FLEX frames, with the POCSAG settings above (speed, polarity, deviation
    // here)
    bool m_flexMixPocsag;
    Real m_pocsagDeviation;                     //!< Of POCSAG when mixed, Hz (FLEX: the deviation of the channel)
    int m_mixFramesPerMinute;                   //!< 0: the first 2^collapse frames of a minute are FLEX (7.1.1); N: N frames, system collapse 5 (7.1.2)
    bool m_mixShared;                           //!< Shared channel: the minute is 4 minutes (128 frames)
    int m_mixSwitchMs[3];                       //!< 750 bps pattern before POCSAG after FLEX, ms: 512, 1200, 2400 (7.2.1, 7.2.2; 2400 extrapolated)

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    QByteArray m_messageColumns;            //!< The columns of the log of messages: order, sizes, hidden
    QString m_voicePlaybackDeviceName;      //!< Audio output of the voice files played from the log
    RadioPagingCharset::Key m_pocsagCharset; //!< Codepage of the alphanumeric messages of a page without one, in the protocol
    RadioPagingCharset::Key m_flexCharset;
    RadioPagingCharset::Key m_gscCharset;

    RadioPagingModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const RadioPagingModSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
    //!< The directory of the files of the voice sent: the one set, or radiopaging/voice/tx in the application data
    QString voiceDirectory() const;
    //!< The name of the transmitter in the message records, the announcements and the voice file names: its title
    QString channelName() const;
    //!< The codepage set for a protocol (pocsag, flex, gsc)
    const RadioPagingCharset::Key& charset(Protocol protocol) const;
    //!< The default codepage of a protocol: generic ASCII
    static RadioPagingCharset::Key defaultCharset(const char *protocol);
    //!< The standard time offset (minutes) and daylight saving of the system time zone now; UTC if the zone is not
    //!< in STD-43A Table 3.7.2-3
    static void systemTimeZone(int& zoneMinutes, bool& daylightSaving);

    //! Bring the numeric settings into their ranges (ramps, ramp range, deviation, gain)
    void validate();

    static bool isPocsagSpeed(int baud);
    static QString protocolName(Protocol protocol);
    //! Usual deviation of the outer symbol: POCSAG and GSC 4500 Hz, FLEX 4800 Hz (STD-43A 2.1.3)
    static float defaultDeviation(Protocol protocol);
    //! FLEX mode of its name (1600/2, 3200/2, 3200/4, 6400/4): -1 if none
    static int flexModeOf(const QString& name);
};

#endif // INCLUDE_RADIOPAGINGMODSETTINGS_H
