///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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
#include "ammodsettings.h"

AMModSettings::AMModSettings() :
    m_channelMarker(nullptr),
    m_cwKeyerGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AMModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500.0;
    m_modFactor = 0.2f;
    m_toneFrequency = 1000.0f;
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(255, 255, 0).rgb();
    m_title = "AM Modulator";
    m_modAFInput = AMModInputAF::AMModInputNone;
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
    } else { // standalone operation with presets
        s.writeBlob(7, m_cwKeyerSettings.serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(8, m_channelMarker->serialize());
    }

    s.writeString(9, m_title);
    s.writeString(10, m_audioDeviceName);
    s.writeS32(11, (int) m_modAFInput);
    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);
    s.writeU32(16, m_reverseAPIChannelIndex);
    s.writeString(17, m_feedbackAudioDeviceName);
    s.writeReal(18, m_feedbackVolumeFactor);
    s.writeBool(19, m_feedbackAudioEnable);
    s.writeS32(20, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(21, m_rollupState->serialize());
    }

    s.writeS32(22, m_workspaceIndex);
    s.writeBlob(23, m_geometryBytes);
    s.writeBool(24, m_hidden);

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
        uint32_t utmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 12500.0);
        d.readReal(3, &m_toneFrequency, 1000.0);
        d.readReal(4, &m_modFactor, 0.2f);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_volumeFactor, 1.0);
        d.readBlob(7, &bytetmp);

        if (m_cwKeyerGUI) {
            m_cwKeyerGUI->deserialize(bytetmp);
        } else { // standalone operation with presets
            m_cwKeyerSettings.deserialize(bytetmp);
        }

        if (m_channelMarker) {
            d.readBlob(8, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(9, &m_title, "AM Modulator");
        d.readString(10, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(11, &tmp, 0);

        if ((tmp < 0) || (tmp > (int) AMModInputAF::AMModInputTone)) {
            m_modAFInput = AMModInputNone;
        } else {
            m_modAFInput = (AMModInputAF) tmp;
        }

        d.readBool(12, &m_useReverseAPI, false);
        d.readString(13, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(14, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(15, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(16, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(17, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(18, &m_feedbackVolumeFactor, 1.0);
        d.readBool(19, &m_feedbackAudioEnable, false);
        d.readS32(20, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(21, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(22, &m_workspaceIndex, 0);
        d.readBlob(23, &m_geometryBytes);
        d.readBool(24, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
