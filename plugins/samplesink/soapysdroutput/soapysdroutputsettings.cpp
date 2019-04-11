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

#include <QtGlobal>
#include <QDataStream>

#include "util/simpleserializer.h"

#include "soapysdroutputsettings.h"

SoapySDROutputSettings::SoapySDROutputSettings()
{
    resetToDefaults();
}

void SoapySDROutputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_LOppmTenths = 0;
    m_devSampleRate = 1024000;
    m_log2Interp = 0;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_antenna = "NONE";
    m_bandwidth = 1000000;
    m_globalGain = 0;
    m_autoGain = false;
    m_autoDCCorrection = false;
    m_autoIQCorrection = false;
    m_dcCorrection = std::complex<double>{0,0};
    m_iqCorrection = std::complex<double>{0,0};
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray SoapySDROutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeS32(2, m_LOppmTenths);
    s.writeU32(3, m_log2Interp);
    s.writeBool(4, m_transverterMode);
    s.writeS64(5, m_transverterDeltaFrequency);
    s.writeString(6, m_antenna);
    s.writeU32(7, m_bandwidth);
    s.writeBlob(8, serializeNamedElementMap(m_tunableElements));
    s.writeS32(12, m_globalGain);
    s.writeBlob(13, serializeNamedElementMap(m_individualGains));
    s.writeBool(14, m_autoGain);
    s.writeBool(15, m_autoDCCorrection);
    s.writeBool(16, m_autoIQCorrection);
    s.writeDouble(17, m_dcCorrection.real());
    s.writeDouble(18, m_dcCorrection.imag());
    s.writeDouble(19, m_iqCorrection.real());
    s.writeDouble(20, m_iqCorrection.imag());
    s.writeBlob(21, serializeArgumentMap(m_streamArgSettings));
    s.writeBlob(22, serializeArgumentMap(m_deviceArgSettings));
    s.writeBool(23, m_useReverseAPI);
    s.writeString(24, m_reverseAPIAddress);
    s.writeU32(25, m_reverseAPIPort);
    s.writeU32(26, m_reverseAPIDeviceIndex);

    return s.final();
}

bool SoapySDROutputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray blob;
        double realval, imagval;
        uint32_t uintval;

        d.readS32(1, &m_devSampleRate, 1024000);
        d.readS32(2, &m_LOppmTenths, 0);
        d.readU32(3, &m_log2Interp, 0);
        d.readBool(4, &m_transverterMode, false);
        d.readS64(5, &m_transverterDeltaFrequency, 0);
        d.readString(6, &m_antenna, "NONE");
        d.readU32(7, &m_bandwidth, 1000000);
        d.readBlob(8, &blob);
        deserializeNamedElementMap(blob, m_tunableElements);
        d.readS32(12, &m_globalGain, 0);
        d.readBlob(13, &blob);
        deserializeNamedElementMap(blob, m_individualGains);
        d.readBool(14, &m_autoGain, false);
        d.readBool(15, &m_autoDCCorrection, false);
        d.readBool(16, &m_autoIQCorrection, false);
        d.readDouble(17, &realval, 0);
        d.readDouble(18, &imagval, 0);
        m_dcCorrection = std::complex<double>{realval, imagval};
        d.readDouble(19, &realval, 0);
        d.readDouble(20, &imagval, 0);
        m_iqCorrection = std::complex<double>{realval, imagval};
        d.readBlob(21, &blob);
        deserializeArgumentMap(blob, m_streamArgSettings);
        d.readBlob(22, &blob);
        deserializeArgumentMap(blob, m_deviceArgSettings);
        d.readBool(23, &m_useReverseAPI, false);
        d.readString(24, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(25, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(26, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QByteArray SoapySDROutputSettings::serializeNamedElementMap(const QMap<QString, double>& map) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << map;
    delete stream;

    return data;
}

void SoapySDROutputSettings::deserializeNamedElementMap(const QByteArray& data, QMap<QString, double>& map)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> map;
    delete stream;
}

QByteArray SoapySDROutputSettings::serializeArgumentMap(const QMap<QString, QVariant>& map) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << map;
    delete stream;

    return data;
}

void SoapySDROutputSettings::deserializeArgumentMap(const QByteArray& data, QMap<QString, QVariant>& map)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> map;
    delete stream;
}

