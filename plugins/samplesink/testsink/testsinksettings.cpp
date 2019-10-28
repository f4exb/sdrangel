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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "testsinksettings.h"

TestSinkSettings::TestSinkSettings()
{
    resetToDefaults();
}

void TestSinkSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_sampleRate = 48000;
    m_log2Interp = 0;
    m_spectrumGUI = nullptr;
}

QByteArray TestSinkSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU64(1, m_centerFrequency);
    s.writeU64(2, m_sampleRate);
    s.writeU32(3, m_log2Interp);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    return s.final();
}

bool TestSinkSettings::deserialize(const QByteArray& data)
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

        d.readU64(1, &m_sampleRate, 435000*1000);
        d.readU64(2, &m_sampleRate, 48000);
        d.readU32(3, &m_log2Interp, 0);

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
