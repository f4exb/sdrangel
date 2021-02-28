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
    m_correlationThreshold = 10.0f; //<! ones/zero powers correlation threshold in dB
    m_samplesPerBit = 4;
    m_removeTimeout = 60;
    m_feedEnabled = false;
    m_feedHost = "feed.adsbexchange.com";
    m_feedPort = 30005;
    m_feedFormat = BeastBinary;
    m_rgbColor = QColor(244, 151, 57).rgb();
    m_title = "ADS-B Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_airportRange = 100;
    m_airportMinimumSize = AirportType::Medium;
    m_displayHeliports = false;
    m_flightPaths = true;
    m_allFlightPaths = false;
    m_siUnits = false;
    m_tableFontName = "Liberation Sans";
    m_tableFontSize = 9;
    m_displayDemodStats = false;
    m_correlateFullPreamble = true;
    m_demodModeS = false;
    m_deviceIndex = -1;
    m_autoResizeTableColumns = false;
    m_interpolatorPhaseSteps = 4;      // Higher than these two values will struggle to run in real-time
    m_interpolatorTapsPerPhase = 3.5f; // without gaining much improvement in PER
    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray ADSBDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_correlationThreshold);
    s.writeS32(4, m_samplesPerBit);
    s.writeS32(5, m_removeTimeout);
    s.writeBool(6, m_feedEnabled);
    s.writeString(7, m_feedHost);
    s.writeU32(8, m_feedPort);

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

    s.writeFloat(18, m_airportRange);
    s.writeS32(19, (int)m_airportMinimumSize);
    s.writeBool(20, m_displayHeliports);
    s.writeBool(21, m_flightPaths);
    s.writeS32(22, m_deviceIndex);
    s.writeBool(23, m_siUnits);
    s.writeS32(24, (int)m_feedFormat);
    s.writeString(25, m_tableFontName);
    s.writeS32(26, m_tableFontSize);
    s.writeBool(27, m_displayDemodStats);
    s.writeBool(28, m_correlateFullPreamble);
    s.writeBool(29, m_demodModeS);
    s.writeBool(30, m_autoResizeTableColumns);
    s.writeS32(31, m_interpolatorPhaseSteps);
    s.writeFloat(32, m_interpolatorTapsPerPhase);
    s.writeBool(33, m_allFlightPaths);

    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
        s.writeS32(100 + i, m_columnIndexes[i]);
    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
        s.writeS32(200 + i, m_columnSizes[i]);

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
        d.readReal(3, &m_correlationThreshold, 0.0f);
        d.readS32(4, &m_samplesPerBit, 4);
        d.readS32(5, &m_removeTimeout, 60);
        d.readBool(6, &m_feedEnabled, false);
        d.readString(7, &m_feedHost, "feed.adsbexchange.com");
        d.readU32(8, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_feedPort = utmp;
        } else {
            m_feedPort = 30005;
        }

        d.readU32(9, &m_rgbColor, QColor(244, 151, 57).rgb());
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

        d.readFloat(18, &m_airportRange, 100);
        d.readS32(19, (int *)&m_airportMinimumSize, AirportType::Medium);
        d.readBool(20, &m_displayHeliports, false);
        d.readBool(21, &m_flightPaths, true);
        d.readS32(22, &m_deviceIndex, -1);
        d.readBool(23, &m_siUnits, false);
        d.readS32(24, (int *) &m_feedFormat, BeastBinary);
        d.readString(25, &m_tableFontName, "Liberation Sans");
        d.readS32(26, &m_tableFontSize, 9);
        d.readBool(27, &m_displayDemodStats, false);
        d.readBool(28, &m_correlateFullPreamble, true);
        d.readBool(29, &m_demodModeS, false);
        d.readBool(30, &m_autoResizeTableColumns, false);
        d.readS32(31, &m_interpolatorPhaseSteps, 4);
        d.readFloat(32, &m_interpolatorTapsPerPhase, 3.5f);
        d.readBool(33, &m_allFlightPaths, false);

        for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
            d.readS32(100 + i, &m_columnIndexes[i], i);
        for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
            d.readS32(200 + i, &m_columnSizes[i], -1);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
