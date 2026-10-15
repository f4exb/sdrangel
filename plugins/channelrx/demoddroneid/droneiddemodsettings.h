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

#ifndef INCLUDE_DRONEIDDEMODSETTINGS_H
#define INCLUDE_DRONEIDDEMODSETTINGS_H

#include <QByteArray>
#include <QList>
#include <QString>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the table
#define DRONEIDDEMOD_COLUMNS 24

struct DroneIDDemodSettings
{
    enum ScanBand {
        BAND_2_4_GHZ,
        BAND_5_8_GHZ,
        BAND_2_4_AND_5_8_GHZ
    };

    enum MapAltitude {
        MAP_ALTITUDE,               //!< Altitude above WGS84 ellipsoid
        MAP_HEIGHT                  //!< Height above take-off point, displayed relative to ground
    };

    qint32 m_inputFrequencyOffset;
    Real m_detectionThreshold;      //!< Normalised cyclic prefix correlation threshold for burst detection
    int m_turboIterations;          //!< Maximum turbo decoder iterations
    QString m_filterSerial;         //!< Regular expression to filter serial numbers in table
    int m_mapTimeout;               //!< Seconds without a message before drone is removed from Map (0 to never remove)
    MapAltitude m_mapAltitude;      //!< Which of the drone's altitude or height is used for its position on the Map
    bool m_scan;                    //!< Step device center frequency through DroneID channels, until a burst is detected
    ScanBand m_scanBand;            //!< Which channels to scan
    float m_scanDwell;              //!< Seconds to wait on each channel for a burst
    float m_scanHold;               //!< Seconds to remain on a channel after the last burst was detected
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;

    QString m_logFilename;
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
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    QList<int> m_columnIndexes;     //!< How the columns are ordered in the table
    QList<int> m_columnSizes;       //!< Size of the columns in the table

    static constexpr int DRONEIDDEMOD_CHANNEL_SAMPLE_RATE = 15360000; //!< 1024 point FFT with 15kHz subcarrier spacing
    static const QList<qint64> m_frequencies2_4GHz;     //!< DroneID center frequencies in Hz
    static const QList<qint64> m_frequencies5_8GHz;

    static QList<qint64> getScanFrequencies(ScanBand band);

    DroneIDDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

private:
    void resetColumns();
};

#endif /* INCLUDE_DRONEIDDEMODSETTINGS_H */
