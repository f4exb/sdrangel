///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODFREEDV_FREEDVMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODFREEDV_FREEDVMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

#include "dsp/cwkeyersettings.h"

class Serializable;

struct FreeDVModSettings
{
    typedef enum
    {
        FreeDVModInputNone,
        FreeDVModInputTone,
        FreeDVModInputFile,
        FreeDVModInputAudio,
        FreeDVModInputCWTone
    } FreeDVModInputAF;

    typedef enum
    {
        FreeDVMode2400A,
        FreeDVMode1600,
        FreeDVMode800XA,
        FreeDVMode700C,
        FreeDVMode700D
    } FreeDVMode;

    qint64 m_inputFrequencyOffset;
    float m_toneFrequency;
    float m_volumeFactor;
    int  m_spanLog2;
    bool m_audioMute;
    bool m_playLoop;
    quint32 m_rgbColor;

    QString m_title;
    FreeDVModInputAF m_modAFInput;
    QString m_audioDeviceName;
    FreeDVMode m_freeDVMode;
    bool m_gaugeInputElseModem; //!< Volume gauge shows speech input level else modem level
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
    Serializable *m_spectrumGUI;
    Serializable *m_cwKeyerGUI;

    CWKeyerSettings m_cwKeyerSettings; //!< For standalone deserialize operation (without m_cwKeyerGUI)
    Serializable *m_rollupState;

    FreeDVModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    const CWKeyerSettings& getCWKeyerSettings() const { return m_cwKeyerSettings; }
    void setCWKeyerSettings(const CWKeyerSettings& cwKeyerSettings) { m_cwKeyerSettings = cwKeyerSettings; }

    static int getHiCutoff(FreeDVMode freeDVMode);
    static int getLowCutoff(FreeDVMode freeDVMode);
    static int getModSampleRate(FreeDVMode freeDVMode);
};


#endif /* PLUGINS_CHANNELTX_MODFREEDV_FREEDVMODSETTINGS_H_ */
