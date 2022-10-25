///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "util/simpleserializer.h"
#include "remotetcpinputsettings.h"

RemoteTCPInputSettings::RemoteTCPInputSettings()
{
    resetToDefaults();
}

void RemoteTCPInputSettings::resetToDefaults()
{
    m_centerFrequency = 435000000;
    m_loPpmCorrection = 0;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_biasTee = false;
    m_directSampling = false;
    m_devSampleRate = 2000000;
    m_log2Decim = 1;
    for (int i = 0; i < m_maxGains; i++) {
        m_gain[i] = 0;
    }
    m_agc = false;
    m_rfBW = 2500000;
    m_inputFrequencyOffset = 0;
    m_channelGain = 0;
    m_channelSampleRate = m_devSampleRate;
    m_channelDecimation = false;
    m_sampleBits = 8;
    m_dataAddress = "127.0.0.1";
    m_dataPort = 1234;
    m_overrideRemoteSettings = true;
    m_preFill = 1.0f;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray RemoteTCPInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_loPpmCorrection);
    s.writeBool(2, m_dcBlock);
    s.writeBool(3, m_iqCorrection);
    s.writeBool(4, m_biasTee);
    s.writeBool(5, m_directSampling);
    s.writeS32(6, m_devSampleRate);
    s.writeS32(7, m_log2Decim);
    s.writeBool(9, m_agc);
    s.writeS32(10, m_rfBW);
    s.writeS32(11, m_inputFrequencyOffset);
    s.writeS32(12, m_channelGain);
    s.writeS32(13, m_channelSampleRate);
    s.writeBool(14, m_channelDecimation);
    s.writeS32(15, m_sampleBits);
    s.writeU32(16, m_dataPort);
    s.writeString(17, m_dataAddress);
    s.writeBool(18, m_overrideRemoteSettings);
    s.writeFloat(19, m_preFill);
    s.writeBool(20, m_useReverseAPI);
    s.writeString(21, m_reverseAPIAddress);
    s.writeU32(22, m_reverseAPIPort);
    s.writeU32(23, m_reverseAPIDeviceIndex);

    for (int i = 0; i < m_maxGains; i++) {
        s.writeS32(30+i, m_gain[i]);
    }

    return s.final();
}

bool RemoteTCPInputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        quint32 uintval;

        d.readS32(1, &m_loPpmCorrection, 0);
        d.readBool(2, &m_dcBlock, false);
        d.readBool(3, &m_iqCorrection, false);
        d.readBool(4, &m_biasTee, false);
        d.readBool(5, &m_directSampling, false);
        d.readS32(6, &m_devSampleRate, 2000000);
        d.readS32(7, &m_log2Decim, 1);
        d.readBool(9, &m_agc, false);
        d.readS32(10, &m_rfBW, 2500000);
        d.readS32(11, &m_inputFrequencyOffset, 0);
        d.readS32(12, &m_channelGain, 0);
        d.readS32(13, &m_channelSampleRate, 2000000);
        d.readBool(14, &m_channelDecimation, false);
        d.readS32(15, &m_sampleBits, 8);
        d.readU32(16, &uintval, 1234);
        m_dataPort = uintval % (1<<16);
        d.readString(17, &m_dataAddress, "127.0.0.1");
        d.readBool(18, &m_overrideRemoteSettings, false);
        d.readFloat(19, &m_preFill, 1.0f);
        d.readBool(20, &m_useReverseAPI, false);
        d.readString(21, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(22, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(23, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        for (int i = 0; i < m_maxGains; i++) {
            d.readS32(30+i, &m_gain[i], 0);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RemoteTCPInputSettings::applySettings(const QStringList& settingsKeys, const RemoteTCPInputSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("loPpmCorrection")) {
        m_loPpmCorrection = settings.m_loPpmCorrection;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("biasTee")) {
        m_biasTee = settings.m_biasTee;
    }
    if (settingsKeys.contains("directSampling")) {
        m_directSampling = settings.m_directSampling;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("agc")) {
        m_agc = settings.m_agc;
    }
    if (settingsKeys.contains("rfBW")) {
        m_rfBW = settings.m_rfBW;
    }
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("channelGain")) {
        m_channelGain = settings.m_channelGain;
    }
    if (settingsKeys.contains("channelSampleRate")) {
        m_channelSampleRate = settings.m_channelSampleRate;
    }
    if (settingsKeys.contains("channelDecimation")) {
        m_channelDecimation = settings.m_channelDecimation;
    }
    if (settingsKeys.contains("sampleBits")) {
        m_sampleBits = settings.m_sampleBits;
    }
    if (settingsKeys.contains("dataAddress")) {
        m_dataAddress = settings.m_dataAddress;
    }
    if (settingsKeys.contains("dataPort")) {
        m_dataPort = settings.m_dataPort;
    }
    if (settingsKeys.contains("overrideRemoteSettings")) {
        m_overrideRemoteSettings = settings.m_overrideRemoteSettings;
    }
    if (settingsKeys.contains("preFill")) {
        m_preFill = settings.m_preFill;
    }
    if (settingsKeys.contains("_useReverseAPI")) {
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

    for (int i = 0; i < m_maxGains; i++)
    {
        if (settingsKeys.contains(QString("gain[%1]").arg(i))) {
            m_gain[i] = settings.m_gain[i];
        }
    }
}

QString RemoteTCPInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("loPpmCorrection") || force) {
        ostr << " m_loPpmCorrection: " << m_loPpmCorrection;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("biasTee") || force) {
        ostr << " m_biasTee: " << m_biasTee;
    }
    if (settingsKeys.contains("directSampling") || force) {
        ostr << " m_directSampling: " << m_directSampling;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("agc") || force) {
        ostr << " m_agc: " << m_agc;
    }
    if (settingsKeys.contains("rfBW") || force) {
        ostr << " m_rfBW: " << m_rfBW;
    }
    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("channelGain") || force) {
        ostr << " m_channelGain: " << m_channelGain;
    }
    if (settingsKeys.contains("channelSampleRate") || force) {
        ostr << " m_channelSampleRate: " << m_channelSampleRate;
    }
    if (settingsKeys.contains("channelDecimation") || force) {
        ostr << " m_channelDecimation: " << m_channelDecimation;
    }
    if (settingsKeys.contains("sampleBits") || force) {
        ostr << " m_sampleBits: " << m_sampleBits;
    }
    if (settingsKeys.contains("dataAddress") || force) {
        ostr << " m_dataAddress: " << m_dataAddress.toStdString();
    }
    if (settingsKeys.contains("dataPort") || force) {
        ostr << " m_dataPort: " << m_dataPort;
    }
    if (settingsKeys.contains("overrideRemoteSettings") || force) {
        ostr << " m_overrideRemoteSettings: " << m_overrideRemoteSettings;
    }
    if (settingsKeys.contains("preFill") || force) {
        ostr << " m_preFill: " << m_preFill;
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

    for (int i = 0; i < m_maxGains; i++)
    {
        if (settingsKeys.contains(QString("gain[%1]").arg(i))) {
            ostr << QString(" gain[%1]: ").arg(i).toStdString() <<  m_gain[i];
        }
    }

    return QString(ostr.str().c_str());
}
