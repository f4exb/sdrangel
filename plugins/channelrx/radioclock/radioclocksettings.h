///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RADIOCLOCKSETTINGS_H
#define INCLUDE_RADIOCLOCKSETTINGS_H

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"

class Serializable;

struct RadioClockSettings
{
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_threshold;               //!< For MSF and DCF in dB
    enum Modulation {
        MSF,
        DCF77,
        TDF
    } m_modulation;
    enum DisplayTZ {
        BROADCAST,
        LOCAL,
        UTC
    } m_timezone;
    int m_scopeCh1;
    int m_scopeCh2;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_scopeGUI;

    RadioClockSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_RADIOCLOCKSETTINGS_H */
