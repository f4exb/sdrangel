///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/ctcssfrequencies.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "m17modsettings.h"

M17ModSettings::M17ModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void M17ModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 16000.0f;
    m_fmDeviation = 10000.0f; //!< full deviation
    m_toneFrequency = 1000.0f;
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "M17 Modulator";
    m_modAFInput = M17ModInputAF::M17ModInputNone;
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

QByteArray M17ModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(4, m_fmDeviation);
    s.writeU32(5, m_rgbColor);
    s.writeReal(6, m_toneFrequency);
    s.writeReal(7, m_volumeFactor);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeString(12, m_title);
    s.writeS32(13, (int) m_modAFInput);
    s.writeString(14, m_audioDeviceName);
    s.writeBool(15, m_useReverseAPI);
    s.writeString(16, m_reverseAPIAddress);
    s.writeU32(17, m_reverseAPIPort);
    s.writeU32(18, m_reverseAPIDeviceIndex);
    s.writeU32(19, m_reverseAPIChannelIndex);
    s.writeString(20, m_feedbackAudioDeviceName);
    s.writeReal(21, m_feedbackVolumeFactor);
    s.writeBool(22, m_feedbackAudioEnable);
    s.writeS32(23, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);

    return s.final();
}

bool M17ModSettings::deserialize(const QByteArray& data)
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
        d.readReal(4, &m_fmDeviation, 10000.0);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_toneFrequency, 1000.0);
        d.readReal(7, &m_volumeFactor, 1.0);
        d.readBlob(8, &bytetmp);

        if (m_channelMarker)
        {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(12, &m_title, "M17 Modulator");

        d.readS32(13, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) M17ModInputAF::M17ModInputTone)) {
            m_modAFInput = M17ModInputNone;
        } else {
            m_modAFInput = (M17ModInputAF) tmp;
        }

        d.readString(14, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(15, &m_useReverseAPI, false);
        d.readString(16, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(17, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(18, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(19, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(20, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(21, &m_feedbackVolumeFactor, 1.0);
        d.readBool(22, &m_feedbackAudioEnable, false);
        d.readS32(23, &m_streamIndex, 0);
        d.readS32(25, &tmp, 0023);

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);

        return true;
    }
    else
    {
        qDebug() << "NFMModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

