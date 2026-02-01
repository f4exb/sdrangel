///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include "doa2settings.h"

DOA2Settings::DOA2Settings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DOA2Settings::resetToDefaults()
{
    m_correlationType = CorrelationFFT;
    m_rgbColor = QColor(250, 120, 120).rgb();
    m_title = "DOA 2 sources";
    m_log2Decim = 0;
    m_filterChainHash = 0;
    m_phase = 0;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_antennaAz = 0;
    m_basebandDistance = 500;
    m_squelchdB = -50;
    m_fftAveragingIndex = 0;
}

QByteArray DOA2Settings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(2, (int) m_correlationType);
    s.writeU32(3, m_rgbColor);
    s.writeString(4, m_title);
    s.writeU32(5, m_log2Decim);
    s.writeU32(6, m_filterChainHash);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIDeviceIndex);
    s.writeU32(11, m_reverseAPIChannelIndex);
    s.writeS32(12, m_phase);
    s.writeS32(13,m_workspaceIndex);
    s.writeBlob(14, m_geometryBytes);
    s.writeBool(15, m_hidden);
    s.writeS32(16, m_antennaAz);
    s.writeU32(17, m_basebandDistance);
    s.writeS32(18, m_squelchdB);
    s.writeS32(19, m_fftAveragingIndex);

    if (m_scopeGUI) {
        s.writeBlob(21, m_scopeGUI->serialize());
    }
    if (m_channelMarker) {
        s.writeBlob(22, m_channelMarker->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(23, m_rollupState->serialize());
    }

    return s.final();
}

bool DOA2Settings::deserialize(const QByteArray& data)
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
        int tmp;
        quint32 utmp;

        d.readS32(2, &tmp, 0);
        m_correlationType = (CorrelationType) tmp;
        d.readU32(3, &m_rgbColor);
        d.readString(4, &m_title, "DOA 2 sources");
        d.readU32(5, &utmp, 0);
        m_log2Decim = utmp > 6 ? 6 : utmp;
        d.readU32(6, &m_filterChainHash, 0);
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(11, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(12, &tmp, 0);
        m_phase = tmp < -180 ? -180 : tmp > 180 ? 180 : tmp;
        d.readS32(13, &m_workspaceIndex);
        d.readBlob(14, &m_geometryBytes);
        d.readBool(15, &m_hidden, false);
        d.readS32(16, &tmp, 0);
        m_antennaAz = tmp < 0 ? 0 : tmp > 359 ? 359 : tmp;
        d.readU32(17, &utmp, 500);
        m_basebandDistance = utmp == 0 ? 1 : utmp;
        d.readS32(18, &m_squelchdB, -50);
        d.readS32(19, &tmp, 0);
        m_fftAveragingIndex = tmp < 0 ?
            0 :
            tmp > 3*m_averagingMaxExponent + 3 ?
                3*m_averagingMaxExponent + 3:
                tmp;

        if (m_scopeGUI)
        {
            d.readBlob(21, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(22, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        if (m_rollupState)
        {
            d.readBlob(23, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void DOA2Settings::applySettings(const QStringList& settingsKeys, const DOA2Settings& settings)
{
    if (settingsKeys.contains("correlationType")) {
        m_correlationType = settings.m_correlationType;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("filterChainHash")) {
        m_filterChainHash = settings.m_filterChainHash;
    }
    if (settingsKeys.contains("phase")) {
        m_phase = settings.m_phase;
    }
    if (settingsKeys.contains("antennaAz")) {
        m_antennaAz = settings.m_antennaAz;
    }
    if (settingsKeys.contains("basebandDistance")) {
        m_basebandDistance = settings.m_basebandDistance;
    }
    if (settingsKeys.contains("squelchdB")) {
        m_squelchdB = settings.m_squelchdB;
    }
    if (settingsKeys.contains("fftAveragingIndex")) {
        m_fftAveragingIndex = settings.m_fftAveragingIndex;
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

QString DOA2Settings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("correlationType") || force) {
        ostr << " m_correlationType: " << (int)m_correlationType;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("filterChainHash") || force) {
        ostr << " m_filterChainHash: " << m_filterChainHash;
    }
    if (settingsKeys.contains("phase") || force) {
        ostr << " m_phase: " << m_phase;
    }
    if (settingsKeys.contains("antennaAz") || force) {
        ostr << " m_antennaAz: " << m_antennaAz;
    }
    if (settingsKeys.contains("basebandDistance") || force) {
        ostr << " m_basebandDistance: " << m_basebandDistance;
    }
    if (settingsKeys.contains("squelchdB") || force) {
        ostr << " m_squelchdB: " << m_squelchdB;
    }
    if (settingsKeys.contains("fftAveragingIndex") || force) {
        ostr << " m_fftAveragingIndex: " << m_fftAveragingIndex;
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

int DOA2Settings::getAveragingValue(int averagingIndex)
{
    if (averagingIndex <= 0) {
        return 1;
    }

    int v = averagingIndex - 1;
    int m = pow(10.0, v/3 > m_averagingMaxExponent ? m_averagingMaxExponent : v/3);
    int x = 1;

    if (v % 3 == 0) {
        x = 2;
    } else if (v % 3 == 1) {
        x = 5;
    } else if (v % 3 == 2) {
        x = 10;
    }

    return x * m;
}

int DOA2Settings::getAveragingIndex(int averagingValue)
{
    if (averagingValue <= 1) {
        return 0;
    }

    int v = averagingValue;
    int j = 0;

    for (int i = 0; i <= m_averagingMaxExponent; i++)
    {
        if (v < 20)
        {
            if (v < 2) {
                j = 0;
            } else if (v < 5) {
                j = 1;
            } else if (v < 10) {
                j = 2;
            } else {
                j = 3;
            }

            return 3*i + j;
        }

        v /= 10;
    }

    return 3*m_averagingMaxExponent + 3;
}
