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

#ifndef PLUGINS_CHANNELTX_MODNFM_NFMMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODNFM_NFMMODSETTINGS_H_

#include <QByteArray>

#include "dsp/cwkeyersettings.h"

class Serializable;

struct NFMModSettings
{
    typedef enum
    {
        NFMModInputNone,
        NFMModInputTone,
        NFMModInputFile,
        NFMModInputAudio,
        NFMModInputCWTone
    } NFMModInputAF;

    static const int m_nbChannelSpacings;
    static const int m_channelSpacings[];
    static const int m_rfBW[];
    static const int m_afBW[];
    static const int m_fmDev[];

    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_afBandwidth;
    float m_fmDeviation;
    float m_toneFrequency;
    float m_volumeFactor;
    bool m_channelMute;
    bool m_playLoop;
    bool m_ctcssOn;
    int  m_ctcssIndex;
    bool m_dcsOn;
    int m_dcsCode;
    bool m_dcsPositive;
    bool m_preEmphasisOn;
    bool m_bpfOn;
    quint32 m_rgbColor;
    QString m_title;
    NFMModInputAF m_modAFInput;
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

    NFMModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    const CWKeyerSettings& getCWKeyerSettings() const { return m_cwKeyerSettings; }
    void setCWKeyerSettings(const CWKeyerSettings& cwKeyerSettings) { m_cwKeyerSettings = cwKeyerSettings; }

    static int getChannelSpacing(int index);
    static int getChannelSpacingIndex(int channelSpacing);
    static int getRFBW(int index);
    static int getRFBWIndex(int rfbw);
    static int getAFBW(int index);
    static int getAFBWIndex(int afbw);
    static int getFMDev(int index);
    static int getFMDevIndex(int fmDev);
    static int getNbCTCSSFreq();
    static float getCTCSSFreq(int index);
    static int getCTCSSFreqIndex(float ctcssFreq);
};



#endif /* PLUGINS_CHANNELTX_MODNFM_NFMMODSETTINGS_H_ */
