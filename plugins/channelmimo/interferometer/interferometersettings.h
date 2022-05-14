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

#ifndef INCLUDE_INTERFEROMETERSETTINGS_H
#define INCLUDE_INTERFEROMETERSETTINGS_H

#include <QByteArray>
#include <QString>

class Serializable;

struct InterferometerSettings
{
    enum CorrelationType
    {
        Correlation0,
        Correlation1,
        CorrelationAdd,
        CorrelationMultiply,
        CorrelationIFFT,
        CorrelationIFFTStar,
        CorrelationFFT,
        CorrelationIFFT2
    };

    CorrelationType m_correlationType;
    quint32 m_rgbColor;
    QString m_title;
    uint32_t m_log2Decim;
    uint32_t m_filterChainHash;
    int m_phase;
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
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;

    InterferometerSettings();
    void resetToDefaults();
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif // INCLUDE_INTERFEROMETERSETTINGS_H
