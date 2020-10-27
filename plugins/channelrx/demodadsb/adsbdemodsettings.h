///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_

#include <QtGlobal>
#include <QString>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

struct ADSBDemodSettings
{
    int32_t m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_correlationThreshold;
    int m_samplesPerBit;
    int m_removeTimeout;                // Time in seconds before removing an aircraft, unless a new frame is received
    bool m_beastEnabled;
    QString m_beastHost;
    uint16_t m_beastPort;

    quint32 m_rgbColor;
    QString m_title;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    Serializable *m_channelMarker;

    ADSBDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_ */
