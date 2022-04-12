///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
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

#include "chanalyzersettings.h"

ChannelAnalyzerSettings::ChannelAnalyzerSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ChannelAnalyzerSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rationalDownSample = false;
    m_rationalDownSamplerRate = 0;
    m_bandwidth = 5000;
    m_lowCutoff = 300;
    m_log2Decim = 0;
    m_ssb = false;
    m_pll = false;
    m_fll = false;
    m_costasLoop = false;
    m_rrc = false;
    m_rrcRolloff = 35; // 0.35
    m_pllPskOrder = 1;
    m_pllBandwidth = 0.002f;
    m_pllDampingFactor = 0.5f;
    m_pllLoopGain = 10.0f;
    m_inputType = InputSignal;
    m_rgbColor = QColor(128, 128, 128).rgb();
    m_title = "Channel Analyzer";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray ChannelAnalyzerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_bandwidth);

    if (m_spectrumGUI) {
        s.writeBlob(3, m_spectrumGUI->serialize());
    }

    s.writeU32(4, m_rgbColor);
    s.writeS32(5, m_lowCutoff);
    s.writeS32(6, m_log2Decim);
    s.writeBool(7, m_ssb);

    if (m_scopeGUI) {
        s.writeBlob(8, m_scopeGUI->serialize());
    }

    s.writeBool(9, m_rationalDownSample);
    s.writeU32(10, m_rationalDownSamplerRate);
    s.writeBool(11, m_pll);
    s.writeBool(12, m_fll);
    s.writeU32(13, m_pllPskOrder);
    s.writeS32(14, (int) m_inputType);
    s.writeString(15, m_title);
    s.writeBool(16, m_rrc);
    s.writeU32(17, m_rrcRolloff);
    s.writeFloat(18, m_pllBandwidth);
    s.writeFloat(19, m_pllDampingFactor);
    s.writeFloat(20, m_pllLoopGain);
    s.writeBool(21, m_costasLoop);

    if (m_rollupState) {
        s.writeBlob(22, m_rollupState->serialize());
    }

    s.writeBool(23, m_useReverseAPI);
    s.writeString(24, m_reverseAPIAddress);
    s.writeU32(25, m_reverseAPIPort);
    s.writeU32(26, m_reverseAPIDeviceIndex);
    s.writeU32(27, m_reverseAPIChannelIndex);
    s.writeS32(28, m_streamIndex);
    s.writeS32(29, m_workspaceIndex);
    s.writeBlob(30, m_geometryBytes);
    s.writeBool(31, m_hidden);

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
        unsigned int utmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_bandwidth, 5000);

        if (m_spectrumGUI) {
            d.readBlob(3, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(4, &m_rgbColor);
        d.readS32(5, &m_lowCutoff, 3);
        d.readS32(6, &m_log2Decim, 0);
        d.readBool(7, &m_ssb, false);

        if (m_scopeGUI) {
            d.readBlob(8, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readBool(9, &m_rationalDownSample, false);
        d.readU32(10, &m_rationalDownSamplerRate, 2000U);
        d.readBool(11, &m_pll, false);
        d.readBool(12, &m_fll, false);
        d.readU32(13, &m_pllPskOrder, 1);
        d.readS32(14, &tmp, 0);
        m_inputType = (InputType) tmp;
        d.readString(15, &m_title, "Channel Analyzer");
        d.readBool(16, &m_rrc, false);
        d.readU32(17, &m_rrcRolloff, 35);
        d.readFloat(18, &m_pllBandwidth, 0.002f);
        d.readFloat(19, &m_pllDampingFactor, 0.5f);
        d.readFloat(20, &m_pllLoopGain, 10.0f);
        d.readBool(21, &m_costasLoop, false);

        if (m_rollupState)
        {
            d.readBlob(22, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readBool(23, &m_useReverseAPI, false);
        d.readString(24, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(25, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(26, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(27, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(28, &m_streamIndex, 0);
        d.readS32(29, &m_workspaceIndex, 0);
        d.readBlob(30, &m_geometryBytes);
        d.readBool(31, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
