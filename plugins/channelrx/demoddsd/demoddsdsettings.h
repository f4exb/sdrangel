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

#ifndef PLUGINS_CHANNELRX_DEMODDSD_DEMODDSDSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODDSD_DEMODDSDSETTINGS_H_

#include <QByteArray>

class Serializable;

struct DSDDemodSettings
{
    int m_inputSampleRate;
    qint64 m_inputFrequencyOffset;
    int  m_rfBandwidth;
    int  m_demodGain;
    int  m_volume;
    int  m_baudRate;
    int  m_fmDeviation;
    int  m_squelchGate;
    Real m_squelch;
    bool m_audioMute;
    quint32 m_audioSampleRate;
    bool m_enableCosineFiltering;
    bool m_syncOrConstellation;
    bool m_slot1On;
    bool m_slot2On;
    bool m_tdmaStereo;
    bool m_pllLock;
    bool m_udpCopyAudio;
    QString m_udpAddress;
    quint16 m_udpPort;

    Serializable *m_channelMarker;
    Serializable *m_scopeGUI;

    DSDDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};


#endif /* PLUGINS_CHANNELRX_DEMODDSD_DEMODDSDSETTINGS_H_ */
