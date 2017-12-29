///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef PLUGINS_CHANNELRX_DEMODBFM_BFMDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODBFM_BFMDEMODSETTINGS_H_

class Serializable;

struct BFMDemodSettings
{
    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_afBandwidth;
    Real m_volume;
    Real m_squelch;
    quint32 m_audioSampleRate;
    bool m_audioStereo;
    bool m_lsbStereo;
    bool m_showPilot;
    bool m_rdsActive;
    bool m_copyAudioToUDP;
    QString m_udpAddress;
    quint16 m_udpPort;
    quint32 m_rgbColor;
    QString m_title;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;

    static const int m_nbRFBW;
    static const int m_rfBW[];

    BFMDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static int getRFBW(int index);
    static int getRFBWIndex(int rfbw);
};


#endif /* PLUGINS_CHANNELRX_DEMODBFM_BFMDEMODSETTINGS_H_ */
