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
#include "ssbmodsettings.h"

const int SSBModSettings::m_agcTimeConstant[] = {
        1,
        2,
        5,
       10,
       20,
       50,
      100,
      200,
      500,
      990};

const int SSBModSettings::m_nbAGCTimeConstants = 10;

SSBModSettings::SSBModSettings() :
    m_channelMarker(0),
    m_spectrumGUI(0),
    m_cwKeyerGUI(0)
{
    resetToDefaults();
}

void SSBModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_bandwidth = 3000.0;
    m_lowCutoff = 300.0;
    m_usb = true;
    m_toneFrequency = 1000.0;
    m_volumeFactor = 1.0;
    m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
    m_spanLog2 = 3;
    m_audioBinaural = false;
    m_audioFlipChannels = false;
    m_dsb = false;
    m_audioMute = false;
    m_playLoop = false;
    m_agc = false;
    m_agcOrder = 0.2;
    m_agcTime = 9600;
    m_agcThresholdEnable = true;
    m_agcThreshold = -40; // dB
    m_agcThresholdGate = 192;
    m_agcThresholdDelay = 2400;
    m_rgbColor = QColor(0, 255, 0).rgb();
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_title = "SSB Modulator";
}

QByteArray SSBModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, roundf(m_bandwidth / 100.0));
    s.writeS32(3, roundf(m_toneFrequency / 10.0));

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);

    if (m_cwKeyerGUI) {
        s.writeBlob(6, m_cwKeyerGUI->serialize());
    }

    s.writeS32(7, roundf(m_lowCutoff / 100.0));
    s.writeS32(8, m_spanLog2);
    s.writeBool(9, m_audioBinaural);
    s.writeBool(10, m_audioFlipChannels);
    s.writeBool(11, m_dsb);
    s.writeBool(12, m_agc);
    s.writeS32(13, getAGCTimeConstantIndex(m_agcTime/48));
    s.writeS32(14, m_agcThreshold); // dB
    s.writeS32(15, m_agcThresholdGate / 48);
    s.writeS32(16, m_agcThresholdDelay / 48);
    s.writeS32(17, roundf(m_agcOrder * 100.0));

    if (m_channelMarker) {
        s.writeBlob(18, m_channelMarker->serialize());
    }

    s.writeString(19, m_title);

    return s.final();
}

bool SSBModSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;

        d.readS32(2, &tmp, 30);
        m_bandwidth = tmp * 100.0;

        d.readS32(3, &tmp, 100);
        m_toneFrequency = tmp * 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);

        if (m_cwKeyerGUI) {
            d.readBlob(6, &bytetmp);
            m_cwKeyerGUI->deserialize(bytetmp);
        }

        d.readS32(7, &tmp, 3);
        m_lowCutoff = tmp * 100.0;

        d.readS32(8, &m_spanLog2, 3);
        d.readBool(9, &m_audioBinaural, false);
        d.readBool(10, &m_audioFlipChannels, false);
        d.readBool(11, &m_dsb, false);
        d.readBool(12, &m_agc, false);
        d.readS32(13, &tmp, 7);
        m_agcTime = getAGCTimeConstant(tmp) * 48;
        d.readS32(14, &m_agcThreshold, -40);
        d.readS32(15, &tmp, 4);
        m_agcThresholdGate = tmp * 48;
        d.readS32(16, &tmp, 5);
        m_agcThresholdDelay = tmp * 48;
        d.readS32(17, &tmp, 20);
        m_agcOrder = tmp / 100.0;

        if (m_channelMarker) {
            d.readBlob(18, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(19, &m_title, "SSB Modulator");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int SSBModSettings::getAGCTimeConstant(int index)
{
    if (index < 0) {
        return m_agcTimeConstant[0];
    } else if (index < m_nbAGCTimeConstants) {
        return m_agcTimeConstant[index];
    } else {
        return m_agcTimeConstant[m_nbAGCTimeConstants-1];
    }
}

int SSBModSettings::getAGCTimeConstantIndex(int agcTimeConstant)
{
    for (int i = 0; i < m_nbAGCTimeConstants; i++)
    {
        if (agcTimeConstant <= m_agcTimeConstant[i])
        {
            return i;
        }
    }

    return m_nbAGCTimeConstants-1;
}
