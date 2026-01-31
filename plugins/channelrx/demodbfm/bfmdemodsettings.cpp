///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "bfmdemodsettings.h"

const int BFMDemodSettings::m_nbRFBW = 9;
const int BFMDemodSettings::m_rfBW[] = {
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 220000, 250000
};

BFMDemodSettings::BFMDemodSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void BFMDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = getRFBW(5);
    m_afBandwidth = 15000;
    m_volume = 2.0;
    m_squelch = -60.0;
    m_audioMute = false;
    m_audioStereo = false;
    m_lsbStereo = false;
    m_showPilot = false;
    m_rdsActive = false;
    m_deEmphasis = DeEmphasis50us;
    m_rgbColor = QColor(80, 120, 228).rgb();
    m_title = "Broadcast FM Demod";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray BFMDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, getRFBWIndex(m_rfBandwidth));
    s.writeS32(3, m_afBandwidth/1000.0);
    s.writeS32(4, m_volume*10.0);
    s.writeS32(5, m_squelch);
    s.writeU32(7, m_rgbColor);

    if (m_spectrumGUI) {
        s.writeBlob(8, m_spectrumGUI->serialize());
    }

    s.writeBool(9, m_audioStereo);
    s.writeBool(10, m_lsbStereo);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeString(12, m_title);
    s.writeString(13, m_audioDeviceName);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);
    s.writeS32(19, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(20, m_rollupState->serialize());
    }

    s.writeBool(21, m_rdsActive);
    s.writeS32(22, m_workspaceIndex);
    s.writeBlob(23, m_geometryBytes);
    s.writeBool(24, m_hidden);
    s.writeBool(25, m_audioMute);
    s.writeS32(26, m_deEmphasis);

    return s.final();
}

bool BFMDemodSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &tmp, 4);
        m_rfBandwidth = getRFBW(tmp);
        d.readS32(3, &tmp, 3);
        m_afBandwidth = tmp * 1000.0;
        d.readS32(4, &tmp, 20);
        m_volume = tmp * 0.1;
        d.readS32(5, &tmp, -60);
        m_squelch = tmp;
        d.readU32(7, &m_rgbColor);


        if (m_spectrumGUI)
        {
            d.readBlob(8, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readBool(9, &m_audioStereo, false);
        d.readBool(10, &m_lsbStereo, false);

        if (m_channelMarker)
        {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(12, &m_title, "Broadcast FM Demod");
        d.readString(13, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(17, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(18, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(19, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(20, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readBool(21, &m_rdsActive, false);
        d.readS32(22, &m_workspaceIndex, 0);
        d.readBlob(23, &m_geometryBytes);
        d.readBool(24, &m_hidden, false);
        d.readBool(25, &m_audioMute, false);
        d.readS32(26, &tmp, DeEmphasis50us);
        m_deEmphasis = static_cast<DeEmphasis>(tmp);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void BFMDemodSettings::applySettings(const QStringList& settingsKeys, const BFMDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("afBandwidth")) {
        m_afBandwidth = settings.m_afBandwidth;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("squelch")) {
        m_squelch = settings.m_squelch;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("audioStereo")) {
        m_audioStereo = settings.m_audioStereo;
    }
    if (settingsKeys.contains("lsbStereo")) {
        m_lsbStereo = settings.m_lsbStereo;
    }
    if (settingsKeys.contains("showPilot")) {
        m_showPilot = settings.m_showPilot;
    }
    if (settingsKeys.contains("rdsActive")) {
        m_rdsActive = settings.m_rdsActive;
    }
    if (settingsKeys.contains("deEmphasis")) {
        m_deEmphasis = settings.m_deEmphasis;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
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
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString BFMDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("afBandwidth") || force) {
        ostr << " m_afBandwidth: " << m_afBandwidth;
    }
    if (settingsKeys.contains("volume") || force) {
        ostr << " m_volume: " << m_volume;
    }
    if (settingsKeys.contains("squelch") || force) {
        ostr << " m_squelch: " << m_squelch;
    }
    if (settingsKeys.contains("audioMute") || force) {
        ostr << " m_audioMute: " << m_audioMute;
    }
    if (settingsKeys.contains("audioStereo") || force) {
        ostr << " m_audioStereo: " << m_audioStereo;
    }
    if (settingsKeys.contains("lsbStereo") || force) {
        ostr << " m_lsbStereo: " << m_lsbStereo;
    }
    if (settingsKeys.contains("showPilot") || force) {
        ostr << " m_showPilot: " << m_showPilot;
    }
    if (settingsKeys.contains("rdsActive") || force) {
        ostr << " m_rdsActive: " << m_rdsActive;
    }
    if (settingsKeys.contains("deEmphasis") || force) {
        ostr << " m_deEmphasis: " << m_deEmphasis;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        ostr << " m_audioDeviceName: " << m_audioDeviceName.toStdString();
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

int BFMDemodSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbRFBW) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbRFBW-1];
    }
}

int BFMDemodSettings::getRFBWIndex(int rfbw)
{
    for (int i = 0; i < m_nbRFBW; i++)
    {
        if (rfbw <= m_rfBW[i])
        {
            return i;
        }
    }

    return m_nbRFBW-1;
}
