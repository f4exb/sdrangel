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
    enum M17ModInputAF
    {
        M17ModInputNone,
        M17ModInputFile,
        M17ModInputAudio,
        M17ModInputTone
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
    M17ModInputAF m_modAFInput;
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

    Serializable *m_channelMarker;
    Serializable *m_rollupState;

    M17ModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELTX_MODM17_M17MODSETTINGS_H_ */
