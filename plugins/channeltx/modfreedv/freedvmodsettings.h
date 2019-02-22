///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

    static const int m_nbAGCTimeConstants;
    static const int m_agcTimeConstant[];

    qint64 m_inputFrequencyOffset;
    Real m_bandwidth;
    Real m_lowCutoff;
    bool m_usb;
    float m_toneFrequency;
    float m_volumeFactor;
    int  m_spanLog2;
    bool m_audioBinaural;
    bool m_audioFlipChannels;
    bool m_dsb;
    bool m_audioMute;
    bool m_playLoop;
    bool m_agc;
    float m_agcOrder;
    int m_agcTime;
    bool m_agcThresholdEnable;
    int m_agcThreshold;
    int m_agcThresholdGate;
    int m_agcThresholdDelay;
    quint32 m_rgbColor;

    QString m_title;
    FreeDVModInputAF m_modAFInput;
    QString m_audioDeviceName;

    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;
    Serializable *m_cwKeyerGUI;

    FreeDVModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static int getAGCTimeConstant(int index);
    static int getAGCTimeConstantIndex(int agcTimeConstant);
};


#endif /* PLUGINS_CHANNELTX_MODFREEDV_FREEDVMODSETTINGS_H_ */
