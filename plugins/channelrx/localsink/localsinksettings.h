///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_LOCALSINKSETTINGS_H_
#define INCLUDE_LOCALSINKSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "dsp/fftwindow.h"

class Serializable;

struct LocalSinkSettings
{
    int m_localDeviceIndex;
    quint32 m_rgbColor;
    QString m_title;
    uint32_t m_log2Decim;
    uint32_t m_filterChainHash;
    bool m_play;
    bool m_dsp;
    int m_gaindB;
    bool m_fftOn;
    uint32_t m_log2FFT;
    FFTWindow::Function m_fftWindow;
    bool m_reverseFilter;
    static const uint32_t m_maxFFTBands = 20;
    std::vector<std::pair<float, float>> m_fftBands;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    Serializable *m_spectrumGUI;
    Serializable *m_channelMarker;
    Serializable *m_rollupState;

    LocalSinkSettings();
    void resetToDefaults();
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const LocalSinkSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* INCLUDE_LOCALSINKSETTINGS_H_ */
