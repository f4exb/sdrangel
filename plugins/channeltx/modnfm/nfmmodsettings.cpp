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
#include <QDebug>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "nfmmodsettings.h"

const int NFMModSettings::m_rfBW[] = {
        3000, 4000, 5000, 6250, 8330, 10000, 12500, 15000, 20000, 25000, 40000
};
const int NFMModSettings::m_nbRfBW = 11;

const float NFMModSettings::m_ctcssFreqs[] = {
        67.0,  71.9,  74.4,  77.0,  79.7,  82.5,  85.4,  88.5,  91.5,  94.8,
        97.4, 100.0, 103.5, 107.2, 110.9, 114.8, 118.8, 123.0, 127.3, 131.8,
       136.5, 141.3, 146.2, 151.4, 156.7, 162.2, 167.9, 173.8, 179.9, 186.2,
       192.8, 203.5
};
const int NFMModSettings::m_nbCTCSSFreqs = 32;


NFMModSettings::NFMModSettings() :
    m_channelMarker(0),
    m_cwKeyerGUI(0)
{
    resetToDefaults();
}

void NFMModSettings::resetToDefaults()
{
    m_basebandSampleRate = 48000;
    m_outputSampleRate = 48000;
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500.0f;
    m_fmDeviation = 5000.0f;
    m_toneFrequency = 1000.0f;
    m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_ctcssOn = false;
    m_ctcssIndex = 0;
    m_rgbColor = QColor(255, 0, 0).rgb();
    m_title = "NFM Modulator";
}

QByteArray NFMModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_afBandwidth);
    s.writeReal(4, m_fmDeviation);
    s.writeU32(5, m_rgbColor);
    s.writeReal(6, m_toneFrequency);
    s.writeReal(7, m_volumeFactor);

    if (m_cwKeyerGUI) {
        s.writeBlob(8, m_cwKeyerGUI->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeBool(9, m_ctcssOn);
    s.writeS32(10, m_ctcssIndex);
    s.writeString(12, m_title);

    return s.final();
}

bool NFMModSettings::deserialize(const QByteArray& data)
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
        d.readReal(2, &m_rfBandwidth, 12500.0);
        d.readReal(3, &m_afBandwidth, 1000.0);
        d.readReal(4, &m_fmDeviation, 5000.0);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_toneFrequency, 1000.0);
        d.readReal(7, &m_volumeFactor, 1.0);

        if (m_cwKeyerGUI) {
            d.readBlob(8, &bytetmp);
            m_cwKeyerGUI->deserialize(bytetmp);
        }

        d.readBool(9, &m_ctcssOn, false);
        d.readS32(10, &m_ctcssIndex, 0);

        if (m_channelMarker) {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(12, &m_title, "NFM Modulator");

        return true;
    }
    else
    {
        qDebug() << "NFMModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

int NFMModSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbRfBW) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbRfBW-1];
    }
}

int NFMModSettings::getRFBWIndex(int rfbw)
{
    for (int i = 0; i < m_nbRfBW; i++)
    {
        if (rfbw <= m_rfBW[i])
        {
            return i;
        }
    }

    return m_nbRfBW-1;
}

float NFMModSettings::getCTCSSFreq(int index)
{
    if (index < 0) {
        return m_ctcssFreqs[0];
    } else if (index < m_nbCTCSSFreqs) {
        return m_ctcssFreqs[index];
    } else {
        return m_ctcssFreqs[m_nbCTCSSFreqs-1];
    }
}

int NFMModSettings::getCTCSSFreqIndex(float ctcssFreq)
{
    for (int i = 0; i < m_nbCTCSSFreqs; i++)
    {
        if (ctcssFreq <= m_ctcssFreqs[i])
        {
            return i;
        }
    }

    return m_nbCTCSSFreqs-1;
}

