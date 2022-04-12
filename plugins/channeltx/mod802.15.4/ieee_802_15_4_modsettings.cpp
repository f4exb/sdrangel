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
