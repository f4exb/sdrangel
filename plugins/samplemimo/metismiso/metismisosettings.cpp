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

void MetisMISOSettings::applySettings(const QStringList& settingsKeys, const MetisMISOSettings& settings)
{
    if (settingsKeys.contains("nbReceivers")) {
        m_nbReceivers = settings.m_nbReceivers;
    }
    if (settingsKeys.contains("txEnable")) {
        m_txEnable = settings.m_txEnable;
    }
    if (settingsKeys.contains("rx1CenterFrequency")) {
        m_rxCenterFrequencies[0] = settings.m_rxCenterFrequencies[0];
    }
    if (settingsKeys.contains("rx2CenterFrequency")) {
        m_rxCenterFrequencies[1] = settings.m_rxCenterFrequencies[1];
    }
    if (settingsKeys.contains("rx3CenterFrequency")) {
        m_rxCenterFrequencies[2] = settings.m_rxCenterFrequencies[2];
    }
    if (settingsKeys.contains("rx4CenterFrequency")) {
        m_rxCenterFrequencies[3] = settings.m_rxCenterFrequencies[3];
    }
    if (settingsKeys.contains("rx5CenterFrequency")) {
        m_rxCenterFrequencies[4] = settings.m_rxCenterFrequencies[4];
    }
    if (settingsKeys.contains("rx6CenterFrequency")) {
        m_rxCenterFrequencies[5] = settings.m_rxCenterFrequencies[5];
    }
    if (settingsKeys.contains("rx7CenterFrequency")) {
        m_rxCenterFrequencies[6] = settings.m_rxCenterFrequencies[6];
    }
    if (settingsKeys.contains("rx8CenterFrequency")) {
        m_rxCenterFrequencies[7] = settings.m_rxCenterFrequencies[7];
    }
    if (settingsKeys.contains("rx1SubsamplingIndex")) {
        m_rxSubsamplingIndexes[0] = settings.m_rxSubsamplingIndexes[0];
    }
    if (settingsKeys.contains("rx2SubsamplingIndex")) {
        m_rxSubsamplingIndexes[1] = settings.m_rxSubsamplingIndexes[1];
    }
    if (settingsKeys.contains("rx3SubsamplingIndex")) {
        m_rxSubsamplingIndexes[2] = settings.m_rxSubsamplingIndexes[2];
    }
    if (settingsKeys.contains("rx4SubsamplingIndex")) {
        m_rxSubsamplingIndexes[3] = settings.m_rxSubsamplingIndexes[3];
    }
    if (settingsKeys.contains("rx5SubsamplingIndex")) {
        m_rxSubsamplingIndexes[4] = settings.m_rxSubsamplingIndexes[4];
    }
    if (settingsKeys.contains("rx6SubsamplingIndex")) {
        m_rxSubsamplingIndexes[5] = settings.m_rxSubsamplingIndexes[5];
    }
    if (settingsKeys.contains("rx7SubsamplingIndex")) {
        m_rxSubsamplingIndexes[6] = settings.m_rxSubsamplingIndexes[6];
    }
    if (settingsKeys.contains("rx8SubsamplingIndex")) {
        m_rxSubsamplingIndexes[7] = settings.m_rxSubsamplingIndexes[7];
    }
    if (settingsKeys.contains("txCenterFrequency")) {
        m_txCenterFrequency = settings.m_txCenterFrequency;
    }
    if (settingsKeys.contains("rxTransverterMode")) {
        m_rxTransverterMode = settings.m_rxTransverterMode;
    }
    if (settingsKeys.contains("rxTransverterDeltaFrequency")) {
        m_rxTransverterDeltaFrequency = settings.m_rxTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("txTransverterMode")) {
        m_txTransverterMode = settings.m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency")) {
        m_txTransverterDeltaFrequency = settings.m_txTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("iqOrder")) {
        m_iqOrder = settings.m_iqOrder;
    }
    if (settingsKeys.contains("sampleRateIndex")) {
        m_sampleRateIndex = settings.m_sampleRateIndex;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("LOppmTenths")) {
        m_LOppmTenths = settings.m_LOppmTenths;
    }
    if (settingsKeys.contains("preamp")) {
        m_preamp = settings.m_preamp;
    }
    if (settingsKeys.contains("random")) {
        m_random = settings.m_random;
    }
    if (settingsKeys.contains("dither")) {
        m_dither = settings.m_dither;
    }
    if (settingsKeys.contains("duplex")) {
        m_duplex = settings.m_duplex;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("txDrive")) {
        m_txDrive = settings.m_txDrive;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("spectrumStreamIndex")) {
        m_spectrumStreamIndex = settings.m_spectrumStreamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
}

QString MetisMISOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("nbReceivers") || force) {
        ostr << " m_nbReceivers: " << m_nbReceivers;
    }
    if (settingsKeys.contains("txEnable") || force) {
        ostr << " m_txEnable: " << m_txEnable;
    }
    if (settingsKeys.contains("rx1CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[0]: " << m_rxCenterFrequencies[0];
    }
    if (settingsKeys.contains("rx2CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[1]: " << m_rxCenterFrequencies[1];
    }
    if (settingsKeys.contains("rx3CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[2]: " << m_rxCenterFrequencies[2];
    }
    if (settingsKeys.contains("rx4CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[3]: " << m_rxCenterFrequencies[3];
    }
    if (settingsKeys.contains("rx5CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[4]: " << m_rxCenterFrequencies[4];
    }
    if (settingsKeys.contains("rx6CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[5]: " << m_rxCenterFrequencies[5];
    }
    if (settingsKeys.contains("rx7CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[6]: " << m_rxCenterFrequencies[6];
    }
    if (settingsKeys.contains("rx8CenterFrequency") || force) {
        ostr << " m_rxCenterFrequencies[7]: " << m_rxCenterFrequencies[7];
    }
    if (settingsKeys.contains("rx1SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[0]: " << m_rxSubsamplingIndexes[0];
    }
    if (settingsKeys.contains("rx2SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[1]: " << m_rxSubsamplingIndexes[1];
    }
    if (settingsKeys.contains("rx3SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[2]: " << m_rxSubsamplingIndexes[2];
    }
    if (settingsKeys.contains("rx4SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[3]: " << m_rxSubsamplingIndexes[3];
    }
    if (settingsKeys.contains("rx5SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[4]: " << m_rxSubsamplingIndexes[4];
    }
    if (settingsKeys.contains("rx6SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[5]: " << m_rxSubsamplingIndexes[5];
    }
    if (settingsKeys.contains("rx7SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[6]: " << m_rxSubsamplingIndexes[6];
    }
    if (settingsKeys.contains("rx8SubsamplingIndex") || force) {
        ostr << " m_rxSubsamplingIndexes[7]: " << m_rxSubsamplingIndexes[7];
    }
    if (settingsKeys.contains("txCenterFrequency") || force) {
        ostr << " m_txCenterFrequency: " << m_txCenterFrequency;
    }
    if (settingsKeys.contains("rxTransverterMode") || force) {
        ostr << " m_rxTransverterMode: " << m_rxTransverterMode;
    }
    if (settingsKeys.contains("rxTransverterDeltaFrequency") || force) {
        ostr << " m_rxTransverterDeltaFrequency: " << m_rxTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("txTransverterMode") || force) {
        ostr << " m_txTransverterMode: " << m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency") || force) {
        ostr << " m_txTransverterDeltaFrequency: " << m_txTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("iqOrder") || force) {
        ostr << " m_iqOrder: " << m_iqOrder;
    }
    if (settingsKeys.contains("sampleRateIndex") || force) {
        ostr << " m_sampleRateIndex: " << m_sampleRateIndex;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("LOppmTenths") || force) {
        ostr << " m_LOppmTenths: " << m_LOppmTenths;
    }
    if (settingsKeys.contains("preamp") || force) {
        ostr << " m_preamp: " << m_preamp;
    }
    if (settingsKeys.contains("random") || force) {
        ostr << " m_random: " << m_random;
    }
    if (settingsKeys.contains("dither") || force) {
        ostr << " m_dither: " << m_dither;
    }
    if (settingsKeys.contains("duplex") || force) {
        ostr << " m_duplex: " << m_duplex;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("txDrive") || force) {
        ostr << " m_txDrive: " << m_txDrive;
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("spectrumStreamIndex") || force) {
        ostr << " m_spectrumStreamIndex: " << m_spectrumStreamIndex;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }

    return QString(ostr.str().c_str());
}


int MetisMISOSettings::getSampleRateFromIndex(unsigned int index)
{
    if (index < 3) {
        return (1<<index) * 48000;
    } else {
        return 48000;
    }
}
