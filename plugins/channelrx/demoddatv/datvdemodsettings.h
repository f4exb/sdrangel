///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELRX_DEMODATV_DATVDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODATV_DATVDEMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

#include "leansdr/dvb.h"

class Serializable;

struct DATVDemodSettings
{
    enum dvb_version
    {
        DVB_S,
        DVB_S2
    };

    enum DATVModulation
    {
        BPSK,
        QPSK,
        PSK8,
        APSK16,
        APSK32,
        APSK64E,
        QAM16,
        QAM64,
        QAM256
    };

    enum dvb_sampler
    {
        SAMP_NEAREST,
        SAMP_LINEAR,
        SAMP_RRC
    };

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_rfBandwidth;
    int m_centerFrequency;
    dvb_version m_standard;
    DATVModulation m_modulation;
    leansdr::code_rate m_fec;
    bool m_audioMute;
    QString m_audioDeviceName;
    int m_symbolRate;
    int m_notchFilters;
    bool m_allowDrift;
    bool m_fastLock;
    dvb_sampler m_filter;
    bool m_hardMetric;
    float m_rollOff;
    bool m_viterbi;
    int m_excursion;
    int m_audioVolume;
    bool m_videoMute;

    DATVDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void debug(const QString& msg) const;
    bool isDifferent(const DATVDemodSettings& other);
};

#endif // PLUGINS_CHANNELRX_DEMODATV_DATVDEMODSETTINGS_H_