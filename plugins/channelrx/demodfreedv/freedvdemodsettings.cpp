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
#include "freedvdemodsettings.h"

#ifdef SDR_RX_SAMPLE_24BIT
const int FreeDVDemodSettings::m_minPowerThresholdDB = -120;
const float FreeDVDemodSettings::m_mminPowerThresholdDBf = 120.0f;
#else
const int FreeDVDemodSettings::m_minPowerThresholdDB = -100;
const float FreeDVDemodSettings::m_mminPowerThresholdDBf = 100.0f;
#endif

FreeDVDemodSettings::FreeDVDemodSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void FreeDVDemodSettings::resetToDefaults()
{
    m_audioMute = false;
    m_agc = true;
    m_volume = 3.0;
    m_volumeIn = 1.0;
    m_spanLog2 = 3;
    m_inputFrequencyOffset = 0;
    m_rgbColor = QColor(0, 255, 204).rgb();
    m_title = "FreeDV Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_freeDVMode = FreeDVMode2400A;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray FreeDVDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(3, m_volume * 10.0);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);
    s.writeS32(6, m_volumeIn * 10.0);
    s.writeS32(7, m_spanLog2);
    s.writeBool(11, m_agc);
    s.writeString(16, m_title);
    s.writeString(17, m_audioDeviceName);
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIDeviceIndex);
    s.writeU32(22, m_reverseAPIChannelIndex);
    s.writeS32(23, (int) m_freeDVMode);
    s.writeS32(24, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(25, m_rollupState->serialize());
    }

    s.writeS32(26, m_workspaceIndex);
    s.writeBlob(27, m_geometryBytes);
    s.writeBool(28, m_hidden);

    return s.final();
}

bool FreeDVDemodSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &tmp, 30);
        d.readS32(3, &tmp, 30);
        m_volume = tmp / 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);
        d.readS32(6, &tmp, 10);
        m_volumeIn = tmp / 10.0;
        d.readS32(7, &m_spanLog2, 3);
        d.readBool(11, &m_agc, false);
        d.readString(16, &m_title, "SSB Demodulator");
        d.readString(17, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(18, &m_useReverseAPI, false);
        d.readString(19, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(20, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(21, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(22, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        d.readS32(23, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) FreeDVMode::FreeDVMode700D)) {
            m_freeDVMode = FreeDVMode::FreeDVMode2400A;
        } else {
            m_freeDVMode = (FreeDVMode) tmp;
        }

        d.readS32(24, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(25, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(26, &m_workspaceIndex, 0);
        d.readBlob(27, &m_geometryBytes);
        d.readBool(28, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void FreeDVDemodSettings::applySettings(const QStringList& settingsKeys, const FreeDVDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("volumeIn")) {
        m_volumeIn = settings.m_volumeIn;
    }
    if (settingsKeys.contains("spanLog2")) {
        m_spanLog2 = settings.m_spanLog2;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("agc")) {
        m_agc = settings.m_agc;
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
    if (settingsKeys.contains("freeDVMode")) {
        m_freeDVMode = settings.m_freeDVMode;
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

QString FreeDVDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("volume") || force) {
        ostr << " m_volume: " << m_volume;
    }
    if (settingsKeys.contains("volumeIn") || force) {
        ostr << " m_volumeIn: " << m_volumeIn;
    }
    if (settingsKeys.contains("spanLog2") || force) {
        ostr << " m_spanLog2: " << m_spanLog2;
    }
    if (settingsKeys.contains("audioMute") || force) {
        ostr << " m_audioMute: " << m_audioMute;
    }
    if (settingsKeys.contains("agc") || force) {
        ostr << " m_agc: " << m_agc;
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
    if (settingsKeys.contains("freeDVMode") || force) {
        ostr << " m_freeDVMode: " << m_freeDVMode;
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

int FreeDVDemodSettings::getHiCutoff(FreeDVMode freeDVMode)
{
    switch(freeDVMode)
    {
        case FreeDVMode800XA: // C4FM NB
        case FreeDVMode700C: // OFDM
        case FreeDVMode700D: // OFDM
        case FreeDVMode1600: // OFDM
            return 2400.0;
            break;
        case FreeDVMode2400A: // C4FM WB
        default:
            return 6000.0;
            break;
    }
}

int FreeDVDemodSettings::getLowCutoff(FreeDVMode freeDVMode)
{
    switch(freeDVMode)
    {
        case FreeDVMode800XA: // C4FM NB
            return 400.0;
            break;
        case FreeDVMode700C: // OFDM
        case FreeDVMode700D: // OFDM
        case FreeDVMode1600: // OFDM
            return 600.0;
            break;
        case FreeDVMode2400A: // C4FM WB
        default:
            return 0.0;
            break;
    }
}

int FreeDVDemodSettings::getModSampleRate(FreeDVMode freeDVMode)
{
    if (freeDVMode == FreeDVMode2400A) {
        return 48000;
    } else {
        return 8000;
    }
}
