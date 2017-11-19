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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSINKSETTINGS_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSINKSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

struct Serializable;

struct UDPSinkSettings
{
    enum SampleFormat {
        FormatS16LE,
        FormatNFM,
        FormatLSB,
        FormatUSB,
        FormatAM,
        FormatNone
    };

    int m_basebandSampleRate;
    Real m_outputSampleRate;
    SampleFormat m_sampleFormat;
    Real m_inputSampleRate;
    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_lowCutoff;
    int m_fmDeviation;
    Real m_amModFactor;
    bool m_channelMute;
    Real m_gainIn;
    Real m_gainOut;
    Real m_squelch; //!< squared magnitude
    Real m_squelchGate; //!< seconds
    bool m_squelchEnabled;
    bool m_autoRWBalance;
    bool m_stereoInput;
    quint32 m_rgbColor;

    QString m_udpAddress;
    uint16_t m_udpPort;

    QString m_title;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;

    UDPSinkSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};




#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINKSETTINGS_H_ */
