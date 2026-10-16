///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DVBDEMODSETTINGS_H
#define INCLUDE_DVBDEMODSETTINGS_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "dtv/dtvguide.h"

class Serializable;

struct DVBDemodSettings
{
    //! m_standard's values. Also DVBDemodSink::Status::m_standard's, the standard being received (the automatic ones
    //! while none's found yet), which has no StandardDVBT2Lite: T2-Lite's shown as StandardDVBT2
    enum Standard {
        StandardAutoTerrestrial = 0,    //!< DVB-T or T2
        StandardDVBT = 1,
        StandardDVBT2 = 2,              //!< T2-base, or T2-Lite alone
        StandardDVBT2Lite = 3,
        StandardAutoSatellite = 4,      //!< DVB-S or S2
        StandardDVBS = 5,
        StandardDVBS2 = 6
    };

    //! A programme to record, from the programme guide, and a series
    typedef DTVScheduledRecording ScheduledRecording;
    typedef DTVSeriesRecording SeriesRecording;

    qint64 m_inputFrequencyOffset;
    int m_bandwidth;                //!< Channel bandwidth in Hz: 8, 7 or 6 MHz
    //! 0 automatic (DVB-T or T2), 1 DVB-T, 2 DVB-T2 (T2-base, or T2-Lite alone), 3 DVB-T2 T2-Lite, 4 automatic (DVB-S or
    //! S2), 5 DVB-S, 6 DVB-S2
    int m_standard;
    int m_plpId;                    //!< DVB-T2 PLP to decode, -1 for the first data PLP
    bool m_lowPriority;             //!< DVB-T hierarchical modes: decode the low priority stream, rather than the high
    int m_symbolRate;               //!< DVB-S and S2: symbols a second, 0 to measure it (Auto, the default)
    int m_rollOff;                  //!< DVB-S2: 0 automatic (from the BBHEADER), 35, 25, 20, or S2X's 15, 10, 5 (hundredths). DVB-S's is 0.35
    int m_codeRate;                 //!< DVB-S: -1 automatic, 0 to 4 for 1/2, 2/3, 3/4, 5/6, 7/8
    int m_isi;                      //!< DVB-S2: input stream to decode, in a multiple input stream, -1 for the first
                                    //!< (that isn't only null packets)
    int m_plScramblingCode;         //!< DVB-S2: Gold code n of the physical layer scrambling, 0 for broadcasting
    bool m_equaliser;               //!< DVB-S and S2: undo the receive chain's response (the device's and transponder's filters)
    int m_freesatRegion;            //!< Freesat's regional channel numbers: region id, 0 for the default region's
    bool m_udpEnabled;              //!< Send the transport stream over UDP
    QString m_udpAddress;
    uint16_t m_udpPort;
    bool m_videoEnabled;            //!< Decode and display the selected service
    int m_serviceId;                //!< MPEG program number of the service to play, 0 for the first with video
    bool m_deinterlace;
    bool m_hbbtv;                   //!< Run the service's HbbTV applications (red button), over the video
    bool m_hardwareDecoding;        //!< Decode video with the GPU, where possible
    QString m_audioLanguage;        //!< ISO 639-2 code of the audio to play, or empty for the first
    bool m_audioDescription;        //!< Play audio description, where a service has it
    bool m_audioDescriptionMixed;   //!< Audio description: the broadcaster's mix rather than mixed in here, where both
    bool m_cleanAudio;              //!< Play clean audio (for the hard of hearing), where a service has it
    bool m_subtitles;               //!< Show subtitles, where a service has them
    QString m_subtitleLanguage;     //!< ISO 639-2 code of the subtitles to show, or empty for the first
    bool m_subtitlesHardOfHearing;  //!< The hard of hearing subtitles, where there are both
    int m_avOffset;                 //!< Milliseconds the picture is delayed relative to the sound (positive for later)
    bool m_audioMute;
    int m_audioVolume;              //!< 0..100
    QString m_audioDeviceName;
    QString m_recordDirectory;      //!< Where recordings are saved
    int m_recordMode;               //!< 0 the whole multiplex, 1 the service selected
    int m_recordStartPadding;       //!< Seconds scheduled recordings start before the programme
    int m_recordEndPadding;         //!< Seconds they continue after it
    int m_epgReport;                //!< Web API report: 0 the guide's now and next, 1 also every service's, 2 also a schedule
    int m_epgReportServiceId;       //!< Service whose schedule the report gives, 0 for the one the now and next are of
    int m_epgReportNetworkId;       //!< Its original_network_id, -1 for the network received
    QList<ScheduledRecording> m_scheduledRecordings;
    QList<SeriesRecording> m_seriesRecordings;
    //! For a scheduled recording of a service on another multiplex: retune to it, when nothing's being recorded, and
    //! back afterwards (the service it's on played meanwhile)
    bool m_recordRetune;
    //! Start and end scheduled recordings by the programme's running status, where the broadcaster sends it, so
    //! one that starts late or over-runs is recorded whole
    bool m_recordFollowRunning;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    QList<int> m_splitterSizes;     //!< Heights of the signal and video areas, as the user left them (empty for the default)
    //! DVB-S/S2: run to switch the LNB's polarisation before tuning to a multiplex on another, with %p for the one wanted
    //! (H, V, L or R), else it's added as the last argument. Empty to ask the user. Not in the Web API, as it's run here
    QString m_polarisationCommand;
    qint64 m_scanFrom;              //!< The last scan's range (downlink Hz, as the device's), the next's default. 0 for none
    qint64 m_scanTo;

    DVBDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const DVBDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    static QByteArray serializeSchedule(const QList<ScheduledRecording>& schedule);
    static QByteArray serializeSeries(const QList<SeriesRecording>& series);
    static QList<SeriesRecording> deserializeSeries(const QByteArray& data);
    //! DVB-T's 5 to 8 MHz (5 MHz in EN 300 744 annex G), and DVB-T2's 1.7 MHz (as used for T2-Lite) and 10 MHz (for
    //! professional use) too
    static bool isValidBandwidth(int bandwidth) {
        return (bandwidth == 10000000) || (bandwidth == 8000000) || (bandwidth == 7000000) || (bandwidth == 6000000)
            || (bandwidth == 5000000) || (bandwidth == 1700000);
    }
    static bool hasDVBT(int bandwidth) { return (bandwidth != 1700000) && (bandwidth != 10000000); }
    //! The terrestrial receivers' sample rate for a bandwidth: 8/7 times it, but 131/71 MS/s for 1.7 MHz (EN 302 755
    //! table 65)
    static double terrestrialSampleRate(int bandwidth);
    static bool isValidStandard(int standard) { return (standard >= StandardAutoTerrestrial) && (standard <= StandardDVBS2); }
    static bool isSatellite(int standard) { return standard >= StandardAutoSatellite; }
    //! 0 for automatic (measured from the signal)
    static bool isValidSymbolRate(int symbolRate) { return (symbolRate == 0) || ((symbolRate >= 10000) && (symbolRate <= 50000000)); }
    static bool isValidRollOff(int rollOff) {
        return (rollOff == 0) || (rollOff == 35) || (rollOff == 25) || (rollOff == 20) || (rollOff == 15) || (rollOff == 10) || (rollOff == 5);
    }
    //! A DVB-S2 BBHEADER's roll-off as the receiver codes it (0 0.35, 1 0.25, 2 0.20, S2X's 4 0.15, 5 0.10, 6 0.05) in
    //! hundredths, 0 if not known
    static int rollOffHundredths(int code) {
        static const int values[7] = {35, 25, 20, 0, 15, 10, 5};
        return (code >= 0) && (code < 7) ? values[code] : 0;
    }
    static bool isValidCodeRate(int codeRate) { return (codeRate >= -1) && (codeRate <= 4); }
    static bool isValidScramblingCode(int n) { return (n >= 0) && (n < 262142); }
    static bool isValidISI(int isi) { return (isi >= -1) && (isi <= 255); }
    static bool isValidFreesatRegion(int region) { return (region >= 0) && (region <= 65535); }
    //! The channel's bandwidth: the terrestrial channel's, or (1 + roll-off) times the symbol rate
    //! Of the symbol rate set, or symbolRate (as found, when automatic): 0 if neither's known
    //! Of the signal: the symbol rate times 1 + roll-off, or the terrestrial carriers' span (7.61 MHz in 8 MHz, 7.77 MHz
    //! in DVB-T2's extended carrier mode)
    int occupiedBandwidth(double symbolRate = 0.0, bool extended = false) const;
    static bool isValidEPGReport(int epgReport) { return (epgReport >= 0) && (epgReport <= 2); }
    static QList<ScheduledRecording> deserializeSchedule(const QByteArray& data);
};

#endif // INCLUDE_DVBDEMODSETTINGS_H
