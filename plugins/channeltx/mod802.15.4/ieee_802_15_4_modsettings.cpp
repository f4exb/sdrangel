///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2021 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <QColor>
#include <QDebug>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "ieee_802_15_4_modsettings.h"
#include "ieee_802_15_4_macframe.h"

IEEE_802_15_4_ModSettings::IEEE_802_15_4_ModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void IEEE_802_15_4_ModSettings::resetToDefaults()
{
    IEEE_802_15_4_MacFrame macFrame;
    char frame[1024];

    macFrame.toHexCharArray(frame);

    m_inputFrequencyOffset = 0;
    m_modulation = BPSK;
    m_bitRate = 20000;
    m_subGHzBand = true;
    m_rfBandwidth = 2.0f * 300000.0f;
    m_gain = -1.0f; // To avoid overflow, which results in out-of-band RF
    m_channelMute = false;
    m_repeat = false;
    m_repeatDelay = 1.0f;
    m_repeatCount = infinitePackets;
    m_rampUpBits = 0;
    m_rampDownBits = 0;
    m_rampRange = 0;
    m_modulateWhileRamping = true;
    m_lpfTaps = 301;
    m_bbNoise = false;
    m_writeToFile = false;
    m_spectrumRate = m_rfBandwidth;
    m_data = QString(frame);
    m_rgbColor = QColor(255, 0, 0).rgb();
    m_title = "802.15.4 Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_scramble = false;
    m_polynomial = 0x108;
    m_pulseShaping = RC;
    m_beta = 1.0f;
    m_symbolSpan = 6;
    m_udpEnabled = false;
    m_udpBytesFormat = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9998;
    m_workspaceIndex = 0;
    m_hidden = false;
}

bool IEEE_802_15_4_ModSettings::setPHY(QString phy)
{
    float bitRate;
    bool valid;

    // First part of phy string should give bitrate in kbps
    bitRate = phy.split("k")[0].toFloat(&valid) * 1000.0f;

    if (!valid) {
        return false;
    }

    if (phy.contains("BPSK"))
    {
        m_bitRate = bitRate;
        m_subGHzBand = true;
        m_rfBandwidth = 2.0 * bitRate * 15.0;
        m_spectrumRate = m_rfBandwidth;
        m_modulation = IEEE_802_15_4_ModSettings::BPSK;
        m_pulseShaping = RC;
        m_beta = 1.0f;
        m_symbolSpan = 6;
    }
    else if (phy.contains("O-QPSK"))
    {
        m_bitRate = bitRate;
        m_subGHzBand = phy.contains("<1");
        m_rfBandwidth = 2.0 * (bitRate / 4.0) * (m_subGHzBand ? 16.0 : 32.0);
        m_spectrumRate = m_rfBandwidth;
        m_modulation = IEEE_802_15_4_ModSettings::OQPSK;
        if (phy.contains("RC"))
        {
            m_pulseShaping = RC;
            m_beta = 0.8f;
            m_symbolSpan = 6;
        }
        else
            m_pulseShaping = SINE;
    }
    else
        return false;
    return true;
}

QString IEEE_802_15_4_ModSettings::getPHY() const
{
    int decPos = 0;

    if (m_bitRate < 10000) {
        decPos = 1;
    }

    return QString("%1kbps %2").arg(m_bitRate / 1000.0, 0, 'f', decPos).arg(m_modulation == IEEE_802_15_4_ModSettings::BPSK ? "BPSK" : "O-QPSK");
}

int IEEE_802_15_4_ModSettings::getChipRate() const
{
    int chipsPerSymbol, bitsPerSymbol;

    if (m_modulation == BPSK)
    {
        chipsPerSymbol = 15;
        bitsPerSymbol = 1;
    }
    else
    {
        bitsPerSymbol = 4;
        chipsPerSymbol = m_subGHzBand ? 16 : 32;
    }

    return m_bitRate * chipsPerSymbol / bitsPerSymbol;
}

QByteArray IEEE_802_15_4_ModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_bitRate);
    s.writeReal(3, m_rfBandwidth);
    s.writeBool(4, m_subGHzBand);
    s.writeReal(5, m_gain);
    s.writeBool(6, m_channelMute);
    s.writeBool(7, m_repeat);
    s.writeReal(8, m_repeatDelay);
    s.writeS32(9, m_repeatCount);
    s.writeS32(10, m_rampUpBits);
    s.writeS32(11, m_rampDownBits);
    s.writeS32(12, m_rampRange);
    s.writeBool(13, m_modulateWhileRamping);
    s.writeS32(14, m_lpfTaps);
    s.writeBool(15, m_bbNoise);
    s.writeBool(16, m_writeToFile);
    s.writeString(17, m_data);

    s.writeU32(18, m_rgbColor);
    s.writeString(19, m_title);

    if (m_channelMarker) {
        s.writeBlob(20, m_channelMarker->serialize());
    }

    s.writeS32(21, m_streamIndex);
    s.writeBool(22, m_useReverseAPI);
    s.writeString(23, m_reverseAPIAddress);
    s.writeU32(24, m_reverseAPIPort);
    s.writeU32(25, m_reverseAPIDeviceIndex);
    s.writeU32(26, m_reverseAPIChannelIndex);

    s.writeBool(27, m_scramble);
    s.writeS32(28, m_polynomial);
    s.writeS32(29, m_pulseShaping);
    s.writeReal(30, m_beta);
    s.writeS32(31, m_symbolSpan);
    s.writeS32(32, m_spectrumRate);
    s.writeS32(33, m_modulation);
    s.writeBool(34, m_udpEnabled);
    s.writeString(35, m_udpAddress);
    s.writeU32(36, m_udpPort);
    s.writeBool(37, m_udpBytesFormat);

    if (m_rollupState) {
        s.writeBlob(38, m_rollupState->serialize());
    }

    s.writeS32(39, m_workspaceIndex);
    s.writeBlob(40, m_geometryBytes);
    s.writeBool(41, m_hidden);

    return s.final();
}

bool IEEE_802_15_4_ModSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        qint32 tmp;
        uint32_t utmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &m_bitRate, 20000);
        d.readReal(3, &m_rfBandwidth, 2.0f * 300000.0f);
        d.readBool(4, &m_subGHzBand, m_bitRate <= 40000);
        d.readReal(5, &m_gain, 0.0f);
        d.readBool(6, &m_channelMute, false);
        d.readBool(7, &m_repeat, false);
        d.readReal(8, &m_repeatDelay, 1.0f);
        d.readS32(9, &m_repeatCount, -1);
        d.readS32(10, &m_rampUpBits, 8);
        d.readS32(11, &m_rampDownBits, 8);
        d.readS32(12, &m_rampRange, 8);
        d.readBool(13, &m_modulateWhileRamping, true);
        d.readS32(14, &m_lpfTaps, 301);
        d.readBool(15, &m_bbNoise, false);
        d.readBool(16, &m_writeToFile, false);
        d.readString(17, &m_data, "");
        d.readU32(18, &m_rgbColor);
        d.readString(19, &m_title, "802.15.4 Modulator");

        if (m_channelMarker)
        {
            d.readBlob(20, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(21, &m_streamIndex, 0);
        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(25, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(26, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readBool(27, &m_scramble, false);
        d.readS32(28, &m_polynomial, 0x108);
        d.readS32(29, (qint32 *)&m_pulseShaping, RC);
        d.readReal(30, &m_beta, 1.0f);
        d.readS32(31, &m_symbolSpan, 6);
        d.readS32(32, &m_spectrumRate, m_rfBandwidth);
        d.readS32(33, (qint32 *)&m_modulation, m_bitRate < 100000 ? IEEE_802_15_4_ModSettings::BPSK : IEEE_802_15_4_ModSettings::OQPSK);
        d.readBool(34, &m_udpEnabled);
        d.readString(35, &m_udpAddress, "127.0.0.1");
        d.readU32(36, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9998;
        }

        d.readBool(37, &m_udpBytesFormat);

        if (m_rollupState)
        {
            d.readBlob(38, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(39, &m_workspaceIndex, 0);
        d.readBlob(40, &m_geometryBytes);
        d.readBool(41, &m_hidden, false);

        return true;
    }
    else
    {
        qDebug() << "IEEE_802_15_4_ModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

void IEEE_802_15_4_ModSettings::applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_ModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("modulation")) {
        m_modulation = settings.m_modulation;
    }
    if (settingsKeys.contains("bitRate")) {
        m_bitRate = settings.m_bitRate;
    }
    if (settingsKeys.contains("subGHzBand")) {
        m_subGHzBand = settings.m_subGHzBand;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("repeat")) {
        m_repeat = settings.m_repeat;
    }
    if (settingsKeys.contains("repeatDelay")) {
        m_repeatDelay = settings.m_repeatDelay;
    }
    if (settingsKeys.contains("repeatCount")) {
        m_repeatCount = settings.m_repeatCount;
    }
    if (settingsKeys.contains("rampUpBits")) {
        m_rampUpBits = settings.m_rampUpBits;
    }
    if (settingsKeys.contains("rampDownBits")) {
        m_rampDownBits = settings.m_rampDownBits;
    }
    if (settingsKeys.contains("rampRange")) {
        m_rampRange = settings.m_rampRange;
    }
    if (settingsKeys.contains("modulateWhileRamping")) {
        m_modulateWhileRamping = settings.m_modulateWhileRamping;
    }
    if (settingsKeys.contains("lpfTaps")) {
        m_lpfTaps = settings.m_lpfTaps;
    }
    if (settingsKeys.contains("bbNoise")) {
        m_bbNoise = settings.m_bbNoise;
    }
    if (settingsKeys.contains("writeToFile")) {
        m_writeToFile = settings.m_writeToFile;
    }
    if (settingsKeys.contains("spectrumRate")) {
        m_spectrumRate = settings.m_spectrumRate;
    }
    if (settingsKeys.contains("data")) {
        m_data = settings.m_data;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
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
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("scramble")) {
        m_scramble = settings.m_scramble;
    }
    if (settingsKeys.contains("polynomial")) {
        m_polynomial = settings.m_polynomial;
    }
    if (settingsKeys.contains("pulseShaping")) {
        m_pulseShaping = settings.m_pulseShaping;
    }
    if (settingsKeys.contains("beta")) {
        m_beta = settings.m_beta;
    }
    if (settingsKeys.contains("symbolSpan")) {
        m_symbolSpan = settings.m_symbolSpan;
    }
    if (settingsKeys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
    }
    if (settingsKeys.contains("udpBytesFormat")) {
        m_udpBytesFormat = settings.m_udpBytesFormat;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString IEEE_802_15_4_ModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("modulation") || force) {
        ostr << " m_modulation: " << m_modulation;
    }
    if (settingsKeys.contains("bitRate") || force) {
        ostr << " m_bitRate: " << m_bitRate;
    }
    if (settingsKeys.contains("subGHzBand") || force) {
        ostr << " m_subGHzBand: " << m_subGHzBand;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("channelMute") || force) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (settingsKeys.contains("repeat") || force) {
        ostr << " m_repeat: " << m_repeat;
    }
    if (settingsKeys.contains("repeatDelay") || force) {
        ostr << " m_repeatDelay: " << m_repeatDelay;
    }
    if (settingsKeys.contains("repeatCount") || force) {
        ostr << " m_repeatCount: " << m_repeatCount;
    }
    if (settingsKeys.contains("rampUpBits") || force) {
        ostr << " m_rampUpBits: " << m_rampUpBits;
    }
    if (settingsKeys.contains("rampDownBits") || force) {
        ostr << " m_rampDownBits: " << m_rampDownBits;
    }
    if (settingsKeys.contains("rampRange") || force) {
        ostr << " m_rampRange: " << m_rampRange;
    }
    if (settingsKeys.contains("modulateWhileRamping") || force) {
        ostr << " m_modulateWhileRamping: " << m_modulateWhileRamping;
    }
    if (settingsKeys.contains("lpfTaps") || force) {
        ostr << " m_lpfTaps: " << m_lpfTaps;
    }
    if (settingsKeys.contains("bbNoise") || force) {
        ostr << " m_bbNoise: " << m_bbNoise;
    }
    if (settingsKeys.contains("writeToFile") || force) {
        ostr << " m_writeToFile: " << m_writeToFile;
    }
    if (settingsKeys.contains("spectrumRate") || force) {
        ostr << " m_spectrumRate: " << m_spectrumRate;
    }
    if (settingsKeys.contains("data") || force) {
        ostr << " m_data: " << m_data.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
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
    if (settingsKeys.contains("reverseAPIChannelIndex") || force) {
        ostr << " m_reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("scramble") || force) {
        ostr << " m_scramble: " << m_scramble;
    }
    if (settingsKeys.contains("polynomial") || force) {
        ostr << " m_polynomial: " << m_polynomial;
    }
    if (settingsKeys.contains("pulseShaping") || force) {
        ostr << " m_pulseShaping: " << m_pulseShaping;
    }
    if (settingsKeys.contains("beta") || force) {
        ostr << " m_beta: " << m_beta;
    }
    if (settingsKeys.contains("symbolSpan") || force) {
        ostr << " m_symbolSpan: " << m_symbolSpan;
    }
    if (settingsKeys.contains("udpEnabled") || force) {
        ostr << " m_udpEnabled: " << m_udpEnabled;
    }
    if (settingsKeys.contains("udpBytesFormat") || force) {
        ostr << " m_udpBytesFormat: " << m_udpBytesFormat;
    }
    if (settingsKeys.contains("udpAddress") || force) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort") || force) {
        ostr << " m_udpPort: " << m_udpPort;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
