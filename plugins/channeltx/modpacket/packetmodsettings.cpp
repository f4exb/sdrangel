///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "packetmodsettings.h"

PacketModSettings::PacketModSettings()
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
    m_rgbColor = QColor(255, 0, 0).rgb();
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
}

bool PacketModSettings::setMode(QString mode)
{
    int baud;
    bool valid;

    // First part of mode string should give baud rate
    baud = mode.split(" ")[0].toInt(&valid);
    if (!valid)
        return false;

    if (mode.endsWith("AFSK"))
    {
        // UK channels - https://rsgb.org/main/blog/news/gb2rs/headlines/2015/12/04/check-your-aprs-deviation/
        m_baud = baud;
        m_scramble = false;
        m_rfBandwidth = 12500.0f;
        m_fmDeviation = 2500.0f;
        m_spectrumRate = 8000;
        m_modulation = PacketModSettings::AFSK;
    }
    else if (mode.endsWith("FSK"))
    {
        // G3RUH - http://www.jrmiller.demon.co.uk/products/figs/man9k6.pdf
        m_baud = baud;
        m_scramble = true;
        m_rfBandwidth = 20000.0f;
        m_fmDeviation = 3000.0f;
        m_spectrumRate = 24000;
        m_bpf = false;
        m_modulation = PacketModSettings::FSK;
    }
    else
        return false;
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

        if (m_channelMarker) {
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

        return true;
    }
    else
    {
        qDebug() << "PacketModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}
