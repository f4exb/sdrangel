///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

void FreqTrackerSettings::applySettings(const QStringList& settingsKeys, const FreqTrackerSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("squelch")) {
        m_squelch = settings.m_squelch;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("spanLog2")) {
        m_spanLog2 = settings.m_spanLog2;
    }
    if (settingsKeys.contains("alphaEMA")) {
        m_alphaEMA = settings.m_alphaEMA;
    }
    if (settingsKeys.contains("tracking")) {
        m_tracking = settings.m_tracking;
    }
    if (settingsKeys.contains("trackerType")) {
        m_trackerType = settings.m_trackerType;
    }
    if (settingsKeys.contains("pllPskOrder")) {
        m_pllPskOrder = settings.m_pllPskOrder;
    }
    if (settingsKeys.contains("rrc")) {
        m_rrc = settings.m_rrc;
    }
    if (settingsKeys.contains("rrcRolloff")) {
        m_rrcRolloff = settings.m_rrcRolloff;
    }
    if (settingsKeys.contains("squelchGate")) {
        m_squelchGate = settings.m_squelchGate;
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

QString FreqTrackerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("squelch") || force) {
        ostr << " m_squelch: " << m_squelch;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("spanLog2") || force) {
        ostr << " m_spanLog2: " << m_spanLog2;
    }
    if (settingsKeys.contains("alphaEMA") || force) {
        ostr << " m_alphaEMA: " << m_alphaEMA;
    }
    if (settingsKeys.contains("tracking") || force) {
        ostr << " m_tracking: " << m_tracking;
    }
    if (settingsKeys.contains("trackerType") || force) {
        ostr << " m_trackerType: " << m_trackerType;
    }
    if (settingsKeys.contains("pllPskOrder") || force) {
        ostr << " m_pllPskOrder: " << m_pllPskOrder;
    }
    if (settingsKeys.contains("rrc") || force) {
        ostr << " m_rrc: " << m_rrc;
    }
    if (settingsKeys.contains("rrcRolloff") || force) {
        ostr << " m_rrcRolloff: " << m_rrcRolloff;
    }
    if (settingsKeys.contains("squelchGate") || force) {
        ostr << " m_squelchGate: " << m_squelchGate;
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
