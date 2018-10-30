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

        d.readS32(1, &m_devSampleRate, 1024000);
        d.readU32(2, &m_log2Decim);
        d.readS32(3, &intval, (int) FC_POS_CENTER);
        m_fcPos = (fcPos_t) intval;
        d.readBool(4, &m_dcBlock);
        d.readBool(5, &m_iqCorrection);
        d.readS32(6, &m_LOppmTenths);
        d.readBool(7, &m_transverterMode, false);
        d.readS64(8, &m_transverterDeltaFrequency, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
