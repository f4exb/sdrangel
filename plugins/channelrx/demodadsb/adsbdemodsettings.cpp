///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "adsbdemodsettings.h"

ADSBDemodSettings::ADSBDemodSettings() :
    m_channelMarker(0)
{
    resetToDefaults();
}

void ADSBDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 2*1300000;
    m_correlationThreshold = 0.0f;
    m_samplesPerBit = 6;
    m_removeTimeout = 60;
    m_beastEnabled = false;
    m_beastHost = "feed.adsbexchange.com";
    m_beastPort = 30005;
    m_rgbColor = QColor(255, 0, 0).rgb();
    m_title = "ADS-B Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
}

QByteArray ADSBDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_correlationThreshold);
    s.writeS32(4, m_samplesPerBit);
    s.writeS32(5, m_removeTimeout);
    s.writeBool(6, m_beastEnabled);
    s.writeString(7, m_beastHost);
    s.writeU32(8, m_beastPort);

    s.writeU32(9, m_rgbColor);
    if (m_channelMarker) {
        s.writeBlob(10, m_channelMarker->serialize());
    }
    s.writeString(11, m_title);
    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);
    s.writeU32(16, m_reverseAPIChannelIndex);
    s.writeS32(17, m_streamIndex);

    return s.final();
}

bool ADSBDemodSettings::deserialize(const QByteArray& data)
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
            d.readBlob(10, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 2*1300000);
        d.readReal(3, &m_correlationThreshold, 1.0f);
        d.readS32(4, &m_samplesPerBit, 6);
        d.readS32(5, &m_removeTimeout, 60);
        d.readBool(6, &m_beastEnabled, false);
        d.readString(7, &m_beastHost, "feed.adsbexchange.com");
        d.readU32(8, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_beastPort = utmp;
        } else {
            m_beastPort = 30005;
        }

        d.readU32(9, &m_rgbColor, QColor(255, 0, 0).rgb());
        d.readString(11, &m_title, "ADS-B Demodulator");
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
        d.readS32(17, &m_streamIndex, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
