///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_

#include <QtGlobal>
#include <QString>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the table
#define ADSBDEMOD_COLUMNS 25

struct ADSBDemodSettings
{
    int32_t m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_correlationThreshold; //!< Correlation power threshold in dB
    int m_samplesPerBit;
    int m_removeTimeout;                //!< Time in seconds before removing an aircraft, unless a new frame is received
    bool m_feedEnabled;
    QString m_feedHost;
    uint16_t m_feedPort;
    enum FeedFormat {
        BeastBinary,
        BeastHex
    } m_feedFormat;

    quint32 m_rgbColor;
    QString m_title;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_columnIndexes[ADSBDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[ADSBDEMOD_COLUMNS];  //!< Size of the coumns in the table

    Serializable *m_channelMarker;

    float m_airportRange;               //!< How far away should we display airports (km)
    enum AirportType {
        Small,
        Medium,
        Large,
        Heliport
    } m_airportMinimumSize;             //!< What's the minimum size airport that should be displayed
    bool m_displayHeliports;            //!< Whether to display heliports on the map
    bool m_flightPaths;                 //!< Whether to display flight paths
    bool m_allFlightPaths;              //!< Whether to display flight paths for all aircraft
    bool m_siUnits;                     //!< Uses m,kph rather than ft/knts
    QString m_tableFontName;            //!< Font to use for table
    int m_tableFontSize;
    bool m_displayDemodStats;
    bool m_correlateFullPreamble;
    bool m_demodModeS;                  //!< Demodulate all Mode-S frames, not just ADS-B
    int m_deviceIndex;                  //!< Device to set to ATC frequencies
    bool m_autoResizeTableColumns;
    int m_interpolatorPhaseSteps;
    float m_interpolatorTapsPerPhase;

    ADSBDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_ */
