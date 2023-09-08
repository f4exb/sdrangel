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

#ifndef PLUGINS_SAMPLESOURCE_ANDROIDSDRDRIVERINPUT_ANDROIDSDRDRIVERINPUTSETTINGS_H_
#define PLUGINS_SAMPLESOURCE_ANDROIDSDRDRIVERINPUT_ANDROIDSDRDRIVERINPUTSETTINGS_H_

#include <QByteArray>
#include <QString>

struct AndroidSDRDriverInputSettings
{
    static const int m_maxGains = 2;

    uint64_t m_centerFrequency;
    qint32   m_loPpmCorrection;
    bool     m_dcBlock;
    bool     m_iqCorrection;
    bool     m_biasTee;
    bool     m_directSampling;          // RTLSDR only
    int      m_devSampleRate;
    qint32   m_gain[m_maxGains];        // 10ths of a dB
    bool     m_agc;
    qint32   m_rfBW;
    qint32   m_sampleBits;              // Number of bits used to transmit IQ samples (8,16)
    quint16  m_dataPort;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    AndroidSDRDriverInputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const AndroidSDRDriverInputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* PLUGINS_SAMPLESOURCE_ANDROIDSDRDRIVERINPUT_ANDROIDSDRDRIVERINPUTSETTINGS_H_ */
