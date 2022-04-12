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

#ifndef PLUGINS_CHANNELTX_MODAIS_AISMODSETTINGS_H
#define PLUGINS_CHANNELTX_MODAIS_AISMODSETTINGS_H

#include <QByteArray>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

struct AISModSettings
{
    enum Status {
        StatusUnderWayUsingEngine,
        StatusAtAnchor,
        StatusNotUnderCommand,
        StatusRestrictedManoeuverability,
        StatusConstrainedByHerDraught,
        StatusMoored,
        StatusAground,
        StatusEngagedInFishing,
        StatusUnderWaySailing,
        StatusNotDefined
    };

    enum MsgType {
        MsgTypeScheduledPositionReport,
        MsgTypeAssignedPositionReport,
        MsgTypeSpecialPositionReport,
        MsgBaseStationReport
    };

    static const int infinitePackets = -1;

    qint64 m_inputFrequencyOffset;
    int m_baud;
    Real m_rfBandwidth;
    Real m_fmDeviation;
    Real m_gain;
    bool m_channelMute;
    bool m_repeat;
    Real m_repeatDelay;
    int m_repeatCount;
    int m_rampUpBits;
    int m_rampDownBits;
    int m_rampRange;
    bool m_rfNoise;
    bool m_writeToFile;
    MsgType m_msgType;
    QString m_mmsi;
    Status m_status;
    float m_latitude;
    float m_longitude;
    float m_course;
    float m_speed;
    int m_heading;
    QString m_data;
    float m_bt;
    int m_symbolSpan;
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    // Sample rate is multiple of 9600 baud rate (use even multiple so Gausian filter has odd number of taps)
    // Is there any benefit to having this higher?
    static const int AISMOD_SAMPLE_RATE = (9600*6);

    AISModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    bool setMode(QString mode);
    QString getMode() const;
    int getMsgId() const;
    Real getRfBandwidth(int modeIndex);
    Real getFMDeviation(int modeIndex);
    float getBT(int modeIndex);
};

#endif /* PLUGINS_CHANNELTX_MODAIS_AISMODSETTINGS_H */
