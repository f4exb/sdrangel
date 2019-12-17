///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELRX_DEMODNFMTEST_NFMDEMODTESTSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODNFMTEST_NFMDEMODTESTSETTINGS_H_

#include <stdint.h>

class Serializable;

struct NFMDemodTestSettings
{
    static const int m_nbRfBW;
    static const int m_rfBW[];
    static const int m_fmDev[];

    int32_t m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_afBandwidth;
    int  m_fmDeviation;
    int  m_squelchGate;
    bool m_deltaSquelch;
    Real m_squelch; //!< deci-Bels
    Real m_volume;
    bool m_ctcssOn;
    bool m_audioMute;
    int  m_ctcssIndex;
    quint32 m_rgbColor;
    QString m_title;
    QString m_audioDeviceName;
    bool m_highPass;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    Serializable *m_channelMarker;

    NFMDemodTestSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static int getRFBW(int index);
    static int getFMDev(int index);
    static int getRFBWIndex(int rfbw);
};



#endif /* PLUGINS_CHANNELRX_DEMODNFMTEST_NFMDEMODTESTSETTINGS_H_ */
