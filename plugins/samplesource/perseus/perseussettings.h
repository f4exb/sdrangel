///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSSETTINGS_H_
#define PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSSETTINGS_H_

#include <QtGlobal>
#include <QString>

struct PerseusSettings
{
    typedef enum
    {
        Attenuator_None,
        Attenuator_10dB,
        Attenuator_20dB,
        Attenuator_30dB,
        Attenuator_last
    } Attenuator;

    quint64 m_centerFrequency;
    qint32  m_LOppmTenths;
    quint32 m_devSampleRateIndex;
    quint32 m_log2Decim;
    bool m_transverterMode;
    qint64 m_transverterDeltaFrequency;
    bool m_iqOrder;
    bool m_adcDither;
    bool m_adcPreamp;
    bool m_wideBand;
    Attenuator m_attenuator;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    PerseusSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const PerseusSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};



#endif /* PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSSETTINGS_H_ */
