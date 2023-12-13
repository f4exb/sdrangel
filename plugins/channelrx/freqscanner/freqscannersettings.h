///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#ifndef INCLUDE_FREQSCANNERSETTINGS_H
#define INCLUDE_FREQSCANNERSETTINGS_H

#include <QByteArray>

class Serializable;
class ChannelAPI;

// Number of columns in the table
#define FREQSCANNER_COLUMNS           10

struct FreqScannerSettings
{
    struct AvailableChannel
    {
        int m_deviceSetIndex;
        int m_channelIndex;
        int m_streamIndex;

        AvailableChannel() = default;
        AvailableChannel(const AvailableChannel&) = default;
        AvailableChannel& operator=(const AvailableChannel&) = default;
    };

    struct FrequencySettings {
        qint64 m_frequency;
        bool m_enabled;
        QString m_notes;
        QString m_threshold;        // QStrings used, as we allow "" for no setting
        QString m_channel;
        QString m_channelBandwidth;
        QString m_squelch;

        QByteArray serialize() const;
        bool deserialize(const QByteArray& data);
    };

    qint32 m_inputFrequencyOffset;  //!< Not modifable in GUI
    qint32 m_channelBandwidth;      //!< Channel bandwidth
    qint32 m_channelFrequencyOffset;//!< Minium DC offset of tuned channel
    Real m_threshold;               //!< Power threshold in dB
    QString m_channel;              //!< Channel (E.g: R1:4) to tune to active frequency
    QList<FrequencySettings> m_frequencySettings; //!< Frequencies to scan and corresponding settings
    float m_scanTime;               //!< In seconds
    float m_retransmitTime;         //!< In seconds
    int m_tuneTime;                 //!< In milliseconds
    enum Priority {
        MAX_POWER,
        TABLE_ORDER
    } m_priority;                   //!< Which frequency has priority when multiple frequencies are above threshold
    enum Measurement {
        PEAK,
        TOTAL
    } m_measurement;                //!< How power is measured
    enum Mode {
        SINGLE,
        CONTINUOUS,
        SCAN_ONLY
    } m_mode;                       //!< Whether to run a single or many scans

    QList<int> m_columnIndexes;//!< How the columns are ordered in the table
    QList<int> m_columnSizes;  //!< Size of the coumns in the table

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

    FreqScannerSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const FreqScannerSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
    QString getChannel(FreqScannerSettings::FrequencySettings *frequencySettings) const;
    Real getThreshold(FreqScannerSettings::FrequencySettings *frequencySettings) const;
    int getChannelBandwidth(FreqScannerSettings::FrequencySettings *frequencySettings) const;
    FreqScannerSettings::FrequencySettings *getFrequencySettings(qint64 frequency);
};

#endif /* INCLUDE_FREQSCANNERSETTINGS_H */
