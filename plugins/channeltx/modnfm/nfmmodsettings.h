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

#ifndef PLUGINS_CHANNELTX_MODNFM_NFMMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODNFM_NFMMODSETTINGS_H_

#include <QByteArray>

class Serializable;

struct NFMModSettings
{
    typedef enum
    {
        NFMModInputNone,
        NFMModInputTone,
        NFMModInputFile,
        NFMModInputAudio,
        NFMModInputCWTone
    } NFMModInputAF;

    static const int m_nbRfBW;
    static const int m_rfBW[];
    static const int m_nbCTCSSFreqs;
    static const float m_ctcssFreqs[];

    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_afBandwidth;
    float m_fmDeviation;
    float m_toneFrequency;
    float m_volumeFactor;
    quint32 m_audioSampleRate;
    bool m_channelMute;
    bool m_playLoop;
    bool m_ctcssOn;
    int  m_ctcssIndex;
    quint32 m_rgbColor;
    QString m_title;
    NFMModInputAF m_modAFInput;

    Serializable *m_channelMarker;
    Serializable *m_cwKeyerGUI;

    NFMModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static int getRFBW(int index);
    static int getRFBWIndex(int rfbw);
    static float getCTCSSFreq(int index);
    static int getCTCSSFreqIndex(float ctcssFreq);
};



#endif /* PLUGINS_CHANNELTX_MODNFM_NFMMODSETTINGS_H_ */
