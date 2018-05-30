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

#ifndef PLUGINS_CHANNELRX_CHANALYZERNG_CHANALYZERSETTINGS_H_
#define PLUGINS_CHANNELRX_CHANALYZERNG_CHANALYZERSETTINGS_H_

#include <QByteArray>

class Serializable;

struct ChannelAnalyzerSettings
{
    enum InputType
    {
        InputSignal,
        InputPLL,
        InputAutoCorr
    };

    int m_frequency;
    bool m_downSample;
    quint32 m_downSampleRate;
    int m_bandwidth;
    int m_lowCutoff;
    int m_spanLog2;
    bool m_ssb;
    bool m_pll;
    bool m_fll;
    bool m_rrc;
    quint32 m_rrcRolloff; //!< in 100ths
    unsigned int m_pllPskOrder;
    InputType m_inputType;
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;
    Serializable *m_scopeGUI;

    ChannelAnalyzerSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_CHANALYZERNG_CHANALYZERSETTINGS_H_ */
