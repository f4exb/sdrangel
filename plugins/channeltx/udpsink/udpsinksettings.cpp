///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "udpsinksettings.h"

UDPSinkSettings::UDPSinkSettings() :
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void UDPSinkSettings::resetToDefaults()
{
    m_sampleFormat = FormatS16LE;
    m_inputSampleRate = 48000;
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500;
    m_fmDeviation = 2500;
    m_amModFactor = 0.95;
    m_channelMute = false;
    m_gainIn = 1.0;
    m_gainOut = 1.0;
    m_squelch = -60.0;
    m_squelchGate = 0.05;
    m_autoRWBalance = true;
    m_stereoInput = false;
    m_squelchEnabled = true;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_title = "UDP Sample Sink";
}

QByteArray UDPSinkSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(2, m_inputFrequencyOffset);
    s.writeS32(3, (int) m_sampleFormat);
    s.writeReal(4, m_inputSampleRate);
    s.writeReal(5, m_rfBandwidth);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    if (m_spectrumGUI) {
        s.writeBlob(7, m_spectrumGUI->serialize());
    }

    s.writeS32(10, roundf(m_gainOut * 10.0));
    s.writeS32(11, m_fmDeviation);
    s.writeReal(12, m_amModFactor);
    s.writeBool(13, m_stereoInput);
    s.writeS32(14, roundf(m_squelch));
    s.writeS32(15, roundf(m_squelchGate * 100.0));
    s.writeBool(16, m_autoRWBalance);
    s.writeS32(17, roundf(m_gainIn * 10.0));
    s.writeString(18, m_udpAddress);
    s.writeU32(19, m_udpPort);
    s.writeString(20, m_title);

    return s.final();
}

bool UDPSinkSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        QString strtmp;
        qint32 s32tmp;
        quint32 u32tmp;

        if (m_channelMarker)
        {
            d.readBlob(6, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(2, &s32tmp, 0);
        m_inputFrequencyOffset = s32tmp;

        d.readS32(3, &s32tmp, 0);

        if (s32tmp < (int) FormatNone) {
            m_sampleFormat = (SampleFormat) s32tmp;
        } else {
            m_sampleFormat = (SampleFormat) ((int) FormatNone - 1);
        }

        d.readReal(4, &m_inputSampleRate, 48000);
        d.readReal(5, &m_rfBandwidth, 32000);

        if (m_spectrumGUI)
        {
            d.readBlob(7, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readS32(10, &s32tmp, 10);
        m_gainOut = s32tmp / 10.0;

        d.readS32(11, &m_fmDeviation, 2500);
        d.readReal(12, &m_amModFactor, 0.95);
        d.readBool(13, &m_stereoInput, false);

        d.readS32(14, &s32tmp, -60);
        m_squelch = s32tmp * 1.0;
        m_squelchEnabled = (s32tmp != -100);

        d.readS32(15, &s32tmp, 5);
        m_squelchGate = s32tmp / 100.0;

        d.readBool(16, &m_autoRWBalance, true);

        d.readS32(17, &s32tmp, 10);
        m_gainIn = s32tmp / 10.0;

        d.readString(18, &m_udpAddress, "127.0.0.1");
        d.readU32(19, &u32tmp, 10);

        if ((u32tmp > 1024) & (u32tmp < 65538)) {
            m_udpPort = u32tmp;
        } else {
            m_udpPort = 9999;
        }

        d.readString(20, &m_title, "UDP Sample Sink");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}





