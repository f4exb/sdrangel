///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
    m_dcBlock = false;
    m_iqCorrection = false;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_fileRecordName = "";
    m_antenna = "NONE";
    m_bandwidth = 1000000;
}

QByteArray SoapySDRInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(2, m_log2Decim);
    s.writeS32(3, (int) m_fcPos);
    s.writeBool(4, m_dcBlock);
    s.writeBool(5, m_iqCorrection);
    s.writeS32(6, m_LOppmTenths);
    s.writeBool(7, m_transverterMode);
    s.writeS64(8, m_transverterDeltaFrequency);
    s.writeString(9, m_antenna);
    s.writeU32(10, m_bandwidth);
    s.writeBlob(11, serializeNamedElementMap(m_tunableElements));

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
        QByteArray blob;

        d.readS32(1, &m_devSampleRate, 1024000);
        d.readU32(2, &m_log2Decim);
        d.readS32(3, &intval, (int) FC_POS_CENTER);
        m_fcPos = (fcPos_t) intval;
        d.readBool(4, &m_dcBlock);
        d.readBool(5, &m_iqCorrection);
        d.readS32(6, &m_LOppmTenths);
        d.readBool(7, &m_transverterMode, false);
        d.readS64(8, &m_transverterDeltaFrequency, 0);
        d.readString(9, &m_antenna, "NONE");
        d.readU32(10, &m_bandwidth, 1000000);
        d.readBlob(11, &blob);
        deserializeNamedElementMap(blob, m_tunableElements);

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
