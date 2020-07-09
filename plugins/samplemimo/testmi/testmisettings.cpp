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

#include <QtGlobal>
#include "util/simpleserializer.h"
#include "testmisettings.h"

TestMIStreamSettings::TestMIStreamSettings()
{
    resetToDefaults();
}

void TestMIStreamSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_frequencyShift = 0;
    m_sampleRate = 768*1000;
    m_log2Decim = 4;
    m_fcPos = FC_POS_CENTER;
    m_sampleSizeIndex = 0;
    m_amplitudeBits = 127;
    m_autoCorrOptions = AutoCorrNone;
    m_modulation = ModulationNone;
    m_modulationTone = 44; // 440 Hz
    m_amModulation = 50; // 50%
    m_fmDeviation = 50; // 5 kHz
    m_dcFactor = 0.0f;
    m_iFactor = 0.0f;
    m_qFactor = 0.0f;
    m_phaseImbalance = 0.0f;
}

TestMISettings::TestMISettings()
{
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_streams.push_back(TestMIStreamSettings());
    m_streams.push_back(TestMIStreamSettings());
}

TestMISettings::TestMISettings(const TestMISettings& other) :
    m_streams(other.m_streams)
{
    m_useReverseAPI = other.m_useReverseAPI;
    m_reverseAPIAddress = other.m_reverseAPIAddress;
    m_reverseAPIPort = other.m_reverseAPIPort;
    m_reverseAPIDeviceIndex = other.m_reverseAPIDeviceIndex;
}

void TestMISettings::resetToDefaults()
{
    for (unsigned int i = 0; i < m_streams.size(); i++) {
        m_streams[i].resetToDefaults();
    }
}

QByteArray TestMISettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeBool(1, m_useReverseAPI);
    s.writeString(2, m_reverseAPIAddress);
    s.writeU32(3, m_reverseAPIPort);
    s.writeU32(4, m_reverseAPIDeviceIndex);

    for (unsigned int i = 0; i < m_streams.size(); i++)
    {
        s.writeS32(10 + 30*i, m_streams[i].m_frequencyShift);
        s.writeU32(11 + 30*i, m_streams[i].m_sampleRate);
        s.writeU32(12 + 30*i, m_streams[i].m_log2Decim);
        s.writeS32(13 + 30*i, (int) m_streams[i].m_fcPos);
        s.writeU32(14 + 30*i, m_streams[i].m_sampleSizeIndex);
        s.writeS32(15 + 30*i, m_streams[i].m_amplitudeBits);
        s.writeS32(16 + 30*i, (int) m_streams[i].m_autoCorrOptions);
        s.writeFloat(17 + 30*i, m_streams[i].m_dcFactor);
        s.writeFloat(18 + 30*i, m_streams[i].m_iFactor);
        s.writeFloat(19 + 30*i, m_streams[i].m_qFactor);
        s.writeFloat(20 + 30*i, m_streams[i].m_phaseImbalance);
        s.writeS32(21 + 30*i, (int) m_streams[i].m_modulation);
        s.writeS32(22 + 30*i, m_streams[i].m_modulationTone);
        s.writeS32(23 + 30*i, m_streams[i].m_amModulation);
        s.writeS32(24 + 30*i, m_streams[i].m_fmDeviation);
    }

    return s.final();
}

bool TestMISettings::deserialize(const QByteArray& data)
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
        uint32_t utmp;

        d.readBool(1, &m_useReverseAPI, false);
        d.readString(2, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(3, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(4, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;

        for (unsigned int i = 0; i < m_streams.size(); i++)
        {
            d.readS32(10 + 30*i, &m_streams[i].m_frequencyShift, 0);
            d.readU32(11 + 30*i, &m_streams[i].m_sampleRate, 768*1000);
            d.readU32(12 + 30*i, &m_streams[i].m_log2Decim, 4);
            d.readS32(13 + 30*i, &intval, 0);
            m_streams[i].m_fcPos = (TestMIStreamSettings::fcPos_t) intval;
            d.readU32(14 + 30*i, &m_streams[i].m_sampleSizeIndex, 0);
            d.readS32(15 + 30*i, &m_streams[i].m_amplitudeBits, 128);
            d.readS32(16 + 30*i, &intval, 0);

            if (intval < 0 || intval > (int) TestMIStreamSettings::AutoCorrLast) {
                m_streams[i].m_autoCorrOptions = TestMIStreamSettings::AutoCorrNone;
            } else {
                m_streams[i].m_autoCorrOptions = (TestMIStreamSettings::AutoCorrOptions) intval;
            }

            d.readFloat(17 + 30*i, &m_streams[i].m_dcFactor, 0.0f);
            d.readFloat(18 + 30*i, &m_streams[i].m_iFactor, 0.0f);
            d.readFloat(19 + 30*i, &m_streams[i].m_qFactor, 0.0f);
            d.readFloat(20 + 30*i, &m_streams[i].m_phaseImbalance, 0.0f);
            d.readS32(21 + 30*i, &intval, 0);

            if (intval < 0 || intval > (int) TestMIStreamSettings::ModulationLast) {
                m_streams[i].m_modulation = TestMIStreamSettings::ModulationNone;
            } else {
                m_streams[i].m_modulation = (TestMIStreamSettings::Modulation) intval;
            }

            d.readS32(22 + 30*i, &m_streams[i].m_modulationTone, 44);
            d.readS32(23 + 30*i, &m_streams[i].m_amModulation, 50);
            d.readS32(24 + 30*i, &m_streams[i].m_fmDeviation, 50);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}






