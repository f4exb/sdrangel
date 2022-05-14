///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
    m_channelMarker(nullptr),
    m_cwKeyerGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void WFMModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 125000.0f;
    m_afBandwidth = 15000.0f;
    m_fmDeviation = 50000.0f;
    m_toneFrequency = 1000.0f;
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(0, 0, 255).rgb();
    m_title = "WFM Modulator";
    m_modAFInput = WFMModInputNone;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_feedbackAudioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_feedbackVolumeFactor = 0.5f;
    m_feedbackAudioEnable = false;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
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
    } else { // standalone operation with presets
        s.writeBlob(6, m_cwKeyerSettings.serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(9, m_channelMarker->serialize());
    }

    s.writeString(10, m_title);
    s.writeString(11, m_audioDeviceName);
    s.writeS32(12, (int) m_modAFInput);
    s.writeBool(13, m_useReverseAPI);
    s.writeString(14, m_reverseAPIAddress);
    s.writeU32(15, m_reverseAPIPort);
    s.writeU32(16, m_reverseAPIDeviceIndex);
    s.writeU32(17, m_reverseAPIChannelIndex);
    s.writeS32(18, m_streamIndex);
    s.writeString(19, m_feedbackAudioDeviceName);
    s.writeReal(20, m_feedbackVolumeFactor);
    s.writeBool(21, m_feedbackAudioEnable);

    if (m_rollupState) {
        s.writeBlob(22, m_rollupState->serialize());
    }

    s.writeS32(23, m_workspaceIndex);
    s.writeBlob(24, m_geometryBytes);
    s.writeBool(25, m_hidden);

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
        uint32_t utmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 125000.0);
        d.readReal(3, &m_afBandwidth, 15000.0);
        d.readReal(4, &m_fmDeviation, 50000.0);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_toneFrequency, 1000.0);
        d.readReal(7, &m_volumeFactor, 1.0);
        d.readBlob(8, &bytetmp);

        if (m_cwKeyerGUI) {
            m_cwKeyerGUI->deserialize(bytetmp);
        } else { // standalone operation with presets
            m_cwKeyerSettings.deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(9, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(10, &m_title, "WFM Modulator");
        d.readString(11, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);

        d.readS32(12, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) WFMModInputAF::WFMModInputTone)) {
            m_modAFInput = WFMModInputNone;
        } else {
            m_modAFInput = (WFMModInputAF) tmp;
        }

        d.readBool(13, &m_useReverseAPI, false);
        d.readString(14, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(15, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(16, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(17, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(18, &m_streamIndex, 0);
        d.readString(19, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(20, &m_feedbackVolumeFactor, 1.0);
        d.readBool(21, &m_feedbackAudioEnable, false);

        if (m_rollupState)
        {
            d.readBlob(22, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(23, &m_workspaceIndex, 0);
        d.readBlob(24, &m_geometryBytes);
        d.readBool(25, &m_hidden, false);

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
