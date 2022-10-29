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
#include "util/simpleserializer.h"
#include "testsourcesettings.h"

TestSourceSettings::TestSourceSettings()
{
    resetToDefaults();
}

void TestSourceSettings::resetToDefaults()
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
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray TestSourceSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(2, m_frequencyShift);
    s.writeU32(3, m_sampleRate);
    s.writeU32(4, m_log2Decim);
    s.writeS32(5, (int) m_fcPos);
    s.writeU32(6, m_sampleSizeIndex);
    s.writeS32(7, m_amplitudeBits);
    s.writeS32(8, (int) m_autoCorrOptions);
    s.writeFloat(10, m_dcFactor);
    s.writeFloat(11, m_iFactor);
    s.writeFloat(12, m_qFactor);
    s.writeFloat(13, m_phaseImbalance);
    s.writeS32(14, (int) m_modulation);
    s.writeS32(15, m_modulationTone);
    s.writeS32(16, m_amModulation);
    s.writeS32(17, m_fmDeviation);
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIDeviceIndex);

    return s.final();
}

bool TestSourceSettings::deserialize(const QByteArray& data)
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

        d.readS32(2, &m_frequencyShift, 0);
        d.readU32(3, &m_sampleRate, 768*1000);
        d.readU32(4, &m_log2Decim, 4);
        d.readS32(5, &intval, 0);
        m_fcPos = (fcPos_t) intval;
        d.readU32(6, &m_sampleSizeIndex, 0);
        d.readS32(7, &m_amplitudeBits, 128);
        d.readS32(8, &intval, 0);

        if (intval < 0 || intval > (int) AutoCorrLast) {
            m_autoCorrOptions = AutoCorrNone;
        } else {
            m_autoCorrOptions = (AutoCorrOptions) intval;
        }

        d.readFloat(10, &m_dcFactor, 0.0f);
        d.readFloat(11, &m_iFactor, 0.0f);
        d.readFloat(12, &m_qFactor, 0.0f);
        d.readFloat(13, &m_phaseImbalance, 0.0f);
        d.readS32(14, &intval, 0);

        if (intval < 0 || intval > (int) ModulationLast) {
            m_modulation = ModulationNone;
        } else {
            m_modulation = (Modulation) intval;
        }

        d.readS32(15, &m_modulationTone, 44);
        d.readS32(16, &m_amModulation, 50);
        d.readS32(17, &m_fmDeviation, 50);

        d.readBool(18, &m_useReverseAPI, false);
        d.readString(19, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(20, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(21, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void TestSourceSettings::applySettings(const QStringList& settingsKeys, const TestSourceSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("frequencyShift")) {
        m_frequencyShift = settings.m_frequencyShift;
    }
    if (settingsKeys.contains("sampleRate")) {
        m_sampleRate = settings.m_sampleRate;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("fcPos")) {
        m_fcPos = settings.m_fcPos;
    }
    if (settingsKeys.contains("sampleSizeIndex")) {
        m_sampleSizeIndex = settings.m_sampleSizeIndex;
    }
    if (settingsKeys.contains("amplitudeBits")) {
        m_amplitudeBits = settings.m_amplitudeBits;
    }
    if (settingsKeys.contains("autoCorrOptions")) {
        m_autoCorrOptions = settings.m_autoCorrOptions;
    }
    if (settingsKeys.contains("modulation")) {
        m_modulation = settings.m_modulation;
    }
    if (settingsKeys.contains("modulationTone")) {
        m_modulationTone = settings.m_modulationTone;
    }
    if (settingsKeys.contains("amModulation")) {
        m_amModulation = settings.m_amModulation;
    }
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (settingsKeys.contains("dcFactor")) {
        m_dcFactor = settings.m_dcFactor;
    }
    if (settingsKeys.contains("iFactor")) {
        m_iFactor = settings.m_iFactor;
    }
    if (settingsKeys.contains("qFactor")) {
        m_qFactor = settings.m_qFactor;
    }
    if (settingsKeys.contains("phaseImbalance")) {
        m_phaseImbalance = settings.m_phaseImbalance;
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

QString TestSourceSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("frequencyShift") || force) {
        ostr << " m_frequencyShift: " << m_frequencyShift;
    }
    if (settingsKeys.contains("sampleRate") || force) {
        ostr << " m_sampleRate: " << m_sampleRate;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("fcPos") || force) {
        ostr << " m_fcPos: " << m_fcPos;
    }
    if (settingsKeys.contains("sampleSizeIndex") || force) {
        ostr << " m_sampleSizeIndex: " << m_sampleSizeIndex;
    }
    if (settingsKeys.contains("amplitudeBits") || force) {
        ostr << " m_amplitudeBits: " << m_amplitudeBits;
    }
    if (settingsKeys.contains("autoCorrOptions") || force) {
        ostr << " m_autoCorrOptions: " << m_autoCorrOptions;
    }
    if (settingsKeys.contains("modulation") || force) {
        ostr << " m_modulation: " << m_modulation;
    }
    if (settingsKeys.contains("modulationTone") || force) {
        ostr << " m_modulationTone: " << m_modulationTone;
    }
    if (settingsKeys.contains("amModulation") || force) {
        ostr << " m_amModulation: " << m_amModulation;
    }
    if (settingsKeys.contains("fmDeviation") || force) {
        ostr << " m_fmDeviation: " << m_fmDeviation;
    }
    if (settingsKeys.contains("dcFactor") || force) {
        ostr << " m_dcFactor: " << m_dcFactor;
    }
    if (settingsKeys.contains("iFactor") || force) {
        ostr << " m_iFactor: " << m_iFactor;
    }
    if (settingsKeys.contains("qFactor") || force) {
        ostr << " m_qFactor: " << m_qFactor;
    }
    if (settingsKeys.contains("phaseImbalance") || force) {
        ostr << " m_phaseImbalance: " << m_phaseImbalance;
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
