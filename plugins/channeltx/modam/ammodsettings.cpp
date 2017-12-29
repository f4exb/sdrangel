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
#include "ammodsettings.h"

AMModSettings::AMModSettings() :
    m_channelMarker(0),
    m_cwKeyerGUI(0)
{
    resetToDefaults();
}

void AMModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500.0;
    m_modFactor = 0.2f;
    m_toneFrequency = 1000.0f;
    m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(255, 255, 0).rgb();
    m_title = "AM Modulator";
}

QByteArray AMModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_toneFrequency);
    s.writeReal(4, m_modFactor);
    s.writeU32(5, m_rgbColor);
    s.writeReal(6, m_volumeFactor);

    if (m_cwKeyerGUI) {
        s.writeBlob(7, m_cwKeyerGUI->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(8, m_channelMarker->serialize());
    }

    s.writeString(9, m_title);

    return s.final();
}

bool AMModSettings::deserialize(const QByteArray& data)
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
        d.readReal(3, &m_toneFrequency, 1000.0);
        d.readReal(4, &m_modFactor, 0.2f);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_volumeFactor, 1.0);

        if (m_cwKeyerGUI) {
            d.readBlob(7, &bytetmp);
            m_cwKeyerGUI->deserialize(bytetmp);
        }

        if (m_channelMarker) {
            d.readBlob(8, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(9, &m_title, "AM Modulator");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
