///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
    m_sampleRate = 100000;
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
    m_recordAverage = true;
    m_recordMax = true;
    m_recordMin = true;
    m_recordPulseAverage = true;
    m_recordPathLoss = true;
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
    s.writeBool(40, m_recordAverage);
    s.writeBool(41, m_recordMax);
    s.writeBool(42, m_recordMin);
    s.writeBool(43, m_recordPulseAverage);
    s.writeBool(44, m_recordPathLoss);

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

    s.writeS32(31, m_workspaceIndex);
    s.writeBlob(32, m_geometryBytes);
    s.writeBool(33, m_hidden);

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
        d.readS32(9, &m_sampleRate, 100000);
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
        d.readBool(40, &m_recordAverage, true);
        d.readBool(41, &m_recordMax, true);
        d.readBool(42, &m_recordMin, true);
        d.readBool(43, &m_recordPulseAverage, true);
        d.readBool(44, &m_recordPathLoss, true);

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

void HeatMapSettings::applySettings(const QStringList& settingsKeys, const HeatMapSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("minPower")) {
        m_minPower = settings.m_minPower;
    }
    if (settingsKeys.contains("maxPower")) {
        m_maxPower = settings.m_maxPower;
    }
    if (settingsKeys.contains("colorMapName")) {
        m_colorMapName = settings.m_colorMapName;
    }
    if (settingsKeys.contains("mode")) {
        m_mode = settings.m_mode;
    }
    if (settingsKeys.contains("pulseThreshold")) {
        m_pulseThreshold = settings.m_pulseThreshold;
    }
    if (settingsKeys.contains("averagePeriodUS")) {
        m_averagePeriodUS = settings.m_averagePeriodUS;
    }
    if (settingsKeys.contains("sampleRate")) {
        m_sampleRate = settings.m_sampleRate;
    }
    if (settingsKeys.contains("txPosValid")) {
        m_txPosValid = settings.m_txPosValid;
    }
    if (settingsKeys.contains("txLatitude")) {
        m_txLatitude = settings.m_txLatitude;
    }
    if (settingsKeys.contains("txLongitude")) {
        m_txLongitude = settings.m_txLongitude;
    }
    if (settingsKeys.contains("txPower")) {
        m_txPower = settings.m_txPower;
    }
    if (settingsKeys.contains("displayChart")) {
        m_displayChart = settings.m_displayChart;
    }
    if (settingsKeys.contains("displayAverage")) {
        m_displayAverage = settings.m_displayAverage;
    }
    if (settingsKeys.contains("displayMax")) {
        m_displayMax = settings.m_displayMax;
    }
    if (settingsKeys.contains("displayMin")) {
        m_displayMin = settings.m_displayMin;
    }
    if (settingsKeys.contains("displayPulseAverage")) {
        m_displayPulseAverage = settings.m_displayPulseAverage;
    }
    if (settingsKeys.contains("displayPathLoss")) {
        m_displayPathLoss = settings.m_displayPathLoss;
    }
    if (settingsKeys.contains("displayMins")) {
        m_displayMins = settings.m_displayMins;
    }
    if (settingsKeys.contains("recordAverage")) {
        m_recordAverage = settings.m_recordAverage;
    }
    if (settingsKeys.contains("recordMax")) {
        m_recordMax = settings.m_recordMax;
    }
    if (settingsKeys.contains("recordMin")) {
        m_recordMin = settings.m_recordMin;
    }
    if (settingsKeys.contains("recordPulseAverage")) {
        m_recordPulseAverage = settings.m_recordPulseAverage;
    }
    if (settingsKeys.contains("recordPathLoss")) {
        m_recordPathLoss = settings.m_recordPathLoss;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString HeatMapSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("minPower") || force) {
        ostr << " m_minPower: " << m_minPower;
    }
    if (settingsKeys.contains("maxPower") || force) {
        ostr << " m_maxPower: " << m_maxPower;
    }
    if (settingsKeys.contains("colorMapName") || force) {
        ostr << " m_colorMapName: " << m_colorMapName.toStdString();
    }
    if (settingsKeys.contains("mode") || force) {
        ostr << " m_mode: " << m_mode;
    }
    if (settingsKeys.contains("pulseThreshold") || force) {
        ostr << " m_pulseThreshold: " << m_pulseThreshold;
    }
    if (settingsKeys.contains("averagePeriodUS") || force) {
        ostr << " m_averagePeriodUS: " << m_averagePeriodUS;
    }
    if (settingsKeys.contains("sampleRate") || force) {
        ostr << " m_sampleRate: " << m_sampleRate;
    }
    if (settingsKeys.contains("txPosValid") || force) {
        ostr << " m_txPosValid: " << m_txPosValid;
    }
    if (settingsKeys.contains("txLatitude") || force) {
        ostr << " m_txLatitude: " << m_txLatitude;
    }
    if (settingsKeys.contains("txLongitude") || force) {
        ostr << " m_txLongitude: " << m_txLongitude;
    }
    if (settingsKeys.contains("txPower") || force) {
        ostr << " m_txPower: " << m_txPower;
    }
    if (settingsKeys.contains("displayChart") || force) {
        ostr << " m_displayChart: " << m_displayChart;
    }
    if (settingsKeys.contains("displayAverage") || force) {
        ostr << " m_displayAverage: " << m_displayAverage;
    }
    if (settingsKeys.contains("displayMax") || force) {
        ostr << " m_displayMax: " << m_displayMax;
    }
    if (settingsKeys.contains("displayMin") || force) {
        ostr << " m_displayMin: " << m_displayMin;
    }
    if (settingsKeys.contains("displayPulseAverage") || force) {
        ostr << " m_displayPulseAverage: " << m_displayPulseAverage;
    }
    if (settingsKeys.contains("displayPathLoss") || force) {
        ostr << " m_displayPathLoss: " << m_displayPathLoss;
    }
    if (settingsKeys.contains("displayMins") || force) {
        ostr << " m_displayMins: " << m_displayMins;
    }
    if (settingsKeys.contains("recordAverage") || force) {
        ostr << " m_recordAverage: " << m_recordAverage;
    }
    if (settingsKeys.contains("recordMax") || force) {
        ostr << " m_recordMax: " << m_recordMax;
    }
    if (settingsKeys.contains("recordMin") || force) {
        ostr << " m_recordMin: " << m_recordMin;
    }
    if (settingsKeys.contains("recordPulseAverage") || force) {
        ostr << " m_recordPulseAverage: " << m_recordPulseAverage;
    }
    if (settingsKeys.contains("recordPathLoss") || force) {
        ostr << " m_recordPathLoss: " << m_recordPathLoss;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex") || force) {
        ostr << " m_reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
