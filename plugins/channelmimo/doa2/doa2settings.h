///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_DOA2SETTINGS_H
#define INCLUDE_DOA2SETTINGS_H

#include <QByteArray>
#include <QString>

class Serializable;

struct DOA2Settings
{
    enum CorrelationType
    {
        Correlation0,
        Correlation1,
        CorrelationFFT,
    };

    CorrelationType m_correlationType;
    quint32 m_rgbColor;
    QString m_title;
    uint32_t m_log2Decim;
    uint32_t m_filterChainHash;
    int m_phase;
    int m_antennaAz;
    uint32_t m_basebandDistance; //!< in millimeters
    int m_squelchdB;
    int m_fftAveragingIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    Serializable *m_channelMarker;
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;

    DOA2Settings();
    void resetToDefaults();
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    static int getAveragingValue(int averagingIndex);
    static int getAveragingIndex(int averagingValue);
    static const int m_averagingMaxExponent = 5; //!< Max 1M (10 * 10^5)
};

#endif // INCLUDE_DOA2SETTINGS_H
