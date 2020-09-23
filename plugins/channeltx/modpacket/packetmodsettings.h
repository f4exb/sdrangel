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

#ifndef PLUGINS_CHANNELTX_MODPACKET_PACKETMODSETTINGS_H
#define PLUGINS_CHANNELTX_MODPACKET_PACKETMODSETTINGS_H

#include <QByteArray>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

struct PacketModSettings
{
    static const int infinitePackets = -1;

    qint64 m_inputFrequencyOffset;
    enum Modulation {AFSK, FSK} m_modulation;
    int m_baud;
    Real m_rfBandwidth;
    Real m_fmDeviation;
    Real m_gain;
    bool m_channelMute;
    bool m_repeat;
    Real m_repeatDelay;
    int m_repeatCount;
    int m_rampUpBits;
    int m_rampDownBits;
    int m_rampRange;
    bool m_modulateWhileRamping;
    int m_markFrequency;
    int m_spaceFrequency;
    int m_ax25PreFlags;
    int m_ax25PostFlags;
    int m_ax25Control;
    int m_ax25PID;
    bool m_preEmphasis;
    Real m_preEmphasisTau;
    Real m_preEmphasisHighFreq;
    int m_lpfTaps;
    bool m_bbNoise;
    bool m_rfNoise;
    bool m_writeToFile;
    int m_spectrumRate;
    QString m_callsign;
    QString m_to;
    QString m_via;
    QString m_data;
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    bool m_bpf;
    Real m_bpfLowCutoff;
    Real m_bpfHighCutoff;
    int m_bpfTaps;
    bool m_scramble;
    int m_polynomial;
    bool m_pulseShaping;
    float m_beta;
    int m_symbolSpan;

    PacketModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    bool setMode(QString mode);
    QString getMode() const;
};

#endif /* PLUGINS_CHANNELTX_MODPACKET_PACKETMODSETTINGS_H */
