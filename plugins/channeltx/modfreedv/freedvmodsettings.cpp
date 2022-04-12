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
#include "freedvmodsettings.h"

FreeDVModSettings::FreeDVModSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_cwKeyerGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void FreeDVModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_toneFrequency = 1000.0;
    m_volumeFactor = 1.0;
    m_spanLog2 = 3;
    m_audioMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(0, 255, 204).rgb();
    m_title = "FreeDV Modulator";
    m_modAFInput = FreeDVModInputAF::FreeDVModInputNone;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_freeDVMode = FreeDVMode::FreeDVMode2400A;
    m_gaugeInputElseModem = false;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray FreeDVModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(3, roundf(m_toneFrequency / 10.0));

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);

    if (m_cwKeyerGUI) {
        s.writeBlob(6, m_cwKeyerGUI->serialize());
    } else { // standalone operation with presets
        s.writeBlob(6, m_cwKeyerSettings.serialize());
    }

    s.writeBool(7, m_gaugeInputElseModem);
    s.writeS32(8, m_spanLog2);
    s.writeS32(10, (int) m_freeDVMode);

    if (m_channelMarker) {
        s.writeBlob(18, m_channelMarker->serialize());
    }

    s.writeString(19, m_title);
    s.writeString(20, m_audioDeviceName);
    s.writeS32(21, (int) m_modAFInput);
    s.writeBool(22, m_useReverseAPI);
    s.writeString(23, m_reverseAPIAddress);
    s.writeU32(24, m_reverseAPIPort);
    s.writeU32(25, m_reverseAPIDeviceIndex);
    s.writeU32(26, m_reverseAPIChannelIndex);
    s.writeS32(27, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(28, m_rollupState->serialize());
    }

    s.writeS32(29, m_workspaceIndex);
    s.writeBlob(30, m_geometryBytes);
    s.writeBool(31, m_hidden);

    return s.final();
}

bool FreeDVModSettings::deserialize(const QByteArray& data)
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

        d.readS32(3, &tmp, 100);
        m_toneFrequency = tmp * 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);
        d.readBlob(6, &bytetmp);

        if (m_cwKeyerGUI) {
            m_cwKeyerGUI->deserialize(bytetmp);
        } else { // standalone operation with presets
            m_cwKeyerSettings.deserialize(bytetmp);
        }

        d.readBool(7, &m_gaugeInputElseModem, false);
        d.readS32(8, &m_spanLog2, 3);

        d.readS32(10, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) FreeDVMode::FreeDVMode700D)) {
            m_freeDVMode = FreeDVMode::FreeDVMode2400A;
        } else {
            m_freeDVMode = (FreeDVMode) tmp;
        }

        if (m_channelMarker) {
            d.readBlob(18, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(19, &m_title, "FreeDV Modulator");
        d.readString(20, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);

        d.readS32(21, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) FreeDVModInputAF::FreeDVModInputTone)) {
            m_modAFInput = FreeDVModInputNone;
        } else {
            m_modAFInput = (FreeDVModInputAF) tmp;
        }

        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(25, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(26, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(27, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(28, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

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

int FreeDVModSettings::getHiCutoff(FreeDVMode freeDVMode)
{
    switch(freeDVMode)
    {
        case FreeDVModSettings::FreeDVMode800XA: // C4FM NB
        case FreeDVModSettings::FreeDVMode700C: // OFDM
        case FreeDVModSettings::FreeDVMode700D: // OFDM
        case FreeDVModSettings::FreeDVMode1600: // OFDM
            return 2400.0;
            break;
        case FreeDVModSettings::FreeDVMode2400A: // C4FM WB
        default:
            return 6000.0;
            break;
    }
}

int FreeDVModSettings::getLowCutoff(FreeDVMode freeDVMode)
{
    switch(freeDVMode)
    {
        case FreeDVModSettings::FreeDVMode800XA: // C4FM NB
            return 400.0;
            break;
        case FreeDVModSettings::FreeDVMode700C: // OFDM
        case FreeDVModSettings::FreeDVMode700D: // OFDM
        case FreeDVModSettings::FreeDVMode1600: // OFDM
            return 600.0;
            break;
        case FreeDVModSettings::FreeDVMode2400A: // C4FM WB
        default:
            return 0.0;
            break;
    }
}

int FreeDVModSettings::getModSampleRate(FreeDVMode freeDVMode)
{
    if (freeDVMode == FreeDVModSettings::FreeDVMode2400A) {
        return 48000;
    } else {
        return 8000;
    }
}
