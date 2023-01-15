///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef PLUGINS_CHANNELRX_DEMODFT8_FT8DEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODFT8_FT8DEMODSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "dsp/fftwindow.h"

class Serializable;

struct FT8DemodFilterSettings
{
    int  m_spanLog2;
    Real m_rfBandwidth;
    Real m_lowCutoff;
    FFTWindow::Function m_fftWindow;

    FT8DemodFilterSettings() :
        m_spanLog2(2),
        m_rfBandwidth(3000),
        m_lowCutoff(200),
        m_fftWindow(FFTWindow::Blackman)
    {}
};

struct FT8DemodSettings
{
    qint32 m_inputFrequencyOffset;
    // Real m_rfBandwidth;
    // Real m_lowCutoff;
    Real m_volume;
    // int  m_spanLog2;
    bool m_agc;
    quint32 m_rgbColor;
    QString m_title;
    int m_ft8SampleRate;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    // FFTWindow::Function m_fftWindow;
    std::vector<FT8DemodFilterSettings> m_filterBank;
    unsigned int m_filterIndex;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;
    Serializable *m_rollupState;

    FT8DemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static const int m_minPowerThresholdDB;
    static const float m_mminPowerThresholdDBf;
};


#endif /* PLUGINS_CHANNELRX_DEMODFT8_FT8DEMODSETTINGS_H_ */
