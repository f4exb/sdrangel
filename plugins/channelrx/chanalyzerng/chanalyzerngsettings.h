///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
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

#ifndef PLUGINS_CHANNELRX_CHANALYZERNG_CHANALYZERNGSETTINGS_H_
#define PLUGINS_CHANNELRX_CHANALYZERNG_CHANALYZERNGSETTINGS_H_

#include <QByteArray>

class Serializable;

struct ChannelAnalyzerNGSettings
{
    int m_frequency;
    bool m_downSample;
    quint32 m_downSampleRate;
    int m_bandwidth;
    int m_lowCutoff;
    int m_spanLog2;
    bool m_ssb;
    bool m_pll;
    bool m_fll;
    unsigned int m_pllPskOrder;
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;
    Serializable *m_scopeGUI;

    ChannelAnalyzerNGSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_CHANALYZERNG_CHANALYZERNGSETTINGS_H_ */
