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

#ifndef INCLUDE_APTDEMODSETTINGS_H
#define INCLUDE_APTDEMODSETTINGS_H

#include <QByteArray>
#include <QHash>

class Serializable;

struct APTDemodSettings
{
    qint32 m_inputFrequencyOffset;
    float m_rfBandwidth;
    float m_fmDeviation;
    bool m_cropNoise;
    bool m_denoise;
    bool m_linearEqualise;
    bool m_histogramEqualise;
    bool m_precipitationOverlay;
    bool m_flip;
    enum ChannelSelection {BOTH_CHANNELS, CHANNEL_A, CHANNEL_B} m_channels;
    bool m_decodeEnabled;
    bool m_satelliteTrackerControl;             //! Whether Sat Tracker can set direction of pass
    QString m_satelliteName;                    //!< All, NOAA 15, NOAA 18 or NOAA 19
    bool m_autoSave;
    QString m_autoSavePath;
    int m_autoSaveMinScanLines;

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

    APTDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_APTDEMODSETTINGS_H */
