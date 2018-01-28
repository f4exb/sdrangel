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
#include "udpsrcsettings.h"

UDPSrcSettings::UDPSrcSettings() :
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void UDPSrcSettings::resetToDefaults()
{
    m_outputSampleRate = 48000;
    m_sampleFormat = FormatIQ;
    m_sampleSize = Size16bits;
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500;
    m_fmDeviation = 2500;
    m_channelMute = false;
    m_gain = 1.0;
    m_squelchdB = -60;
    m_squelchGate = 0.0;
    m_squelchEnabled = true;
    m_agc = false;
    m_audioActive = false;
    m_audioStereo = false;
    m_volume = 20;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_audioPort = 9998;
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_title = "UDP Sample Source";
}

QByteArray UDPSrcSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(2, m_inputFrequencyOffset);
    s.writeS32(3, (int) m_sampleFormat);
    s.writeReal(4, m_outputSampleRate);
    s.writeReal(5, m_rfBandwidth);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    if (m_spectrumGUI) {
        s.writeBlob(7, m_spectrumGUI->serialize());
    }

    s.writeS32(8, m_gain*10.0);
    s.writeU32(9, m_rgbColor);
    s.writeBool(11, m_audioActive);
    s.writeS32(12, m_volume);
    s.writeBool(14, m_audioStereo);
    s.writeS32(15, m_fmDeviation);
    s.writeS32(16, m_squelchdB);
    s.writeS32(17, m_squelchGate);
    s.writeBool(18, m_agc);
    s.writeString(19, m_title);
    s.writeS32(20, (int) m_sampleFormat);

    return s.final();

}

bool UDPSrcSettings::deserialize(const QByteArray& data)
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
        int32_t s32tmp;

        if (m_channelMarker) {
            d.readBlob(6, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(2, &s32tmp, 0);
        m_inputFrequencyOffset = s32tmp;

        d.readS32(3, &s32tmp, FormatIQ);

        if ((s32tmp >= 0) && (s32tmp < (int) FormatNone)) {
            m_sampleFormat = (SampleFormat) s32tmp;
        } else {
            m_sampleFormat = FormatIQ;
        }

        d.readReal(4, &m_outputSampleRate, 48000.0);
        d.readReal(5, &m_rfBandwidth, 32000.0);

        if (m_spectrumGUI) {
            d.readBlob(7, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readS32(8, &s32tmp, 10);
        m_gain = s32tmp / 10.0;
        d.readU32(9, &m_rgbColor);
        d.readBool(11, &m_audioActive, false);
        d.readS32(12, &m_volume, 20);
        d.readBool(14, &m_audioStereo, false);
        d.readS32(15, &m_fmDeviation, 2500);
        d.readS32(16, &m_squelchdB, -60);
        d.readS32(17, &m_squelchGate, 5);
        d.readBool(18, &m_agc, false);
        d.readString(19, &m_title, "UDP Sample Source");

        d.readS32(20, &s32tmp, Size16bits);

        if ((s32tmp >= 0) && (s32tmp < (int) SizeNone)) {
            m_sampleSize = (SampleSize) s32tmp;
        } else {
            m_sampleSize = Size16bits;
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

