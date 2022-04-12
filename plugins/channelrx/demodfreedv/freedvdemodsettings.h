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

#ifndef PLUGINS_CHANNELRX_DEMODFREEDV_FREEDVDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODFREEDV_FREEDVDEMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

class Serializable;

struct FreeDVDemodSettings
{
    typedef enum
    {
        FreeDVMode2400A,
        FreeDVMode1600,
        FreeDVMode800XA,
        FreeDVMode700C,
        FreeDVMode700D,
    } FreeDVMode;

    qint32 m_inputFrequencyOffset;
    Real m_volume;
    Real m_volumeIn;
    int  m_spanLog2;
    bool m_audioMute;
    bool m_agc;
    quint32 m_rgbColor;
    QString m_title;
    QString m_audioDeviceName;
    FreeDVMode m_freeDVMode;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
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
    Serializable *m_rollupState;

    FreeDVDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static const int m_minPowerThresholdDB;
    static const float m_mminPowerThresholdDBf;

    static int getHiCutoff(FreeDVMode freeDVMode);
    static int getLowCutoff(FreeDVMode freeDVMode);
    static int getModSampleRate(FreeDVMode freeDVMode);
};


#endif /* PLUGINS_CHANNELRX_DEMODFREEDV_FREEDVDEMODSETTINGS_H_ */
