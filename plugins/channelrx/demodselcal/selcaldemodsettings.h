///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_SELCALDEMODSETTINGS_H
#define INCLUDE_SELCALDEMODSETTINGS_H

#include <QByteArray>
#include <QList>
#include <QStringList>

#include "dsp/dsptypes.h"

class Serializable;

struct SELCALDemodSettings {

    enum Modulation {
        USB = 0,
        AM = 1
    };

    static constexpr int SELCALDEMOD_CHANNEL_SAMPLE_RATE = 8000;
    static constexpr int m_columns = 9;
    static constexpr int SELCALDEMOD_SPECTRUM_SAMPLE_RATE = 4000;

    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    int m_modulation;
    Real m_toneFrequencyOffset;
    bool m_autoToneFrequencyOffset;
    Real m_threshold; // dB
    Real m_volume;
    bool m_audioMute;
    int m_squelch;
    QString m_audioDeviceName;
    QString m_filter;
    bool m_udpEnabled;
    QString m_udpAddress;
    quint32 m_udpPort;
    QString m_logFilename;
    bool m_logEnabled;
    quint32 m_rgbColor;
    QString m_title;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    quint32 m_reverseAPIPort;
    quint32 m_reverseAPIDeviceIndex;
    quint32 m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    Serializable *m_channelMarker = nullptr;
    Serializable *m_rollupState = nullptr;
    Serializable *m_spectrumGUI = nullptr;
    QList<int> m_columnIndexes;
    QList<int> m_columnSizes;

    SELCALDemodSettings();
    void resetToDefaults();
    void normalize();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& keys, const SELCALDemodSettings& settings);
    QString getDebugString(const QStringList& keys, bool force = false) const;

    void setChannelMarker(Serializable *marker)
    {
        m_channelMarker = marker;
    }

    void setRollupState(Serializable *state)
    {
        m_rollupState = state;
    }

    void setSpectrumGUI(Serializable *spectrumGUI)
    {
        m_spectrumGUI = spectrumGUI;
    }
};

#endif
