///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_REMOTECHANNELSINKSETTINGS_H_
#define INCLUDE_REMOTECHANNELSINKSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <QList>

class Serializable;

struct RemoteTCPSinkSettings
{
    enum Protocol {
        RTL0,                           // Compatible with rtl_tcp
        SDRA,                           // SDRangel remote TCP protocol which extends rtl_tcp
        SDRA_WSS                        // SDRA using WebSocket Secure
    };

    enum Compressor {
        FLAC,
        ZLIB
    };

    qint32 m_channelSampleRate;
    qint32 m_inputFrequencyOffset;
    qint32 m_gain;                      // 10ths of a dB
    uint32_t m_sampleBits;
    QString m_dataAddress;
    uint16_t m_dataPort;
    enum Protocol m_protocol;
    bool m_iqOnly;                      // Send uncompressed IQ only (No position or messages)
    Compressor m_compression;           // How IQ stream is compressed
    int m_compressionLevel;
    int m_blockSize;
    bool m_squelchEnabled;
    float m_squelch;
    float m_squelchGate;                // In seconds
    bool m_remoteControl;               // Whether remote control is enabled
    int m_maxClients;
    int m_timeLimit;                    // Time limit per connection in minutes, if server busy. 0 = no limit.
    int m_maxSampleRate;

    QString m_certificate;              // SSL certificate
    QString m_key;                      // SSL key

    bool m_public;                      // Whether to list publically
    QString m_publicAddress;            // IP address / host for public listing
    int m_publicPort;                   // What port number for public listing
    qint64 m_minFrequency;              // Minimum frequency for public listing
    qint64 m_maxFrequency;              // Maximum frequency for public listing
    QString m_antenna;                  // Anntenna description for public listing
    QString m_location;                 // Anntenna location for public listing
    QStringList m_ipBlacklist;          // List of IP addresses to refuse connections from
    bool m_isotropic;                   // Antenna is isotropic
    float m_azimuth;                    // Antenna azimuth angle
    float m_elevation;                  // Antenna elevation angle
    QString m_rotator;                  // Id of Rotator Controller feature to get az/el from

    quint32 m_rgbColor;
    QString m_title;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
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

    RemoteTCPSinkSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const RemoteTCPSinkSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* INCLUDE_REMOTECHANNELSINKSETTINGS_H_ */
