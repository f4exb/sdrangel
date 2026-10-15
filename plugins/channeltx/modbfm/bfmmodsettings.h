///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_CHANNELTX_MODBFM_BFMMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODBFM_BFMMODSETTINGS_H_

#include <QByteArray>

#include "dsp/cwkeyersettings.h"
#include "dsp/dsptypes.h"

class Serializable;

struct BFMModSettings
{
    typedef enum
    {
        BFMModInputNone,
        BFMModInputTone,
        BFMModInputFile,
        BFMModInputAudio,
        BFMModInputCWTone
    } BFMModInputAF;

    static const int m_nbRfBW;
    static const int m_rfBW[];
    static const Real m_minAFBandwidth;
    static const Real m_maxAFBandwidth; //!< Keeps audio clear of the pilot and, in stereo, the RDS band

    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_afBandwidth;
    float m_fmDeviation; //!< Peak frequency deviation in Hz
    float m_toneFrequency;
    float m_volumeFactor;
    bool m_audioStereo;
    int m_preEmphasis; //!< 0: off, 1: 50 us, 2: 75 us
    float m_pilotLevel;
    bool m_rdsActive;
    float m_rdsLevel;
    quint16 m_rdsPI;
    quint8 m_rdsPTY;
    QString m_rdsPS;
    QString m_rdsRadioText;
    QString m_fileName; //!< WAV or raw audio file played when the file input is selected
    bool m_channelMute;
    bool m_playLoop;
    quint32 m_rgbColor;
    QString m_title;
    BFMModInputAF m_modAFInput;
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

    BFMModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const BFMModSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    const CWKeyerSettings& getCWKeyerSettings() const { return m_cwKeyerSettings; }
    void setCWKeyerSettings(const CWKeyerSettings& cwKeyerSettings) { m_cwKeyerSettings = cwKeyerSettings; }

    static int getRFBW(int index);
    static int getRFBWIndex(int rfbw);
    static Real boundRFBandwidth(Real rfBandwidth);
    static Real boundAFBandwidth(Real afBandwidth);
    static float boundFMDeviation(float fmDeviation);
    static float boundToneFrequency(float toneFrequency);
    static float boundVolumeFactor(float volumeFactor);
    static float boundFeedbackVolumeFactor(float feedbackVolumeFactor);
};



#endif /* PLUGINS_CHANNELTX_MODBFM_BFMMODSETTINGS_H_ */
