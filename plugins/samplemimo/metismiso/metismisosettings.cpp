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
    m_txEnable = other.m_txEnable;
    std::copy(other.m_rxCenterFrequencies, other.m_rxCenterFrequencies + m_maxReceivers, m_rxCenterFrequencies);
    std::copy(other.m_rxSubsamplingIndexes, other.m_rxSubsamplingIndexes + m_maxReceivers, m_rxSubsamplingIndexes);
    m_txCenterFrequency = other.m_txCenterFrequency;
    m_rxTransverterMode = other.m_rxTransverterMode;
    m_rxTransverterDeltaFrequency = other.m_rxTransverterDeltaFrequency;
    m_txTransverterMode = other.m_txTransverterMode;
    m_txTransverterDeltaFrequency = other.m_txTransverterDeltaFrequency;
    m_iqOrder = other.m_iqOrder;
    m_sampleRateIndex = other.m_sampleRateIndex;
    m_log2Decim = other.m_log2Decim;
    m_LOppmTenths = other.m_LOppmTenths;
    m_preamp = other.m_preamp;
    m_random = other.m_random;
    m_dither = other.m_dither;
    m_duplex = other.m_duplex;
    m_dcBlock = other.m_dcBlock;
    m_iqCorrection = other.m_iqCorrection;
    m_txDrive = other.m_txDrive;
    m_streamIndex = other.m_streamIndex;
    m_spectrumStreamIndex = other.m_spectrumStreamIndex;
    m_useReverseAPI = other.m_useReverseAPI;
    m_reverseAPIAddress = other.m_reverseAPIAddress;
    m_reverseAPIPort = other.m_reverseAPIPort;
    m_reverseAPIDeviceIndex = other.m_reverseAPIDeviceIndex;
}

void MetisMISOSettings::resetToDefaults()
{
    m_nbReceivers = 1;
    m_txEnable = false;
    std::fill(m_rxCenterFrequencies, m_rxCenterFrequencies + m_maxReceivers, 7074000);
    std::fill(m_rxSubsamplingIndexes, m_rxSubsamplingIndexes + m_maxReceivers, 0);
    m_txCenterFrequency = 7074000;
    m_rxTransverterMode = false;
    m_rxTransverterDeltaFrequency = 0;
    m_txTransverterMode = false;
    m_txTransverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_sampleRateIndex = 0; // 48000 kS/s
    m_log2Decim = 0;
    m_LOppmTenths = 0;
    m_preamp = false;
    m_random = false;
    m_dither = false;
    m_duplex = false;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_txDrive = 15;
    m_streamIndex = 0;
    m_spectrumStreamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray MetisMISOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU32(1, m_nbReceivers);
    s.writeBool(2, m_txEnable);
    s.writeU64(3, m_txCenterFrequency);
    s.writeBool(4, m_rxTransverterMode);
    s.writeS64(5, m_rxTransverterDeltaFrequency);
    s.writeBool(6, m_txTransverterMode);
    s.writeS64(7, m_txTransverterDeltaFrequency);
    s.writeBool(8, m_iqOrder);
    s.writeU32(9, m_sampleRateIndex);
    s.writeU32(10, m_log2Decim);
    s.writeS32(11, m_LOppmTenths);
    s.writeBool(12, m_preamp);
    s.writeBool(13, m_random);
    s.writeBool(14, m_dither);
    s.writeBool(15, m_duplex);
    s.writeBool(16, m_dcBlock);
    s.writeBool(17, m_iqCorrection);
    s.writeU32(18, m_txDrive);
    s.writeBool(19, m_useReverseAPI);
    s.writeString(20, m_reverseAPIAddress);
    s.writeU32(21, m_reverseAPIPort);
    s.writeU32(22, m_reverseAPIDeviceIndex);
    s.writeS32(23, m_streamIndex);
    s.writeS32(24, m_spectrumStreamIndex);

    for (int i = 0; i < m_maxReceivers; i++)
    {
        s.writeU64(30+i, m_rxCenterFrequencies[i]);
        s.writeU32(50+i, m_rxSubsamplingIndexes[i]);
    }

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
        uint32_t utmp;

        d.readU32(1, &m_nbReceivers, 1);
        d.readBool(2, &m_txEnable, false);
        d.readU64(3, &m_txCenterFrequency, 7074000);
        d.readBool(4, &m_rxTransverterMode, false);
        d.readS64(5, &m_rxTransverterDeltaFrequency, 0);
        d.readBool(6, &m_txTransverterMode, false);
        d.readS64(7, &m_txTransverterDeltaFrequency, 0);
        d.readBool(8, &m_iqOrder, true);
        d.readU32(9, &m_sampleRateIndex, 0);
        d.readU32(10, &m_log2Decim, 0);
        d.readS32(11, &m_LOppmTenths, 0);
        d.readBool(12, &m_preamp, false);
        d.readBool(13, &m_random, false);
        d.readBool(14, &m_dither, false);
        d.readBool(15, &m_duplex, false);
        d.readBool(16, &m_dcBlock, false);
        d.readBool(17, &m_iqCorrection, false);
        d.readU32(18, &m_txDrive, 15);
        d.readBool(19, &m_useReverseAPI, false);
        d.readString(20, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(21, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(22, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;

        for (int i = 0; i < m_maxReceivers; i++)
        {
            d.readU64(30+i, &m_rxCenterFrequencies[i], 7074000);
            d.readU32(50+i, &m_rxSubsamplingIndexes[i], 0);
        }

        d.readS32(23, &m_streamIndex, 0);
        d.readS32(24, &m_spectrumStreamIndex, 0);

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
