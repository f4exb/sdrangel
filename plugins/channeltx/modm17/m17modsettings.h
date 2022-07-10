///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODM17_M17MODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODM17_M17MODSETTINGS_H_

#include <QByteArray>

#include "dsp/cwkeyersettings.h"

class Serializable;

struct M17ModSettings
{
    enum M17Mode
    {
        M17ModeNone,
        M17ModeFMTone,
        M17ModeFMAudio,
        M17ModeM17Audio,
        M17ModeM17Packet,
        M17ModeM17BERT
    };

    enum AudioType
    {
        AudioNone,
        AudioFile,
        AudioInput
    };

    enum PacketType
    {
        PacketSMS,
        PacketAPRS
    };

    qint64 m_inputFrequencyOffset;
    float m_rfBandwidth;
    float m_fmDeviation;
    float m_toneFrequency;
    float m_volumeFactor;
    bool m_channelMute;
    bool m_playLoop;
    quint32 m_rgbColor;
    QString m_title;
    M17Mode m_m17Mode;
    AudioType m_audioType;
    PacketType m_packetType;
    QString m_audioDeviceName;         //!< This is the audio device you get the audio samples from
    QString m_feedbackAudioDeviceName; //!< This is the audio device you send the audio samples to for audio feedback
    float m_feedbackVolumeFactor;
    bool m_feedbackAudioEnable;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    QString m_sourceCall;
    QString m_destCall;
    bool m_insertPosition;
    uint8_t m_can;

    QString m_smsText;
    bool m_loopPacket;
    uint32_t m_loopPacketInterval;

    QString m_aprsCallsign;
    QString m_aprsTo;
    QString m_aprsVia;
    QString m_aprsData;
    bool m_aprsInsertPosition;

    Serializable *m_channelMarker;
    Serializable *m_rollupState;

    M17ModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const M17ModSettings& settings);
};



#endif /* PLUGINS_CHANNELTX_MODM17_M17MODSETTINGS_H_ */
