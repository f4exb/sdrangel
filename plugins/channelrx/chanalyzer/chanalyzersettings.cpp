///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
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

#include "chanalyzersettings.h"

ChannelAnalyzerSettings::ChannelAnalyzerSettings() :
    m_channelMarker(0),
    m_spectrumGUI(0),
    m_scopeGUI(0)
{
    resetToDefaults();
}

void ChannelAnalyzerSettings::resetToDefaults()
{
    m_frequency = 0;
    m_downSample = false;
    m_downSampleRate = 0;
    m_bandwidth = 5000;
    m_lowCutoff = 300;
    m_spanLog2 = 0;
    m_ssb = false;
    m_pll = false;
    m_fll = false;
    m_rrc = false;
    m_rrcRolloff = 35; // 0.35
    m_pllPskOrder = 1;
    m_inputType = InputSignal;
    m_rgbColor = QColor(128, 128, 128).rgb();
    m_title = "Channel Analyzer";
}

QByteArray ChannelAnalyzerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_frequency);
    s.writeS32(2, m_bandwidth);
    s.writeBlob(3, m_spectrumGUI->serialize());
    s.writeU32(4, m_rgbColor);
    s.writeS32(5, m_lowCutoff);
    s.writeS32(6, m_spanLog2);
    s.writeBool(7, m_ssb);
    s.writeBlob(8, m_scopeGUI->serialize());
    s.writeBool(9, m_downSample);
    s.writeU32(10, m_downSampleRate);
    s.writeBool(11, m_pll);
    s.writeBool(12, m_fll);
    s.writeU32(13, m_pllPskOrder);
    s.writeS32(14, (int) m_inputType);
    s.writeString(15, m_title);
    s.writeBool(16, m_rrc);
    s.writeU32(17, m_rrcRolloff);

    return s.final();
}

bool ChannelAnalyzerSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_frequency, 0);
        d.readS32(2, &m_bandwidth, 5000);

        if (m_spectrumGUI) {
            d.readBlob(3, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(4, &m_rgbColor);
        d.readS32(5, &m_lowCutoff, 3);
        d.readS32(6, &m_spanLog2, 0);
        d.readBool(7, &m_ssb, false);

        if (m_scopeGUI) {
            d.readBlob(8, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readBool(9, &m_downSample, false);
        d.readU32(10, &m_downSampleRate, 2000U);
        d.readBool(11, &m_pll, false);
        d.readBool(12, &m_fll, false);
        d.readU32(13, &m_pllPskOrder, 1);
        d.readS32(14, &tmp, 0);
        m_inputType = (InputType) tmp;
        d.readString(15, &m_title, "Channel Analyzer");
        d.readBool(16, &m_rrc, false);
        d.readU32(17, &m_rrcRolloff, 35);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
