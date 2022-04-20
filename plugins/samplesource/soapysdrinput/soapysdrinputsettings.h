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

#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUTSETTINGS_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUTSETTINGS_H_

#include <QtGlobal>
#include <QString>
#include <QVariant>
#include <QMap>

struct SoapySDRInputSettings {
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

    quint64 m_centerFrequency;
    qint32 m_LOppmTenths;
    qint32 m_devSampleRate;
    quint32 m_log2Decim;
    fcPos_t m_fcPos;
    bool m_softDCCorrection;
    bool m_softIQCorrection;
    bool m_transverterMode;
    qint64 m_transverterDeltaFrequency;
    bool m_iqOrder;
    QString m_antenna;
    quint32 m_bandwidth;
    QMap<QString, double> m_tunableElements;
    qint32 m_globalGain;
    QMap<QString, double> m_individualGains;
    bool m_autoGain;
    bool m_autoDCCorrection;
    bool m_autoIQCorrection;
    std::complex<double> m_dcCorrection;
    std::complex<double> m_iqCorrection;
    QMap<QString, QVariant> m_streamArgSettings;
    QMap<QString, QVariant> m_deviceArgSettings;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    SoapySDRInputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

private:
    QByteArray serializeNamedElementMap(const QMap<QString, double>& map) const;
    void deserializeNamedElementMap(const QByteArray& data, QMap<QString, double>& map);
    QByteArray serializeArgumentMap(const QMap<QString, QVariant>& map) const;
    void deserializeArgumentMap(const QByteArray& data, QMap<QString, QVariant>& map);
};

#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUTSETTINGS_H_ */
