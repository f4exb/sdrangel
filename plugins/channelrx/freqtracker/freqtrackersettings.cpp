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
#include "freqtrackersettings.h"

FreqTrackerSettings::FreqTrackerSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void FreqTrackerSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 6000;
    m_log2Decim = 0;
    m_squelch = -40.0;
    m_rgbColor = QColor(200, 244, 66).rgb();
    m_title = "Frequency Tracker";
    m_spanLog2 = 0;
    m_alphaEMA = 0.1;
    m_tracking = false;
    m_trackerType = TrackerFLL;
    m_pllPskOrder = 2; // BPSK
    m_rrc = false;
    m_rrcRolloff = 35;
    m_squelchGate = 5; // 50 ms
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray FreqTrackerSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth/100);
    s.writeU32(3, m_log2Decim);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }
    s.writeS32(5, m_squelch);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeFloat(8, m_alphaEMA);
    s.writeString(9, m_title);
    s.writeBool(10, m_tracking);
    s.writeS32(11, m_spanLog2);
    s.writeS32(12, (int) m_trackerType);
    s.writeU32(13, m_pllPskOrder);
    s.writeBool(14, m_rrc);
    s.writeU32(15, m_rrcRolloff);
    s.writeBool(16, m_useReverseAPI);
    s.writeString(17, m_reverseAPIAddress);
    s.writeU32(18, m_reverseAPIPort);
    s.writeU32(19, m_reverseAPIDeviceIndex);
    s.writeU32(20, m_reverseAPIChannelIndex);
    s.writeS32(21, m_squelchGate);
    s.writeS32(22, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(23, m_rollupState->serialize());
    }

    s.writeS32(24, m_workspaceIndex);
    s.writeBlob(25, m_geometryBytes);
    s.writeBool(26, m_hidden);

    return s.final();
}

bool FreqTrackerSettings::deserialize(const QByteArray& data)
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
        float ftmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &tmp, 4);
        m_rfBandwidth = 100 * tmp;
        d.readU32(3, &utmp, 0);
        m_log2Decim = utmp > 6 ? 6 : utmp;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readS32(5, &tmp, -40);
        m_squelch = tmp;

        if (m_channelMarker)
        {
            d.readBlob(6, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(7, &m_rgbColor, QColor(200, 244, 66).rgb());
        d.readFloat(8, &ftmp, 0.1);
        m_alphaEMA = ftmp < 0.01 ? 0.01 : ftmp > 1.0 ? 1.0 : ftmp;
        d.readString(9, &m_title, "Frequency Tracker");
        d.readBool(10, &m_tracking, false);
        d.readS32(11, &m_spanLog2, 0);
        d.readS32(12, &tmp, 0);
        m_trackerType = tmp < 0 ? TrackerFLL : tmp > 2 ? TrackerPLL : (TrackerType) tmp;
        d.readU32(13, &utmp, 2);
        m_pllPskOrder = utmp > 32 ? 32 : utmp;
        d.readBool(14, &m_rrc, false);
        d.readU32(15, &utmp, 35);
        m_rrcRolloff = utmp > 100 ? 100 : utmp;
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
        d.readS32(21, &tmp, 5);
        m_squelchGate = tmp < 0 ? 0 : tmp > 99 ? 99 : tmp;
        d.readS32(22, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(23, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(24, &m_workspaceIndex, 0);
        d.readBlob(25, &m_geometryBytes);
        d.readBool(26, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
