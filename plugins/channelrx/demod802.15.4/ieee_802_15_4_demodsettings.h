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

#ifndef INCLUDE_IEEE_802_15_4_DEMODSETTINGS_H
#define INCLUDE_IEEE_802_15_4_DEMODSETTINGS_H

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"
#include "util/ieee_802_15_4.h"

class Serializable;

#define IEEE_802_15_4_DEMOD_COLUMNS 16

struct IEEE_802_15_4_DemodSettings
{
    enum UDPFormat {
        ZEP,        //!< ZigBee Encapsulation Protocol v2, as decoded by Wireshark
        Raw         //!< PSDU (MAC frame including FCS)
    };

    enum FrameTypeFilter {
        FilterAll,
        FilterBeacon,
        FilterData,
        FilterAck,
        FilterCommand,
        FilterOther
    };

    qint64 m_inputFrequencyOffset;
    QString m_phy;                  //!< PHY name, as in IEEE_802_15_4_PHY::getPHYNames()
    Real m_rfBandwidth;
    int m_samplesPerChip;           //!< Channel sample rate is this multiplied by the chip rate
    Real m_maxFrequencyOffset;      //!< Maximum carrier frequency offset to search for during acquisition in Hz
    Real m_threshold;               //!< Preamble detection threshold (0-1)
    bool m_showBadFCS;              //!< Show frames with invalid FCS
    FrameTypeFilter m_filterFrameType;
    QString m_filterPANID;          //!< Regular expression to match against PAN IDs
    QString m_filterAddress;        //!< Regular expression to match against source or destination address
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    UDPFormat m_udpFormat;
    QString m_logFilename;          //!< .csv or .pcap
    bool m_logEnabled;
    bool m_useFileTime;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
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

    int m_columnIndexes[IEEE_802_15_4_DEMOD_COLUMNS];   //!< How the columns are ordered in the table
    int m_columnSizes[IEEE_802_15_4_DEMOD_COLUMNS];     //!< Size of the columns in the table

    static const int m_scopeStreams = 8;
    //! Range of preamble detection threshold. Too low a value results in continuous detections
    static constexpr float m_minThreshold = 0.005f;
    static constexpr float m_maxThreshold = 1.0f;
    static const int m_zepPort = 17754;

    IEEE_802_15_4_DemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_DemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    IEEE_802_15_4_PHY getPHY() const;
    int getChannelSampleRate() const;
    //! Set m_phy and parameters that depend on it to default values
    bool setPHY(const QString& phy);
    //! Default maximum frequency offset for a PHY: +-40ppm at each end at the highest frequency band the PHY is used in
    static Real getDefaultMaxFrequencyOffset(const IEEE_802_15_4_PHY& phy);
};

#endif /* INCLUDE_IEEE_802_15_4_DEMODSETTINGS_H */
