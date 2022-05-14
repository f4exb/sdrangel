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

#ifndef INCLUDE_IEEE_802_15_4_MODSETTINGS_H
#define INCLUDE_IEEE_802_15_4_MODSETTINGS_H

#include <QByteArray>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

struct IEEE_802_15_4_ModSettings
{
    enum Modulation {
        BPSK,
        OQPSK
    };

    enum PulseShaping {
        RC,
        SINE
    };

    static const int infinitePackets = -1;

    qint64 m_inputFrequencyOffset;
    Modulation m_modulation;
    int m_bitRate;
    bool m_subGHzBand;
    Real m_rfBandwidth;
    Real m_gain;
    bool m_channelMute;
    bool m_repeat;
    Real m_repeatDelay;
    int m_repeatCount;
    int m_rampUpBits;
    int m_rampDownBits;
    int m_rampRange;
    bool m_modulateWhileRamping;
    int m_lpfTaps;
    bool m_bbNoise;
    bool m_writeToFile;
    int m_spectrumRate;
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
    bool m_scramble;
    int m_polynomial;
    PulseShaping m_pulseShaping;
    float m_beta;
    int m_symbolSpan;
    bool m_udpEnabled;
    bool m_udpBytesFormat; //!< true for bytes payload
    QString m_udpAddress;
    uint16_t m_udpPort;
    Serializable *m_rollupState;
    static const int m_udpBufferSize = 100000;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    IEEE_802_15_4_ModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    bool setPHY(QString phy);
    QString getPHY() const;
    int getChipRate() const;
};

#endif /* INCLUDE_IEEE_802_15_4_MODSETTINGS_H */
