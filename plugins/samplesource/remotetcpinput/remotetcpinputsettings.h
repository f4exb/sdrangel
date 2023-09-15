///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTSETTINGS_H_
#define PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTSETTINGS_H_

#include <QByteArray>
#include <QString>

struct RemoteTCPInputSettings
{
    static const int m_maxGains = 3;

    uint64_t m_centerFrequency;
    qint32   m_loPpmCorrection;
    bool     m_dcBlock;
    bool     m_iqCorrection;
    bool     m_biasTee;
    bool     m_directSampling;          // RTLSDR only
    int      m_devSampleRate;
    int      m_log2Decim;
    qint32   m_gain[m_maxGains];        // 10ths of a dB
    bool     m_agc;
    qint32   m_rfBW;
    qint32   m_inputFrequencyOffset;
    qint32   m_channelGain;             // In dB
    qint32   m_channelSampleRate;
    bool     m_channelDecimation;       // If false, m_channelSampleRate==m_devSampleRate
    qint32   m_sampleBits;              // Number of bits used to transmit IQ samples (8,16,24,32)
    QString  m_dataAddress;
    quint16  m_dataPort;
    bool     m_overrideRemoteSettings;  // When connected, apply local settings to remote, or apply remote settings to local
    float    m_preFill;                 // Input buffer prefill in seconds
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    QStringList m_addressList;          // List of dataAddresses that have been used in the past

    RemoteTCPInputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const RemoteTCPInputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTSETTINGS_H_ */
