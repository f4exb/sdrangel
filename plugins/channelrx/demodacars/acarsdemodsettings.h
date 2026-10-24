///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ACARSDEMODSETTINGS_H
#define INCLUDE_ACARSDEMODSETTINGS_H

#include <QByteArray>
#include <QHash>

#include "dsp/dsptypes.h"
#include "util/aircraftreport.h"

class Serializable;

// Number of columns in the table.
#define ACARSDEMOD_COLUMNS 23

struct AcarsDemodSettings
{
    enum Mode {
        ACARS = 0,      //!< Plain old ACARS: 2400 baud MSK on AM
        VDL2 = 1,       //!< VDL Mode 2: 10500 baud D8PSK, ACARS over AVLC
        HFDL = 2,       //!< HFDL: 1800 baud M-PSK on a 1440 Hz subcarrier, ACARS over MPDU/LPDU
        Aero = 3,       //!< Inmarsat Classic Aero: L band satellite, ACARS over signal units
        Iridium = 4,    //!< Iridium: L band satellite, burst DQPSK from low earth orbit.               
        ModeCount
    };

    // The protocol tag an AircraftReport gets for a given mode.
    static AircraftReport::Protocol protocolForMode(int mode)
    {
        switch (mode)
        {
        case VDL2: return AircraftReport::VDL2;
        case HFDL: return AircraftReport::HFDL;
        case Aero: return AircraftReport::AERO;
        case Iridium: return AircraftReport::IRIDIUM;
        default:   return AircraftReport::ACARS;
        }
    }

    //! Whether a change moves the channel sample rate, and so needs the channelizer
    //! retuned, the resampler rebuilt and the receivers restarted.
    static bool rateChanged(const AcarsDemodSettings& a, const AcarsDemodSettings& b)
    {
        if (a.m_mode != b.m_mode) {
            return true;
        }

        switch (a.m_mode)
        {
        case Aero:    return a.m_aeroChannel != b.m_aeroChannel;
        case Iridium: return a.m_iridiumBandwidth != b.m_iridiumBandwidth;
        default:      return false;
        }
    }

    //! How many channelizer outputs each Iridium scan width uses. They have to be powers
    //! of two, and the sample rate the channel needs is this times the 31.25 kHz channel
    //! spacing - so 256 channels is 8 MS/s and covers 8 MHz, the whole Iridium band.
    static int iridiumScanChannels(int bandwidth)
    {
        static const int channels[] = { 32, 64, 128, 256 };
        const int count = (int) (sizeof(channels) / sizeof(channels[0]));

        return channels[((bandwidth < 0) || (bandwidth >= count)) ? (count - 1) : bandwidth];
    }

    static const int IridiumScanWidths = 4;

    qint32 m_inputFrequencyOffset;
    int m_mode;                     //!< One of enum Mode
    //!< Which Aero rate and channel is tuned, an AcarsAeroReceiver::Submode. Unlike
    //!< HFDL, where the M1 sequence identifies the rate, these are properties of the
    //!< frequency tuned, so they are a setting rather than something detected.
    int m_aeroChannel;
    //!< How much of the band the Iridium scanner covers. Iridium assigns a duplex channel
    //!< when a call is set up, out of about 240 across 7 MHz, so watching one channel sees
    //!< a few percent of the traffic - the scanner splits the whole width with a polyphase
    //!< channelizer instead and runs a receiver on every output. An index into the table
    //!< in iridiumScanChannels() below
    int m_iridiumBandwidth;
    Real m_rfBandwidth;
    Real m_correlationThreshold;   //!< Pre-key detection threshold, 0 to 1
    QString m_filter;       // Regular expression the filtered column must match
    int m_filterColumn;     // Which column to filter: 0 Registration, 1 Flight, 2 Label, 3 Message
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    int m_scopeCh1;
    int m_scopeCh2;
    bool m_displayPhotos;
    //!< Hide frames carrying no usable information from the table - VDL-2
    //!< supervisory/ack frames (RR etc) and Aero satellite housekeeping. They are
    //!< still received and still held in the table, only not shown
    bool m_hideNoInfo;
    //!< Show the date as well as the time in the Date/Time column. The date is only
    //!< worth the width once a session has run past midnight
    bool m_showDate;
    //!< How the table and the decode view divide the space, from QSplitter::saveState
    QByteArray m_splitterState;
    bool m_displayChart;            //!< Show the frames-per-second chart
    QString m_logFilename;
    bool m_logEnabled;


    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    QString m_audioDeviceName;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_columnIndexes[ACARSDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[ACARSDEMOD_COLUMNS];  //!< Size of the columns in the table

    AcarsDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_ACARSDEMODSETTINGS_H */
