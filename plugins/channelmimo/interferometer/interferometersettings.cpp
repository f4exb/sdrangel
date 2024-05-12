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

#include "interferometersettings.h"

InterferometerSettings::InterferometerSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void InterferometerSettings::resetToDefaults()
{
    m_correlationType = CorrelationAdd;
    m_rgbColor = QColor(128, 128, 128).rgb();
    m_title = "Interferometer";
    m_log2Decim = 0;
    m_filterChainHash = 0;
    m_phase = 0;
    m_gain = 0;
    m_localDeviceIndex = -1;
    m_play = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray InterferometerSettings::serialize() const
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
    s.writeS32(16, m_gain);
    s.writeS32(17, m_localDeviceIndex);

    if (m_spectrumGUI) {
        s.writeBlob(20, m_spectrumGUI->serialize());
    }
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

bool InterferometerSettings::deserialize(const QByteArray& data)
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
        d.readString(4, &m_title, "Interpolator");
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
        d.readS32(16, &m_gain, 0);
        d.readS32(17, &m_localDeviceIndex, -1);

        if (m_spectrumGUI)
        {
            d.readBlob(20, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

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

void InterferometerSettings::applySettings(const QStringList& settingsKeys, const InterferometerSettings& settings)
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
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("localDeviceIndex")) {
        m_localDeviceIndex = settings.m_localDeviceIndex;
    }
    if (settingsKeys.contains("play")) {
        m_play = settings.m_play;
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
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString InterferometerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("correlationType")) {
        ostr << " m_correlationType: " << m_correlationType;
    }
    if (settingsKeys.contains("rgbColor")) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("log2Decim")) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("filterChainHash")) {
        ostr << " m_filterChainHash: " << m_filterChainHash;
    }
    if (settingsKeys.contains("phase")) {
        ostr << " m_phase: " << m_phase;
    }
    if (settingsKeys.contains("gain")) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("localDeviceIndex")) {
        ostr << " m_localDeviceIndex: " << m_localDeviceIndex;
    }
    if (settingsKeys.contains("play") || force) {
        ostr << " m_play: " << m_play;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        ostr << " m_reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden")) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
