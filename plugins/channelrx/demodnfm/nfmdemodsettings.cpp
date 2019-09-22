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

const int NFMDemodSettings::m_rfBW[] = {
    5000, 6250, 8330, 10000, 12500, 15000, 20000, 25000, 40000
};
const int NFMDemodSettings::m_fmDev[] = { // corresponding single side FM deviations at 0.4 * BW
    2000, 2500, 3330, 4000,  5000,  6000,  8000,  10000,  16000
};
const int NFMDemodSettings::m_nbRfBW = 9;

NFMDemodSettings::NFMDemodSettings() :
    m_channelMarker(0)
{
    resetToDefaults();
}

void NFMDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500;
    m_afBandwidth = 3000;
    m_fmDeviation = 2000;
    m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
    m_deltaSquelch = false;
    m_squelch = -30.0;
    m_volume = 1.0;
    m_ctcssOn = false;
    m_audioMute = false;
    m_ctcssIndex = 0;
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
}

QByteArray NFMDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, getRFBWIndex(m_rfBandwidth));
    s.writeS32(3, m_afBandwidth/1000.0);
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
        d.readS32(2, &tmp, 4);
        m_rfBandwidth = getRFBW(tmp);
        m_fmDeviation = getFMDev(tmp);
        d.readS32(3, &tmp, 3);
        m_afBandwidth = tmp * 1000.0;
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

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int NFMDemodSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbRfBW) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbRfBW-1];
    }
}

int NFMDemodSettings::getFMDev(int index)
{
    if (index < 0) {
        return m_fmDev[0];
    } else if (index < m_nbRfBW) {
        return m_fmDev[index];
    } else {
        return m_fmDev[m_nbRfBW-1];
    }
}

int NFMDemodSettings::getRFBWIndex(int rfbw)
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
