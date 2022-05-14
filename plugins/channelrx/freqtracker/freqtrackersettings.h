///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef PLUGINS_CHANNELRX_FREQTRACKER_FREQTRACKERSETTINGS_H_
#define PLUGINS_CHANNELRX_FREQTRACKER_FREQTRACKERSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"

class Serializable;

struct FreqTrackerSettings
{
    enum TrackerType
    {
        TrackerNone,
        TrackerFLL,
        TrackerPLL
    };

    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    uint32_t m_log2Decim;
    Real m_squelch;
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;
    int  m_spanLog2;
    float m_alphaEMA; //!< alpha factor for delta frequency EMA
    bool m_tracking;
    TrackerType m_trackerType;
    uint32_t m_pllPskOrder;
    bool m_rrc;
    uint32_t m_rrcRolloff; //!< in 100ths
    int m_squelchGate; //!< in 10s of ms
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    FreqTrackerSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_FREQTRACKER_FREQTRACKERSETTINGS_H_ */
