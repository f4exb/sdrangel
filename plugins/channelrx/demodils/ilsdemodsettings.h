///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ILSDEMODSETTINGS_H
#define INCLUDE_ILSDEMODSETTINGS_H

#include <QByteArray>

#include "util/baudot.h"

class Serializable;

struct ILSDemodSettings
{
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    enum Mode {
        LOC,
        GS
    } m_mode;
    int m_frequencyIndex;
    int m_squelch;
    Real m_volume;
    bool m_audioMute;
    bool m_average;
    enum DDMUnits {
        FULL_SCALE,
        PERCENT,
        MICROAMPS
    } m_ddmUnits;
    float m_identThreshold;     //!< Linear SNR threshold for Morse demodulator

    QString m_ident;
    QString m_runway;
    float m_trueBearing;
    float m_slope;              // In %
    QString m_latitude;         // Of threshold. String, so can support multiple formats
    QString m_longitude;
    int m_elevation;            // Of threshold in feet
    float m_glidePath;          // In degrees
    float m_refHeight;          // In metres
    float m_courseWidth;        // In degrees

    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    QString m_logFilename;
    bool m_logEnabled;
    int m_scopeCh1;
    int m_scopeCh2;

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

    Serializable *m_spectrumGUI;
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    static const int ILSDEMOD_CHANNEL_SAMPLE_RATE = 20480; // 2560*8 - Ident is at 1020. Voice 300/3k. SR chosen so 90/150Hz in middle of bin + 20k b/w for offset-carrier
    static const int ILSDEMOD_SPECTRUM_DECIM_LOG2 = 5;
    static const int ILSDEMOD_SPECTRUM_SAMPLE_RATE = ILSDEMOD_CHANNEL_SAMPLE_RATE / (1 << ILSDEMOD_SPECTRUM_DECIM_LOG2);

    ILSDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_ILSDEMODSETTINGS_H */

