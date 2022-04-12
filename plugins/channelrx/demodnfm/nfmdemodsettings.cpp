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

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "nfmdemodsettings.h"

// Standard channel spacings (kHz) using Carson rule
// beta based ............  11F3   16F3  (36F9)
//  5     6.25  7.5   8.33  12.5   25     40     Spacing
//  0.43  0.43  0.43  0.43  0.83   1.67   1.0    Beta
const int NFMDemodSettings::m_channelSpacings[] = {
    5000, 6250, 7500, 8333, 12500, 25000, 40000
};
const int NFMDemodSettings::m_rfBW[] = {  // RF bandwidth (Hz)
    4800, 6000, 7200, 8000, 11000, 16000, 36000
};
const int NFMDemodSettings::m_afBW[] = {  // audio bandwidth (Hz)
    1700, 2100, 2500, 2800,  3000,  3000,  9000
};
const int NFMDemodSettings::m_fmDev[] = { // peak deviation (Hz) - full is double
     731,  903, 1075, 1204,  2500,  5000,  9000
};
const int NFMDemodSettings::m_nbChannelSpacings = 7;

NFMDemodSettings::NFMDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void NFMDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500;
    m_afBandwidth = 3000;
    m_fmDeviation = 5000;
    m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
    m_deltaSquelch = false;
    m_squelch = -30.0;
    m_volume = 1.0;
    m_ctcssOn = false;
    m_audioMute = false;
    m_ctcssIndex = 0;
    m_dcsOn = false;
    m_dcsCode = 0023;
    m_dcsPositive = false;
    m_rgbColor = QColor(255, 0, 0).rgb();
    m_title = "NFM Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_highPass = true;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray NFMDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_afBandwidth);
    s.writeS32(4, m_volume*10.0);
    s.writeS32(5, static_cast<int>(m_squelch));
    s.writeBool(6, m_highPass);
    s.writeU32(7, m_rgbColor);
    s.writeS32(8, m_ctcssIndex);
    s.writeBool(9, m_ctcssOn);
    s.writeBool(10, m_audioMute);
    s.writeS32(11, m_squelchGate);
    s.writeBool(12, m_deltaSquelch);

    if (m_channelMarker) {
        s.writeBlob(13, m_channelMarker->serialize());
    }

    s.writeString(14, m_title);
    s.writeString(15, m_audioDeviceName);
    s.writeBool(16, m_useReverseAPI);
    s.writeString(17, m_reverseAPIAddress);
    s.writeU32(18, m_reverseAPIPort);
    s.writeU32(19, m_reverseAPIDeviceIndex);
    s.writeU32(20, m_reverseAPIChannelIndex);
    s.writeS32(21, m_streamIndex);
    s.writeReal(22, m_fmDeviation);
    s.writeBool(23, m_dcsOn);
    s.writeU32(24, m_dcsCode);
    s.writeBool(25, m_dcsPositive);

    if (m_rollupState) {
        s.writeBlob(26, m_rollupState->serialize());
    }

    s.writeS32(27, m_workspaceIndex);
    s.writeBlob(28, m_geometryBytes);
    s.writeBool(29, m_hidden);

    return s.final();
}

bool NFMDemodSettings::deserialize(const QByteArray& data)
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
        qint32 tmp;
        uint32_t utmp;

        if (m_channelMarker)
        {
            d.readBlob(13, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 12500.0);
        d.readReal(3, &m_afBandwidth, 3000.0);
        d.readS32(4, &tmp, 20);
        m_volume = tmp / 10.0;
        d.readS32(5, &tmp, -30);
        m_squelch = (tmp < -100 ? tmp/10 : tmp) * 1.0;
        d.readBool(6, &m_highPass, true);
        d.readU32(7, &m_rgbColor, QColor(255, 0, 0).rgb());
        d.readS32(8, &m_ctcssIndex, 0);
        d.readBool(9, &m_ctcssOn, false);
        d.readBool(10, &m_audioMute, false);
        d.readS32(11, &m_squelchGate, 5);
        d.readBool(12, &m_deltaSquelch, false);
        d.readString(14, &m_title, "NFM Demodulator");
        d.readString(15, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(16, &m_useReverseAPI, false);
        d.readString(17, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(18, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(19, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(20, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(21, &m_streamIndex, 0);
        d.readReal(22, &m_fmDeviation, 5000.0);
        d.readBool(23, &m_dcsOn, false);
        d.readU32(24, &utmp, 0023);
        m_dcsCode = utmp < 511 ? utmp : 511;
        d.readBool(25, &m_dcsPositive, false);

        if (m_rollupState)
        {
            d.readBlob(26, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(27, &m_workspaceIndex, 0);
        d.readBlob(28, &m_geometryBytes);
        d.readBool(29, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int NFMDemodSettings::getChannelSpacing(int index)
{
    if (index < 0) {
        return m_channelSpacings[0];
    } else if (index < m_nbChannelSpacings) {
        return m_channelSpacings[index];
    } else {
        return m_channelSpacings[m_nbChannelSpacings-1];
    }
}

int NFMDemodSettings::getChannelSpacingIndex(int channelSpacing)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (channelSpacing <= m_channelSpacings[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}

int NFMDemodSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbChannelSpacings) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbChannelSpacings-1];
    }
}

int NFMDemodSettings::getRFBWIndex(int rfbw)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (rfbw <= m_rfBW[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}

int NFMDemodSettings::getAFBW(int index)
{
    if (index < 0) {
        return m_afBW[0];
    } else if (index < m_nbChannelSpacings) {
        return m_afBW[index];
    } else {
        return m_afBW[m_nbChannelSpacings-1];
    }
}

int NFMDemodSettings::getAFBWIndex(int afbw)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (afbw <= m_afBW[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}

int NFMDemodSettings::getFMDev(int index)
{
    if (index < 0) {
        return m_fmDev[0];
    } else if (index < m_nbChannelSpacings) {
        return m_fmDev[index];
    } else {
        return m_fmDev[m_nbChannelSpacings-1];
    }
}

int NFMDemodSettings::getFMDevIndex(int fmDev)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (fmDev <= m_rfBW[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}
