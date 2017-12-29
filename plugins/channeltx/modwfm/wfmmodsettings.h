///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODWFM_WFMMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODWFM_WFMMODSETTINGS_H_

#include <QByteArray>

class Serializable;

struct WFMModSettings
{
    static const int m_nbRfBW;
    static const int m_rfBW[];

    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_afBandwidth;
    float m_fmDeviation;
    float m_toneFrequency;
    float m_volumeFactor;
    quint32 m_audioSampleRate;
    bool m_channelMute;
    bool m_playLoop;
    quint32 m_rgbColor;
    QString m_title;

    Serializable *m_channelMarker;
    Serializable *m_cwKeyerGUI;

    WFMModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static int getRFBW(int index);
    static int getRFBWIndex(int rfbw);
};



#endif /* PLUGINS_CHANNELTX_MODWFM_WFMMODSETTINGS_H_ */
