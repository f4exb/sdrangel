///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "interferometersettings.h"

InterferometerSettings::InterferometerSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_scopeGUI(nullptr)
{
    resetToDefaults();
}

void InterferometerSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_correlationType = CorrelationAdd;
    m_rgbColor = QColor(128, 128, 128).rgb();
    m_title = "Interferometer";
}

QByteArray InterferometerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, (int) m_correlationType);
    s.writeU32(3, m_rgbColor);
    s.writeString(4, m_title);

    if (m_spectrumGUI) {
        s.writeBlob(20, m_spectrumGUI->serialize());
    }
    if (m_scopeGUI) {
        s.writeBlob(21, m_scopeGUI->serialize());
    }
    if (m_channelMarker) {
        s.writeBlob(22, m_channelMarker->serialize());
    }
}

bool InterferometerSettings::deserialize(const QByteArray& data)
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
        int tmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &tmp, 0);
        m_correlationType = (CorrelationType) tmp;
        d.readU32(3, &m_rgbColor);
        d.readString(4, &m_title, "Interpolator");

        if (m_spectrumGUI) {
            d.readBlob(20, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        if (m_scopeGUI) {
            d.readBlob(21, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        if (m_channelMarker) {
            d.readBlob(21, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}