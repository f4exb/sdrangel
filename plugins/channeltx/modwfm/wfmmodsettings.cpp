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

#include "wfmmodsettings.h"

const int WFMModSettings::m_rfBW[] = {
    12500, 25000, 40000, 60000, 75000, 80000, 100000, 125000, 140000, 160000, 180000, 200000, 220000, 250000
};
const int WFMModSettings::m_nbRfBW = 14;

WFMModSettings::WFMModSettings() :
    m_channelMarker(0),
    m_cwKeyerGUI(0)
{
    resetToDefaults();
}

void WFMModSettings::resetToDefaults()
{
    m_basebandSampleRate = 384000;
    m_outputSampleRate = 384000;
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 125000.0f;
    m_afBandwidth = 15000.0f;
    m_fmDeviation = 50000.0f;
    m_toneFrequency = 1000.0f;
    m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(0, 0, 255).rgb();
    m_title = "WFM Modulator";
}

QByteArray WFMModSettings::serialize() const
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
        s.writeBlob(9, m_channelMarker->serialize());
    }

    s.writeString(10, m_title);

    return s.final();
}

bool WFMModSettings::deserialize(const QByteArray& data)
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
        d.readReal(2, &m_rfBandwidth, 125000.0);
        d.readReal(3, &m_afBandwidth, 15000.0);
        d.readReal(4, &m_fmDeviation, 50000.0);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_toneFrequency, 1000.0);
        d.readReal(7, &m_volumeFactor, 1.0);

        if (m_cwKeyerGUI) {
            d.readBlob(8, &bytetmp);
            m_cwKeyerGUI->deserialize(bytetmp);
        }

        if (m_channelMarker) {
            d.readBlob(9, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(10, &m_title, "WFM Modulator");

        return true;
    }
    else
    {
        qDebug() << "WFMModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

int WFMModSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbRfBW) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbRfBW-1];
    }
}

int WFMModSettings::getRFBWIndex(int rfbw)
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
