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

#include <QDataStream>

#include "util/simpleserializer.h"

#include "soapysdrinputsettings.h"

SoapySDRInputSettings::SoapySDRInputSettings()
{
    resetToDefaults();
}

void SoapySDRInputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_LOppmTenths = 0;
    m_devSampleRate = 1024000;
    m_log2Decim = 0;
    m_fcPos = FC_POS_CENTER;
    m_softDCCorrection = false;
    m_softIQCorrection = false;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
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

QByteArray SoapySDRInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(2, m_log2Decim);
    s.writeS32(3, (int) m_fcPos);
    s.writeBool(4, m_softDCCorrection);
    s.writeBool(5, m_softIQCorrection);
    s.writeS32(6, m_LOppmTenths);
    s.writeBool(7, m_transverterMode);
    s.writeS64(8, m_transverterDeltaFrequency);
    s.writeString(9, m_antenna);
    s.writeU32(10, m_bandwidth);
    s.writeBlob(11, serializeNamedElementMap(m_tunableElements));
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
    s.writeBool(27, m_iqOrder);

    return s.final();
}

bool SoapySDRInputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        int intval;
        uint32_t uintval;
        QByteArray blob;
        double realval, imagval;

        d.readS32(1, &m_devSampleRate, 1024000);
        d.readU32(2, &m_log2Decim, 0);
        d.readS32(3, &intval, (int) FC_POS_CENTER);
        m_fcPos = (fcPos_t) intval;
        d.readBool(4, &m_softDCCorrection, false);
        d.readBool(5, &m_softIQCorrection, false);
        d.readS32(6, &m_LOppmTenths, 0);
        d.readBool(7, &m_transverterMode, false);
        d.readS64(8, &m_transverterDeltaFrequency, 0);
        d.readString(9, &m_antenna, "NONE");
        d.readU32(10, &m_bandwidth, 1000000);
        d.readBlob(11, &blob);
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
        d.readBool(27, &m_iqOrder, true);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QByteArray SoapySDRInputSettings::serializeNamedElementMap(const QMap<QString, double>& map) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << map;
    delete stream;

    return data;
}

void SoapySDRInputSettings::deserializeNamedElementMap(const QByteArray& data, QMap<QString, double>& map)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> map;
    delete stream;
}

QByteArray SoapySDRInputSettings::serializeArgumentMap(const QMap<QString, QVariant>& map) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << map;
    delete stream;

    return data;
}

void SoapySDRInputSettings::deserializeArgumentMap(const QByteArray& data, QMap<QString, QVariant>& map)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> map;
    delete stream;
}

