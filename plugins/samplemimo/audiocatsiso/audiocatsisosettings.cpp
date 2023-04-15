///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#include "audiocatsisosettings.h"

AudioCATSISOSettings::AudioCATSISOSettings()
{
    resetToDefaults();
}

void AudioCATSISOSettings::resetToDefaults()
{
    m_rxCenterFrequency = 14200000;
    m_txCenterFrequency = 14200000;
    m_rxDeviceName = "";
    m_rxVolume = 1.0f;
    m_log2Decim = 0;
    m_rxIQMapping = LR;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_fcPosRx = FC_POS_CENTER;
    m_txDeviceName = "";
    m_txVolume = 1.0f;
    m_txIQMapping = LR;
    m_log2Interp = 0;
    m_fcPosTx = FC_POS_CENTER;
    m_streamIndex = 0;
    m_spectrumStreamIndex = 0;
    m_txEnable = false;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

AudioCATSISOSettings::AudioCATSISOSettings(const AudioCATSISOSettings& other)
{
    m_rxCenterFrequency = other.m_rxCenterFrequency;
    m_txCenterFrequency = other.m_txCenterFrequency;
    m_rxDeviceName = other.m_rxDeviceName;
    m_rxVolume = other.m_rxVolume;
    m_log2Decim = other.m_log2Decim;
    m_rxIQMapping = other.m_rxIQMapping;
    m_dcBlock = other.m_dcBlock;
    m_iqCorrection = other.m_iqCorrection;
    m_fcPosRx = other.m_fcPosRx;
    m_txDeviceName = other.m_txDeviceName;
    m_txVolume = other.m_txVolume;
    m_txIQMapping = other.m_txIQMapping;
    m_log2Interp = other.m_log2Interp;
    m_fcPosTx = other.m_fcPosTx;
    m_streamIndex = other.m_streamIndex;
    m_spectrumStreamIndex = other.m_spectrumStreamIndex;
    m_txEnable = other.m_txEnable;
    m_useReverseAPI = other.m_useReverseAPI;
    m_reverseAPIAddress = other.m_reverseAPIAddress;
    m_reverseAPIPort = other.m_reverseAPIPort;
    m_reverseAPIDeviceIndex = other.m_reverseAPIDeviceIndex;
}

QByteArray AudioCATSISOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_rxDeviceName);
    s.writeU64(2, m_rxCenterFrequency);
    s.writeFloat(3, m_rxVolume);
    s.writeU32(4, m_log2Decim);
    s.writeS32(5, (int)m_rxIQMapping);
    s.writeBool(6, m_dcBlock);
    s.writeBool(7, m_iqCorrection);
    s.writeS32(8, (int) m_fcPosRx);

    s.writeString(21, m_txDeviceName);
    s.writeU64(22, m_rxCenterFrequency);
    s.writeFloat(23, m_txVolume);
    s.writeS32(24, (int)m_txIQMapping);
    s.writeU32(25, m_log2Decim);
    s.writeS32(26, (int) m_fcPosTx);

    s.writeBool(51, m_useReverseAPI);
    s.writeString(52, m_reverseAPIAddress);
    s.writeU32(53, m_reverseAPIPort);
    s.writeU32(54, m_reverseAPIDeviceIndex);
    s.writeS32(55, m_streamIndex);
    s.writeS32(56, m_spectrumStreamIndex);
    s.writeBool(57, m_txEnable);

    return s.final();
}

bool AudioCATSISOSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        uint32_t uintval;
        int intval;

        d.readString(1, &m_rxDeviceName, "");
        d.readU64(2, &m_rxCenterFrequency, 14200000);
        d.readFloat(3, &m_rxVolume, 1.0f);
        d.readU32(4, &m_log2Decim, 0);
        d.readS32(5, (int *)&m_rxIQMapping, IQMapping::L);
        d.readBool(6, &m_dcBlock, false);
        d.readBool(7, &m_iqCorrection, false);
        d.readS32(8, &intval, 2);
        m_fcPosRx = (fcPos_t) intval;

        d.readString(21, &m_txDeviceName, "");
        d.readU64(22, &m_txCenterFrequency, 14200000);
        d.readFloat(23, &m_txVolume, 1.0f);
        d.readS32(24,(int *)&m_txIQMapping, IQMapping::LR);
        d.readU32(25, &m_log2Interp, 0);
        d.readS32(26, &intval, 2);
        m_fcPosTx = (fcPos_t) intval;

        d.readBool(51, &m_useReverseAPI, false);
        d.readString(52, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(53, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(54, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        d.readS32(55, &m_streamIndex, 0);
        d.readS32(56, &m_spectrumStreamIndex, 0);
        d.readBool(57, &m_txEnable, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AudioCATSISOSettings::applySettings(const QStringList& settingsKeys, const AudioCATSISOSettings& settings)
{
    if (settingsKeys.contains("rxDeviceName")) {
        m_rxDeviceName = settings.m_rxDeviceName;
    }
    if (settingsKeys.contains("rxCenterFrequency")) {
        m_rxCenterFrequency = settings.m_rxCenterFrequency;
    }
    if (settingsKeys.contains("rxVolume")) {
        m_rxVolume = settings.m_rxVolume;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("rxIQMapping")) {
        m_rxIQMapping = settings.m_rxIQMapping;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("fcPosRx")) {
        m_fcPosRx = settings.m_fcPosRx;
    }

    if (settingsKeys.contains("txDeviceName")) {
        m_txDeviceName = settings.m_txDeviceName;
    }
    if (settingsKeys.contains("txCenterFrequency")) {
        m_txCenterFrequency = settings.m_txCenterFrequency;
    }
    if (settingsKeys.contains("txVolume")) {
        m_txVolume = settings.m_txVolume;
    }
    if (settingsKeys.contains("txIQMapping")) {
        m_txIQMapping = settings.m_txIQMapping;
    }
    if (settingsKeys.contains("log2Interp")) {
        m_log2Interp = settings.m_log2Interp;
    }
    if (settingsKeys.contains("fcPosTx")) {
        m_fcPosTx = settings.m_fcPosTx;
    }

    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("spectrumStreamIndex")) {
        m_spectrumStreamIndex = settings.m_spectrumStreamIndex;
    }
    if (settingsKeys.contains("txEnable")) {
        m_txEnable = settings.m_txEnable;
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

QString AudioCATSISOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("rxDeviceName") || force) {
        ostr << " m_rxDeviceName: " << m_rxDeviceName.toStdString();
    }
    if (settingsKeys.contains("rxCenterFrequency") || force) {
        ostr << " m_rxCenterFrequency: " << m_rxCenterFrequency;
    }
    if (settingsKeys.contains("rxVolume") || force) {
        ostr << " m_rxVolume: " << m_rxVolume;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("rxIQMapping") || force) {
        ostr << " m_rxIQMapping: " << m_rxIQMapping;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("fcPosRx") || force) {
        ostr << " m_fcPosRx: " << m_fcPosRx;
    }

    if (settingsKeys.contains("txDeviceName") || force) {
        ostr << " m_txDeviceName: " << m_txDeviceName.toStdString();
    }
    if (settingsKeys.contains("txVolume") || force) {
        ostr << " m_txVolume: " << m_txVolume;
    }
    if (settingsKeys.contains("txIQMapping") || force) {
        ostr << " m_txIQMapping: " << m_txIQMapping;
    }
    if (settingsKeys.contains("log2Interp") || force) {
        ostr << " m_log2Interp: " << m_log2Interp;
    }
    if (settingsKeys.contains("fcPosTx") || force) {
        ostr << " m_fcPosTx: " << m_fcPosTx;
    }

    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("spectrumStreamIndex") || force) {
        ostr << " m_spectrumStreamIndex: " << m_spectrumStreamIndex;
    }
    if (settingsKeys.contains("txEnable") || force) {
        ostr << " m_txEnable: " << m_txEnable;
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
