///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free som_udpCopyAudioftware; you can redistribute it and/or modify          //
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

#include "m17demodsettings.h"

M17DemodSettings::M17DemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void M17DemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500.0;
    m_fmDeviation = 3500.0;
    m_volume = 2.0;
    m_baudRate = 4800;
    m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
    m_squelch = -40.0;
    m_audioMute = false;
    m_syncOrConstellation = false;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "M17 Demodulator";
    m_highPassFilter = false;
    m_traceLengthMutliplier = 6; // 300 ms
    m_traceStroke = 100;
    m_traceDecay = 200;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_statusLogEnabled = false;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray M17DemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth/100.0);
    s.writeS32(4, m_fmDeviation/100.0);
    s.writeS32(5, m_squelch);
    s.writeU32(7, m_rgbColor);
    s.writeS32(8, m_squelchGate);
    s.writeS32(9, m_volume*10.0);
    s.writeS32(11, m_baudRate);
    s.writeBool(12, m_statusLogEnabled);
    s.writeBool(13, m_syncOrConstellation);

    if (m_channelMarker) {
        s.writeBlob(17, m_channelMarker->serialize());
    }

    s.writeString(18, m_title);
    s.writeBool(19, m_highPassFilter);
    s.writeString(20, m_audioDeviceName);
    s.writeS32(21, m_traceLengthMutliplier);
    s.writeS32(22, m_traceStroke);
    s.writeS32(23, m_traceDecay);
    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIDeviceIndex);
    s.writeU32(28, m_reverseAPIChannelIndex);
    s.writeBool(29, m_audioMute);
    s.writeS32(30, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(31, m_rollupState->serialize());
    }

    s.writeS32(32, m_workspaceIndex);
    s.writeBlob(33, m_geometryBytes);
    s.writeBool(34, m_hidden);

    return s.final();
}

bool M17DemodSettings::deserialize(const QByteArray& data)
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
        QString strtmp;
        qint32 tmp;
        uint32_t utmp;

        if (m_channelMarker)
        {
            d.readBlob(17, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &tmp, 125);
        m_rfBandwidth = tmp * 100.0;
        d.readS32(4, &tmp, 50);
        m_fmDeviation = tmp * 100.0;
        d.readS32(5, &tmp, -40);
        m_squelch = tmp < -100 ? tmp / 10.0 : tmp;
        d.readU32(7, &m_rgbColor);
        d.readS32(8, &m_squelchGate, 5);
        d.readS32(9, &tmp, 20);
        m_volume = tmp / 10.0;
        d.readS32(11, &m_baudRate, 4800);
        d.readBool(12, &m_statusLogEnabled, false);
        d.readBool(13, &m_syncOrConstellation, false);
        d.readString(18, &m_title, "M17 Demodulator");
        d.readBool(19, &m_highPassFilter, false);
        d.readString(20, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(21, &tmp, 6);
        m_traceLengthMutliplier = tmp < 2 ? 2 : tmp > 30 ? 30 : tmp;
        d.readS32(22, &tmp, 100);
        m_traceStroke = tmp < 0 ? 0 : tmp > 255 ? 255 : tmp;
        d.readS32(23, &tmp, 200);
        m_traceDecay = tmp < 0 ? 0 : tmp > 255 ? 255 : tmp;
        d.readBool(24, &m_useReverseAPI, false);
        d.readString(25, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(26, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(27, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(28, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readBool(29, &m_audioMute, false);
        d.readS32(30, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(31, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(32, &m_workspaceIndex, 0);
        d.readBlob(33, &m_geometryBytes);
        d.readBool(34, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void M17DemodSettings::applySettings(const QStringList& settingsKeys, const M17DemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (settingsKeys.contains("squelch")) {
        m_squelch = settings.m_squelch;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("squelchGate")) {
        m_squelchGate = settings.m_squelchGate;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("baudRate")) {
        m_baudRate = settings.m_baudRate;
    }
    if (settingsKeys.contains("statusLogEnabled")) {
        m_statusLogEnabled = settings.m_statusLogEnabled;
    }
    if (settingsKeys.contains("syncOrConstellation")) {
        m_syncOrConstellation = settings.m_syncOrConstellation;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("highPassFilter")) {
        m_highPassFilter = settings.m_highPassFilter;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("traceLengthMutliplier")) {
        m_traceLengthMutliplier = settings.m_traceLengthMutliplier;
    }
    if (settingsKeys.contains("traceStroke")) {
        m_traceStroke = settings.m_traceStroke;
    }
    if (settingsKeys.contains("traceDecay")) {
        m_traceDecay = settings.m_traceDecay;
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
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("rollupState")) {
        m_rollupState = settings.m_rollupState;
    }
    if (settingsKeys.contains("channelMarker")) {
        m_channelMarker = settings.m_channelMarker;
    }
}
