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
#include "packetmodsettings.h"

PacketModSettings::PacketModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void PacketModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_modulation = PacketModSettings::AFSK;
    m_baud = 1200;
    m_rfBandwidth = 12500.0f;
    m_fmDeviation = 2500.0f;
    m_gain = -2.0f; // To avoid overflow, which results in out-of-band RF
    m_channelMute = false;
    m_repeat = false;
    m_repeatDelay = 1.0f;
    m_repeatCount = infinitePackets;
    m_rampUpBits = 8;
    m_rampDownBits = 8;
    m_rampRange = 60;
    m_modulateWhileRamping = true;
    m_markFrequency = 2200;
    m_spaceFrequency = 1200;
    m_ax25PreFlags = 5;
    m_ax25PostFlags = 4; // Extra seemingly needed for 9600.
    m_ax25Control = 3;
    m_ax25PID = 0xf0;
    m_preEmphasis = false;
    m_preEmphasisTau = 531e-6f; // Narrowband FM
    m_preEmphasisHighFreq = 3000.0f;
    m_lpfTaps = 301;
    m_bbNoise = false;
    m_rfNoise = false;
    m_writeToFile = false;
    m_spectrumRate = 8000;
    m_callsign = "MYCALL";
    m_to = "APRS";
    m_via = "WIDE2-2";
    m_data = ">Using SDRangel";
    m_rgbColor = QColor(0, 105, 2).rgb();
    m_title = "Packet Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_bpf = false;
    m_bpfLowCutoff = m_spaceFrequency - 400.0f;
    m_bpfHighCutoff = m_markFrequency + 400.0f;
    m_bpfTaps = 301;
    m_scramble = false;
    m_polynomial = 0x10800;
    m_pulseShaping = true;
    m_beta = 0.5f;
    m_symbolSpan = 6;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9998;
    m_workspaceIndex = 0;
    m_hidden = false;
}

bool PacketModSettings::setMode(QString mode)
{
    int baud;
    bool valid;
    bool setCommon = false;

    // First part of mode string should give baud rate
    baud = mode.split(" ")[0].toInt(&valid);

    if (!valid) {
        return false;
    }

    if (mode.endsWith("AFSK"))
    {
        // UK channels - https://rsgb.org/main/blog/news/gb2rs/headlines/2015/12/04/check-your-aprs-deviation/
        m_baud = baud;
        m_scramble = false;
        m_rfBandwidth = 12500.0f;
        m_fmDeviation = 2500.0f;
        m_modulation = PacketModSettings::AFSK;
        m_markFrequency = 2200;
        m_spaceFrequency = 1200;
        setCommon = true;
    }
    else if (mode.endsWith("FSK"))
    {
        // G3RUH - http://www.jrmiller.demon.co.uk/products/figs/man9k6.pdf
        m_baud = baud;
        m_scramble = true;
        m_polynomial = 0x10800;
        m_rfBandwidth = 20000.0f;
        m_fmDeviation = 3000.0f;
        m_modulation = PacketModSettings::FSK;
        m_beta = 0.5f;
        m_symbolSpan = 6;
        setCommon = true;
    }
    else
    {
        return false;
    }

    if (baud <= 2400) {
        m_spectrumRate = 8000;
    } else {
        m_spectrumRate = 24000;
    }

    if (setCommon)
    {
        m_ax25PreFlags = 5;
        m_ax25PostFlags = 4;
        m_ax25PID = 0xf0;
        m_ax25Control = 3;
        m_preEmphasis = false;
        m_preEmphasisTau = 531e-6f; // Narrowband FM
        m_preEmphasisHighFreq = 3000.0f;
        m_lpfTaps = 301;
        m_rampUpBits = 8;
        m_rampDownBits = 8;
        m_rampRange = 60;
        m_modulateWhileRamping = true;
        m_bpf = false;
        m_bpfLowCutoff = m_spaceFrequency - 400.0f;
        m_bpfHighCutoff = m_markFrequency + 400.0f;
        m_bpfTaps = 301;
        m_pulseShaping = true;
    }

    return true;
}

QString PacketModSettings::getMode() const
{
    return QString("%1 %2").arg(m_baud).arg(m_modulation == PacketModSettings::AFSK ? "AFSK" : "FSK");
}

QByteArray PacketModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_baud);
    s.writeReal(3, m_rfBandwidth);
    s.writeReal(4, m_fmDeviation);
    s.writeReal(5, m_gain);
    s.writeBool(6, m_channelMute);
    s.writeBool(7, m_repeat);
    s.writeReal(8, m_repeatDelay);
    s.writeS32(9, m_repeatCount);
    s.writeS32(10, m_rampUpBits);
    s.writeS32(11, m_rampDownBits);
    s.writeS32(12, m_rampRange);
    s.writeBool(13, m_modulateWhileRamping);
    s.writeS32(14, m_markFrequency);
    s.writeS32(15, m_spaceFrequency);
    s.writeS32(16, m_ax25PreFlags);
    s.writeS32(17, m_ax25PostFlags);
    s.writeS32(18, m_ax25Control);
    s.writeS32(19, m_ax25PID);
    s.writeBool(20, m_preEmphasis);
    s.writeReal(21, m_preEmphasisTau);
    s.writeReal(22, m_preEmphasisHighFreq);
    s.writeS32(23, m_lpfTaps);
    s.writeBool(24, m_bbNoise);
    s.writeBool(25, m_rfNoise);
    s.writeBool(26, m_writeToFile);
    s.writeString(27, m_callsign);
    s.writeString(28, m_to);
    s.writeString(29, m_via);
    s.writeString(30, m_data);
    s.writeU32(31, m_rgbColor);
    s.writeString(32, m_title);

    if (m_channelMarker) {
        s.writeBlob(33, m_channelMarker->serialize());
    }

    s.writeS32(34, m_streamIndex);
    s.writeBool(35, m_useReverseAPI);
    s.writeString(36, m_reverseAPIAddress);
    s.writeU32(37, m_reverseAPIPort);
    s.writeU32(38, m_reverseAPIDeviceIndex);
    s.writeU32(39, m_reverseAPIChannelIndex);
    s.writeBool(40, m_bpf);
    s.writeReal(41, m_bpfLowCutoff);
    s.writeReal(42, m_bpfHighCutoff);
    s.writeS32(43, m_bpfTaps);
    s.writeBool(44, m_scramble);
    s.writeS32(45, m_polynomial);
    s.writeBool(46, m_pulseShaping);
    s.writeReal(47, m_beta);
    s.writeS32(48, m_symbolSpan);
    s.writeS32(49, m_spectrumRate);
    s.writeS32(50, m_modulation);
    s.writeBool(51, m_udpEnabled);
    s.writeString(52, m_udpAddress);
    s.writeU32(53, m_udpPort);

    if (m_rollupState) {
        s.writeBlob(54, m_rollupState->serialize());
    }

    s.writeS32(55, m_workspaceIndex);
    s.writeBlob(56, m_geometryBytes);
    s.writeBool(57, m_hidden);

    return s.final();
}

bool PacketModSettings::deserialize(const QByteArray& data)
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
        d.readS32(2, &m_baud, 1200);
        d.readReal(3, &m_rfBandwidth, 12500.0f);
        d.readReal(4, &m_fmDeviation, 2500.0f);
        d.readReal(5, &m_gain, 0.0f);
        d.readBool(6, &m_channelMute, false);
        d.readBool(7, &m_repeat, false);
        d.readReal(8, &m_repeatDelay, 1.0f);
        d.readS32(9, &m_repeatCount, -1);
        d.readS32(10, &m_rampUpBits, 8);
        d.readS32(11, &m_rampDownBits, 8);
        d.readS32(12, &m_rampRange, 8);
        d.readBool(13, &m_modulateWhileRamping, true);
        d.readS32(14, &m_markFrequency, 5);
        d.readS32(15, &m_spaceFrequency, 5);
        d.readS32(16, &m_ax25PreFlags, 5);
        d.readS32(17, &m_ax25PostFlags, 4);
        d.readS32(18, &m_ax25Control, 3);
        d.readS32(19, &m_ax25PID, 0xf0);
        d.readBool(20, &m_preEmphasis, false);
        d.readReal(21, &m_preEmphasisTau, 531e-6f);
        d.readReal(22, &m_preEmphasisHighFreq, 3000.0f);
        d.readS32(23, &m_lpfTaps, 301);
        d.readBool(24, &m_bbNoise, false);
        d.readBool(25, &m_rfNoise, false);
        d.readBool(26, &m_writeToFile, false);
        d.readString(27, &m_callsign, "MYCALL");
        d.readString(28, &m_to, "APRS");
        d.readString(29, &m_via, "WIDE2-2");
        d.readString(30, &m_data, ">Using SDRangel");
        d.readU32(31, &m_rgbColor);
        d.readString(32, &m_title, "Packet Modulator");

        if (m_channelMarker)
        {
            d.readBlob(33, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(34, &m_streamIndex, 0);
        d.readBool(35, &m_useReverseAPI, false);
        d.readString(36, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(37, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(38, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(39, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readBool(40, &m_bpf, false);
        d.readReal(41, &m_bpfLowCutoff, 1200.0 - 400.0f);
        d.readReal(42, &m_bpfHighCutoff, 2200.0 + 400.0f);
        d.readS32(43, &m_bpfTaps, 301);
        d.readBool(44, &m_scramble, m_baud == 9600);
        d.readS32(45, &m_polynomial, 0x10800);
        d.readBool(46, &m_pulseShaping, true);
        d.readReal(47, &m_beta, 0.5f);
        d.readS32(48, &m_symbolSpan, 6);
        d.readS32(49, &m_spectrumRate, m_baud == 1200 ? 8000 : 24000);
        d.readS32(50, (qint32 *)&m_modulation, m_baud == 1200 ? PacketModSettings::AFSK : PacketModSettings::FSK);
        d.readBool(51, &m_udpEnabled);
        d.readString(52, &m_udpAddress, "127.0.0.1");
        d.readU32(53, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9998;
        }

        if (m_rollupState)
        {
            d.readBlob(54, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(55, &m_workspaceIndex, 0);
        d.readBlob(56, &m_geometryBytes);
        d.readBool(57, &m_hidden, false);

        return true;
    }
    else
    {
        qDebug() << "PacketModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

void PacketModSettings::applySettings(const QStringList& settingsKeys, const PacketModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("modulation")) {
        m_modulation = settings.m_modulation;
    }
    if (settingsKeys.contains("baud")) {
        m_baud = settings.m_baud;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
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
    if (settingsKeys.contains("markFrequency")) {
        m_markFrequency = settings.m_markFrequency;
    }
    if (settingsKeys.contains("spaceFrequency")) {
        m_spaceFrequency = settings.m_spaceFrequency;
    }
    if (settingsKeys.contains("ax25PreFlags")) {
        m_ax25PreFlags = settings.m_ax25PreFlags;
    }
    if (settingsKeys.contains("ax25PostFlags")) {
        m_ax25PostFlags = settings.m_ax25PostFlags;
    }
    if (settingsKeys.contains("ax25Control")) {
        m_ax25Control = settings.m_ax25Control;
    }
    if (settingsKeys.contains("ax25PID")) {
        m_ax25PID = settings.m_ax25PID;
    }
    if (settingsKeys.contains("preEmphasis")) {
        m_preEmphasis = settings.m_preEmphasis;
    }
    if (settingsKeys.contains("preEmphasisTau")) {
        m_preEmphasisTau = settings.m_preEmphasisTau;
    }
    if (settingsKeys.contains("preEmphasisHighFreq")) {
        m_preEmphasisHighFreq = settings.m_preEmphasisHighFreq;
    }
    if (settingsKeys.contains("lpfTaps")) {
        m_lpfTaps = settings.m_lpfTaps;
    }
    if (settingsKeys.contains("bbNoise")) {
        m_bbNoise = settings.m_bbNoise;
    }
    if (settingsKeys.contains("rfNoise")) {
        m_rfNoise = settings.m_rfNoise;
    }
    if (settingsKeys.contains("writeToFile")) {
        m_writeToFile = settings.m_writeToFile;
    }
    if (settingsKeys.contains("spectrumRate")) {
        m_spectrumRate = settings.m_spectrumRate;
    }
    if (settingsKeys.contains("callsign")) {
        m_callsign = settings.m_callsign;
    }
    if (settingsKeys.contains("to")) {
        m_to = settings.m_to;
    }
    if (settingsKeys.contains("via")) {
        m_via = settings.m_via;
    }
    if (settingsKeys.contains("data")) {
        m_data = settings.m_data;
    }
    if (settingsKeys.contains("bpf")) {
        m_bpf = settings.m_bpf;
    }
    if (settingsKeys.contains("bpfLowCutoff")) {
        m_bpfLowCutoff = settings.m_bpfLowCutoff;
    }
    if (settingsKeys.contains("bpfHighCutoff")) {
        m_bpfHighCutoff = settings.m_bpfHighCutoff;
    }
    if (settingsKeys.contains("bpfTaps")) {
        m_bpfTaps = settings.m_bpfTaps;
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
    if (settingsKeys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
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

QString PacketModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("modulation") || force) {
        ostr << " m_modulation: " << m_modulation;
    }
    if (settingsKeys.contains("baud") || force) {
        ostr << " m_baud: " << m_baud;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("fmDeviation") || force) {
        ostr << " m_fmDeviation: " << m_fmDeviation;
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
    if (settingsKeys.contains("markFrequency") || force) {
        ostr << " m_markFrequency: " << m_markFrequency;
    }
    if (settingsKeys.contains("spaceFrequency") || force) {
        ostr << " m_spaceFrequency: " << m_spaceFrequency;
    }
    if (settingsKeys.contains("ax25PreFlags") || force) {
        ostr << " m_ax25PreFlags: " << m_ax25PreFlags;
    }
    if (settingsKeys.contains("ax25PostFlags") || force) {
        ostr << " m_ax25PostFlags: " << m_ax25PostFlags;
    }
    if (settingsKeys.contains("ax25Control") || force) {
        ostr << " m_ax25Control: " << m_ax25Control;
    }
    if (settingsKeys.contains("ax25PID") || force) {
        ostr << " m_ax25PID: " << m_ax25PID;
    }
    if (settingsKeys.contains("preEmphasis") || force) {
        ostr << " m_preEmphasis: " << m_preEmphasis;
    }
    if (settingsKeys.contains("preEmphasisTau") || force) {
        ostr << " m_preEmphasisTau: " << m_preEmphasisTau;
    }
    if (settingsKeys.contains("preEmphasisHighFreq") || force) {
        ostr << " m_preEmphasisHighFreq: " << m_preEmphasisHighFreq;
    }
    if (settingsKeys.contains("lpfTaps") || force) {
        ostr << " m_lpfTaps: " << m_lpfTaps;
    }
    if (settingsKeys.contains("bbNoise") || force) {
        ostr << " m_bbNoise: " << m_bbNoise;
    }
    if (settingsKeys.contains("rfNoise") || force) {
        ostr << " m_rfNoise: " << m_rfNoise;
    }
    if (settingsKeys.contains("writeToFile") || force) {
        ostr << " m_writeToFile: " << m_writeToFile;
    }
    if (settingsKeys.contains("spectrumRate") || force) {
        ostr << " m_spectrumRate: " << m_spectrumRate;
    }
    if (settingsKeys.contains("callsign") || force) {
        ostr << " m_callsign: " << m_callsign.toStdString();
    }
    if (settingsKeys.contains("to") || force) {
        ostr << " m_to: " << m_to.toStdString();
    }
    if (settingsKeys.contains("via") || force) {
        ostr << " m_via: " << m_via.toStdString();
    }
    if (settingsKeys.contains("data") || force) {
        ostr << " m_data: " << m_data.toStdString();
    }
    if (settingsKeys.contains("bpf") || force) {
        ostr << " m_bpf: " << m_bpf;
    }
    if (settingsKeys.contains("bpfLowCutoff") || force) {
        ostr << " m_bpfLowCutoff: " << m_bpfLowCutoff;
    }
    if (settingsKeys.contains("bpfHighCutoff") || force) {
        ostr << " m_bpfHighCutoff: " << m_bpfHighCutoff;
    }
    if (settingsKeys.contains("bpfTaps") || force) {
        ostr << " m_bpfTaps: " << m_bpfTaps;
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
    if (settingsKeys.contains("udpEnabled") || force) {
        ostr << " m_udpEnabled: " << m_udpEnabled;
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
