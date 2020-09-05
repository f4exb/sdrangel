///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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
#include "metismisosettings.h"

MetisMISOSettings::MetisMISOSettings()
{
    resetToDefaults();
}

MetisMISOSettings::MetisMISOSettings(const MetisMISOSettings& other)
{
    m_nbReceivers = other.m_nbReceivers;
    m_rx1CenterFrequency = other.m_rx1CenterFrequency;
    m_rx2CenterFrequency = other.m_rx2CenterFrequency;
    m_rx3CenterFrequency = other.m_rx3CenterFrequency;
    m_rx4CenterFrequency = other.m_rx4CenterFrequency;
    m_txCenterFrequency = other.m_txCenterFrequency;
    m_sampleRateIndex = other.m_sampleRateIndex;
    m_log2Decim = other.m_log2Decim;
    m_preamp = other.m_preamp;
    m_random = other.m_random;
    m_dither = other.m_dither;
    m_duplex = other.m_duplex;
    m_dcBlock = other.m_dcBlock;
    m_iqCorrection = other.m_iqCorrection;
    m_useReverseAPI = other.m_useReverseAPI;
    m_reverseAPIAddress = other.m_reverseAPIAddress;
    m_reverseAPIPort = other.m_reverseAPIPort;
    m_reverseAPIDeviceIndex = other.m_reverseAPIDeviceIndex;
}

void MetisMISOSettings::resetToDefaults()
{
    m_nbReceivers = 1;
    m_rx1CenterFrequency = 7074000;
    m_rx2CenterFrequency = 7074000;
    m_rx3CenterFrequency = 7074000;
    m_rx4CenterFrequency = 7074000;
    m_txCenterFrequency = 7074000;
    m_sampleRateIndex = 0; // 48000 kS/s
    m_log2Decim = 0;
    m_preamp = false;
    m_random = false;
    m_dither = false;
    m_duplex = false;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray MetisMISOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU32(1, m_nbReceivers);
    s.writeU64(2, m_rx1CenterFrequency);
    s.writeU64(3, m_rx2CenterFrequency);
    s.writeU64(4, m_rx3CenterFrequency);
    s.writeU64(5, m_rx4CenterFrequency);
    s.writeU64(6, m_txCenterFrequency);
    s.writeU32(7, m_sampleRateIndex);
    s.writeU32(8, m_log2Decim);
    s.writeBool(9, m_preamp);
    s.writeBool(10, m_random);
    s.writeBool(11, m_dither);
    s.writeBool(12, m_duplex);
    s.writeBool(13, m_dcBlock);
    s.writeBool(14, m_iqCorrection);
    s.writeBool(15, m_useReverseAPI);
    s.writeString(16, m_reverseAPIAddress);
    s.writeU32(17, m_reverseAPIPort);
    s.writeU32(18, m_reverseAPIDeviceIndex);

    return s.final();
}

bool MetisMISOSettings::deserialize(const QByteArray& data)
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

        d.readU32(1, &m_nbReceivers, 1);
        d.readU64(2, &m_rx1CenterFrequency, 7074000);
        d.readU64(3, &m_rx2CenterFrequency, 7074000);
        d.readU64(4, &m_rx3CenterFrequency, 7074000);
        d.readU64(5, &m_rx4CenterFrequency, 7074000);
        d.readU64(6, &m_txCenterFrequency, 7074000);
        d.readU32(7, &m_sampleRateIndex, 0);
        d.readU32(8, &m_log2Decim, 0);
        d.readBool(8, &m_preamp, false);
        d.readBool(9, &m_random, false);
        d.readBool(10, &m_dither, false);
        d.readBool(11, &m_duplex, false);
        d.readBool(12, &m_dcBlock, false);
        d.readBool(13, &m_iqCorrection, false);
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(17, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int MetisMISOSettings::getSampleRateFromIndex(unsigned int index)
{
    if (index < 3) {
        return (1<<index) * 48000;
    } else {
        return 48000;
    }
}