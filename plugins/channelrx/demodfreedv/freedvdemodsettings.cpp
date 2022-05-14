///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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
#include "freedvdemodsettings.h"

#ifdef SDR_RX_SAMPLE_24BIT
const int FreeDVDemodSettings::m_minPowerThresholdDB = -120;
const float FreeDVDemodSettings::m_mminPowerThresholdDBf = 120.0f;
#else
const int FreeDVDemodSettings::m_minPowerThresholdDB = -100;
const float FreeDVDemodSettings::m_mminPowerThresholdDBf = 100.0f;
#endif

FreeDVDemodSettings::FreeDVDemodSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void FreeDVDemodSettings::resetToDefaults()
{
    m_audioMute = false;
    m_agc = true;
    m_volume = 3.0;
    m_volumeIn = 1.0;
    m_spanLog2 = 3;
    m_inputFrequencyOffset = 0;
    m_rgbColor = QColor(0, 255, 204).rgb();
    m_title = "FreeDV Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_freeDVMode = FreeDVMode2400A;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray FreeDVDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(3, m_volume * 10.0);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);
    s.writeS32(6, m_volumeIn * 10.0);
    s.writeS32(7, m_spanLog2);
    s.writeBool(11, m_agc);
    s.writeString(16, m_title);
    s.writeString(17, m_audioDeviceName);
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIDeviceIndex);
    s.writeU32(22, m_reverseAPIChannelIndex);
    s.writeS32(23, (int) m_freeDVMode);
    s.writeS32(24, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(25, m_rollupState->serialize());
    }

    s.writeS32(26, m_workspaceIndex);
    s.writeBlob(27, m_geometryBytes);
    s.writeBool(28, m_hidden);

    return s.final();
}

bool FreeDVDemodSettings::deserialize(const QByteArray& data)
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
        QString strtmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &tmp, 30);
        d.readS32(3, &tmp, 30);
        m_volume = tmp / 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);
        d.readS32(6, &tmp, 10);
        m_volumeIn = tmp / 10.0;
        d.readS32(7, &m_spanLog2, 3);
        d.readBool(11, &m_agc, false);
        d.readString(16, &m_title, "SSB Demodulator");
        d.readString(17, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(18, &m_useReverseAPI, false);
        d.readString(19, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(20, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(21, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(22, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        d.readS32(23, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) FreeDVMode::FreeDVMode700D)) {
            m_freeDVMode = FreeDVMode::FreeDVMode2400A;
        } else {
            m_freeDVMode = (FreeDVMode) tmp;
        }

        d.readS32(24, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(25, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(26, &m_workspaceIndex, 0);
        d.readBlob(27, &m_geometryBytes);
        d.readBool(28, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int FreeDVDemodSettings::getHiCutoff(FreeDVMode freeDVMode)
{
    switch(freeDVMode)
    {
        case FreeDVMode800XA: // C4FM NB
        case FreeDVMode700C: // OFDM
        case FreeDVMode700D: // OFDM
        case FreeDVMode1600: // OFDM
            return 2400.0;
            break;
        case FreeDVMode2400A: // C4FM WB
        default:
            return 6000.0;
            break;
    }
}

int FreeDVDemodSettings::getLowCutoff(FreeDVMode freeDVMode)
{
    switch(freeDVMode)
    {
        case FreeDVMode800XA: // C4FM NB
            return 400.0;
            break;
        case FreeDVMode700C: // OFDM
        case FreeDVMode700D: // OFDM
        case FreeDVMode1600: // OFDM
            return 600.0;
            break;
        case FreeDVMode2400A: // C4FM WB
        default:
            return 0.0;
            break;
    }
}

int FreeDVDemodSettings::getModSampleRate(FreeDVMode freeDVMode)
{
    if (freeDVMode == FreeDVMode2400A) {
        return 48000;
    } else {
        return 8000;
    }
}
