///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELRX_UDPSRC_UDPSRCSETTINGS_H_
#define PLUGINS_CHANNELRX_UDPSRC_UDPSRCSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

struct Serializable;

struct UDPSrcSettings
{
    enum SampleFormat {
        FormatS16LE,
        FormatNFM,
        FormatNFMMono,
        FormatLSB,
        FormatUSB,
        FormatLSBMono,
        FormatUSBMono,
        FormatAMMono,
        FormatAMNoDCMono,
        FormatAMBPFMono,
        FormatNone
    };

    float m_outputSampleRate;
    SampleFormat m_sampleFormat;
    float m_inputSampleRate;
    int64_t m_inputFrequencyOffset;
    float m_rfBandwidth;
    int m_fmDeviation;
    bool m_channelMute;
    float m_gain;
    int  m_squelchdB;  //!< power dB
    int  m_squelchGate; //!< 100ths seconds
    bool m_squelchEnabled;
    bool m_agc;
    bool m_audioActive;
    bool m_audioStereo;
    int m_volume;
    quint32 m_rgbColor;

    QString m_udpAddress;
    uint16_t m_udpPort;
    uint16_t m_audioPort;

    QString m_title;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;

    UDPSrcSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_UDPSRC_UDPSRCSETTINGS_H_ */
