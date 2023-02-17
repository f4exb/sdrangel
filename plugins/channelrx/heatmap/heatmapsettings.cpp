///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "heatmapsettings.h"

HeatMapSettings::HeatMapSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void HeatMapSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 16000.0f;
    m_minPower = -100.0f;
    m_maxPower = 0.0f;
    m_colorMapName = "Jet";
    m_mode = Average;
    m_pulseThreshold= -50.0f;
    m_averagePeriodUS = 100000;
    m_sampleRate = 100;
    m_txPosValid = false;
    m_txLatitude = 0.0f;
    m_txLongitude = 0.0f;
    m_txPower = 0.0f;
    m_displayChart = true;
    m_displayAverage = true;
    m_displayMax = true;
    m_displayMin = true;
    m_displayPulseAverage = true;
    m_displayPathLoss = true;
    m_displayMins = 2;
    m_rgbColor = QColor(102, 40, 220).rgb();
    m_title = "Heat Map";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray HeatMapSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeFloat(3, m_minPower);
    s.writeFloat(4, m_maxPower);
    s.writeString(5, m_colorMapName);
    s.writeS32(6, (int)m_mode);
    s.writeFloat(7, m_pulseThreshold);
    s.writeS32(8, m_averagePeriodUS);
    s.writeS32(9, m_sampleRate);
    s.writeBool(10, m_txPosValid);
    s.writeFloat(11, m_txLatitude);
    s.writeFloat(12, m_txLongitude);
    s.writeFloat(13, m_txPower);
    s.writeBool(14, m_displayChart);
    s.writeBool(15, m_displayAverage);
    s.writeBool(16, m_displayMax);
    s.writeBool(17, m_displayMin);
    s.writeBool(18, m_displayPulseAverage);
    s.writeBool(19, m_displayPathLoss);
    s.writeS32(20, m_displayMins);

    s.writeU32(21, m_rgbColor);
    s.writeString(22, m_title);

    if (m_channelMarker) {
        s.writeBlob(23, m_channelMarker->serialize());
    }

    s.writeS32(24, m_streamIndex);
    s.writeBool(25, m_useReverseAPI);
    s.writeString(26, m_reverseAPIAddress);
    s.writeU32(27, m_reverseAPIPort);
    s.writeU32(28, m_reverseAPIDeviceIndex);
    s.writeU32(29, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(30, m_rollupState->serialize());
    }

    s.writeS32(32, m_workspaceIndex);
    s.writeBlob(33, m_geometryBytes);
    s.writeBool(34, m_hidden);

    return s.final();
}

bool HeatMapSettings::deserialize(const QByteArray& data)
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
        uint32_t utmp;
        QString strtmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readFloat(2, &m_rfBandwidth, 16000.0f);
        d.readFloat(3, &m_minPower, -100.0f);
        d.readFloat(4, &m_maxPower, 0.0f);
        d.readString(5, &m_colorMapName, "Jet");
        d.readS32(6, (int*)&m_mode, (int)Average);
        d.readFloat(7, &m_pulseThreshold, 50.0f);
        d.readS32(8, &m_averagePeriodUS, 100000);
        d.readS32(9, &m_sampleRate, 100);
        d.readBool(10, &m_txPosValid, false);
        d.readFloat(11, &m_txLatitude);
        d.readFloat(12, &m_txLongitude);
        d.readFloat(13, &m_txPower);
        d.readBool(14, &m_displayChart, true);
        d.readBool(15, &m_displayAverage, true);
        d.readBool(16, &m_displayMax, true);
        d.readBool(17, &m_displayMin, true);
        d.readBool(18, &m_displayPulseAverage, true);
        d.readBool(19, &m_displayPathLoss, true);
        d.readS32(20, &m_displayMins, 2);

        d.readU32(21, &m_rgbColor, QColor(102, 40, 220).rgb());
        d.readString(22, &m_title, "Heat Map");

        if (m_channelMarker)
        {
            d.readBlob(23, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(24, &m_streamIndex, 0);
        d.readBool(25, &m_useReverseAPI, false);
        d.readString(26, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(27, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(28, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(29, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(30, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(31, &m_workspaceIndex, 0);
        d.readBlob(32, &m_geometryBytes);
        d.readBool(33, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

