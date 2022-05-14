///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODWFM_WFMMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODWFM_WFMMODSETTINGS_H_

#include <QByteArray>

#include "dsp/cwkeyersettings.h"

class Serializable;

struct WFMModSettings
{
    typedef enum
    {
        WFMModInputNone,
        WFMModInputTone,
        WFMModInputFile,
        WFMModInputAudio,
        WFMModInputCWTone
    } WFMModInputAF;

    static const int m_nbRfBW;
    static const int m_rfBW[];

    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_afBandwidth;
    float m_fmDeviation;
    float m_toneFrequency;
    float m_volumeFactor;
    bool m_channelMute;
    bool m_playLoop;
    quint32 m_rgbColor;
    QString m_title;
    WFMModInputAF m_modAFInput;
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
    Serializable *m_cwKeyerGUI;

    CWKeyerSettings m_cwKeyerSettings; //!< For standalone deserialize operation (without m_cwKeyerGUI)
    Serializable *m_rollupState;

    WFMModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    const CWKeyerSettings& getCWKeyerSettings() const { return m_cwKeyerSettings; }
    void setCWKeyerSettings(const CWKeyerSettings& cwKeyerSettings) { m_cwKeyerSettings = cwKeyerSettings; }

    static int getRFBW(int index);
    static int getRFBWIndex(int rfbw);
};



#endif /* PLUGINS_CHANNELTX_MODWFM_WFMMODSETTINGS_H_ */
