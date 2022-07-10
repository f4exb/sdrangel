///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/ctcssfrequencies.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "m17modsettings.h"

M17ModSettings::M17ModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void M17ModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 16000.0f;
    m_fmDeviation = 10000.0f; //!< full deviation
    m_toneFrequency = 1000.0f;
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "M17 Modulator";
    m_m17Mode = M17Mode::M17ModeNone;
    m_audioType = AudioType::AudioNone;
    m_packetType = PacketType::PacketSMS;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_feedbackAudioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_feedbackVolumeFactor = 0.5f;
    m_feedbackAudioEnable = false;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_sourceCall = "";
    m_destCall = "";
    m_insertPosition = false;
    m_can = 10;
    m_loopPacket = false;
    m_loopPacketInterval = 60;
    m_smsText = "";
    m_aprsCallsign = "MYCALL";
    m_aprsTo = "APRS";
    m_aprsVia = "WIDE2-2";
    m_aprsData = ">Using SDRangel";
    m_aprsInsertPosition = 0;
}

QByteArray M17ModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(4, m_fmDeviation);
    s.writeU32(5, m_rgbColor);
    s.writeReal(6, m_toneFrequency);
    s.writeReal(7, m_volumeFactor);
    s.writeS32(8, (int) m_m17Mode);
    s.writeS32(9, (int) m_audioType);
    s.writeS32(10, (int) m_packetType);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeString(12, m_title);
    s.writeString(14, m_audioDeviceName);
    s.writeBool(15, m_useReverseAPI);
    s.writeString(16, m_reverseAPIAddress);
    s.writeU32(17, m_reverseAPIPort);
    s.writeU32(18, m_reverseAPIDeviceIndex);
    s.writeU32(19, m_reverseAPIChannelIndex);
    s.writeString(20, m_feedbackAudioDeviceName);
    s.writeReal(21, m_feedbackVolumeFactor);
    s.writeBool(22, m_feedbackAudioEnable);
    s.writeS32(23, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);

    s.writeString(40, m_sourceCall);
    s.writeString(41, m_destCall);
    s.writeBool(42, m_insertPosition);
    s.writeU32(43, m_can);

    s.writeString(50, m_smsText);
    s.writeBool(51, m_loopPacket);
    s.writeU32(52, m_loopPacketInterval);

    s.writeString(60, m_aprsCallsign);
    s.writeString(61, m_aprsTo);
    s.writeString(62, m_aprsVia);
    s.writeString(63, m_aprsData);
    s.writeBool(64, m_aprsInsertPosition);

    return s.final();
}

bool M17ModSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 12500.0);
        d.readReal(4, &m_fmDeviation, 10000.0);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_toneFrequency, 1000.0);
        d.readReal(7, &m_volumeFactor, 1.0);
        d.readS32(8, &tmp, 0);
        m_m17Mode = tmp < 0 ? M17ModeNone : tmp > (int) M17ModeM17BERT ? M17ModeM17BERT : (M17Mode) tmp;
        d.readS32(9, &tmp, 0);
        m_audioType = tmp < 0 ? AudioNone : tmp > (int) AudioInput ? AudioInput : (AudioType) tmp;
        m_packetType = tmp < 0 ? PacketSMS : tmp > (int) PacketAPRS ? PacketAPRS : (PacketType) tmp;

        d.readBlob(11, &bytetmp);

        if (m_channelMarker)
        {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(12, &m_title, "M17 Modulator");

        d.readString(14, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(15, &m_useReverseAPI, false);
        d.readString(16, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(17, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(18, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(19, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(20, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(21, &m_feedbackVolumeFactor, 1.0);
        d.readBool(22, &m_feedbackAudioEnable, false);
        d.readS32(23, &m_streamIndex, 0);
        d.readS32(25, &tmp, 0023);

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);

        d.readString(40, &m_sourceCall, "");
        d.readString(41, &m_destCall, "");
        d.readBool(42, &m_insertPosition, false);
        d.readU32(43, &utmp);
        m_can = utmp < 255 ? utmp : 255;

        d.readString(50, &m_smsText, "");
        d.readBool(51, &m_loopPacket, false);
        d.readU32(52, &m_loopPacketInterval, 60);

        d.readString(60, &m_aprsCallsign, "MYCALL");
        d.readString(61, &m_aprsTo, "");
        d.readString(62, &m_aprsVia, "");
        d.readString(63, &m_aprsData, "");
        d.readBool(64, &m_aprsInsertPosition, false);

        return true;
    }
    else
    {
        qDebug() << "NFMModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

void M17ModSettings::applySettings(const QStringList& settingsKeys, const M17ModSettings& settings)
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
    if (settingsKeys.contains("toneFrequency")) {
        m_toneFrequency = settings.m_toneFrequency;
    }
    if (settingsKeys.contains("volumeFactor")) {
        m_volumeFactor = settings.m_volumeFactor;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("playLoop")) {
        m_playLoop = settings.m_playLoop;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("m17Mode")) {
        m_m17Mode = settings.m_m17Mode;
    }
    if (settingsKeys.contains("audioType")) {
        m_audioType = settings.m_audioType;
    }
    if (settingsKeys.contains("packetType")) {
        m_packetType = settings.m_packetType;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("feedbackAudioDeviceName")) {
        m_feedbackAudioDeviceName = settings.m_feedbackAudioDeviceName;
    }
    if (settingsKeys.contains("feedbackVolumeFactor")) {
        m_feedbackVolumeFactor = settings.m_feedbackVolumeFactor;
    }
    if (settingsKeys.contains("feedbackAudioEnable")) {
        m_feedbackAudioEnable = settings.m_feedbackAudioEnable;
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
    if (settingsKeys.contains("sourceCall")) {
        m_sourceCall = settings.m_sourceCall;
    }
    if (settingsKeys.contains("destCall")) {
        m_destCall = settings.m_destCall;
    }
    if (settingsKeys.contains("insertPosition")) {
        m_insertPosition = settings.m_insertPosition;
    }
    if (settingsKeys.contains("can")) {
        m_can = settings.m_can;
    }
    if (settingsKeys.contains("smsText")) {
        m_smsText = settings.m_smsText;
    }
    if (settingsKeys.contains("loopPacket")) {
        m_loopPacket = settings.m_loopPacket;
    }
    if (settingsKeys.contains("loopPacketInterval")) {
        m_loopPacketInterval = settings.m_loopPacketInterval;
    }
    if (settingsKeys.contains("aprsCallsign")) {
        m_aprsCallsign = settings.m_aprsCallsign;
    }
    if (settingsKeys.contains("aprsTo")) {
        m_aprsTo = settings.m_aprsTo;
    }
    if (settingsKeys.contains("aprsVia")) {
        m_aprsVia = settings.m_aprsVia;
    }
    if (settingsKeys.contains("aprsData")) {
        m_aprsData = settings.m_aprsData;
    }
    if (settingsKeys.contains("aprsInsertPosition")) {
        m_aprsInsertPosition = settings.m_aprsInsertPosition;
    }
    if (settingsKeys.contains("channelMarker")) {
        m_channelMarker = settings.m_channelMarker;
    }
    if (settingsKeys.contains("rollupState")) {
        m_rollupState = settings.m_rollupState;
    }
}
