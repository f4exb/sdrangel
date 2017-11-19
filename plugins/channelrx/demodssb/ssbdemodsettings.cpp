///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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
#include "ssbdemodsettings.h"

SSBDemodSettings::SSBDemodSettings() :
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void SSBDemodSettings::resetToDefaults()
{
    m_audioBinaural = false;
    m_audioFlipChannels = false;
    m_dsb = false;
    m_audioMute = false;
    m_agc = false;
    m_agcClamping = false;
    m_agcPowerThreshold = -40;
    m_agcThresholdGate = 4;
    m_agcTimeLog2 = 7;
    m_rfBandwidth = 3000;
    m_lowCutoff = 300;
    m_volume = 3.0;
    m_spanLog2 = 3;
    m_inputSampleRate = 96000;
    m_inputFrequencyOffset = 0;
    m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
    m_rgbColor = QColor(0, 255, 0).rgb();
    m_title = "SSB Demodulator";
}

QByteArray SSBDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth / 100.0);
    s.writeS32(3, m_volume * 10.0);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);
    s.writeS32(6, m_lowCutoff / 100.0);
    s.writeS32(7, m_spanLog2);
    s.writeBool(8, m_audioBinaural);
    s.writeBool(9, m_audioFlipChannels);
    s.writeBool(10, m_dsb);
    s.writeBool(11, m_agc);
    s.writeS32(12, m_agcTimeLog2);
    s.writeS32(13, m_agcPowerThreshold);
    s.writeS32(14, m_agcThresholdGate);
    s.writeBool(15, m_agcClamping);
    s.writeString(16, m_title);

    return s.final();
}

bool SSBDemodSettings::deserialize(const QByteArray& data)
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
        QString strtmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &tmp, 30);
        m_rfBandwidth = tmp * 100.0;
        d.readS32(3, &tmp, 30);
        m_volume = tmp / 10.0;

        if (m_spectrumGUI) {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);
        d.readS32(6, &tmp, 30);
        m_lowCutoff = tmp * 100.0;
        d.readS32(7, &m_spanLog2, 3);
        d.readBool(8, &m_audioBinaural, false);
        d.readBool(9, &m_audioFlipChannels, false);
        d.readBool(10, &m_dsb, false);
        d.readBool(11, &m_agc, false);
        d.readS32(12, &m_agcTimeLog2, 7);
        d.readS32(13, &m_agcPowerThreshold, -40);
        d.readS32(14, &m_agcThresholdGate, 4);
        d.readBool(15, &m_agcClamping, false);
        d.readString(16, &m_title, "SSB Demodulator");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
