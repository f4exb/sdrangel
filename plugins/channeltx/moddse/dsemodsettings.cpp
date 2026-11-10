///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
#include <algorithm>

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "dsemodsettings.h"

DSEModSettings::DSEModSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_cwKeyerGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DSEModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500.0f;
    m_mode = DSEModModeDStar;
    m_transmit = false;
    m_invert = false;
    m_modAFInput = DSEModInputNone;
    m_toneFrequency = 1000.0f;
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = true;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_vocoder = DSEModVocoderMbelib;
    m_ambeFeatureIndex = -1;
    m_connectAMBE = false;
    m_vocoderGain = 0;
    m_toneDetect = true;
    m_feedbackAudioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_feedbackVolumeFactor = 0.5f;
    m_feedbackAudioEnable = false;
    m_spectrumAudio = false;
    m_noiseSuppress = false;
    m_dtmfSequence = "";
    m_dstarMyCall = "";
    m_dstarMySuffix = "";
    m_dstarUrCall = "CQCQCQ";
    m_dstarRpt1 = "DIRECT";
    m_dstarRpt2 = "DIRECT";
    m_dstarText = "";
    m_dstarFlag1 = 0;
    m_dstarFlag2 = 0;
    m_dstarFlag3 = 0;
    m_dmrColorCode = 1;
    m_dmrSourceId = 1;
    m_dmrDestinationId = 9;
    m_dmrGroupCall = true;
    m_dmrSlot = 1;
    m_dmrDirect = false;
    m_dmrFid = 0;
    m_dmrServiceOptions = 0;
    m_ysfCallsign = "";
    m_ysfDestination = "*****";
    m_ysfDownlink = "";
    m_ysfUplink = "";
    m_ysfMode = DSEModYSFModeVD2;
    m_nxdnRan = 1;
    m_nxdnSourceId = 1;
    m_nxdnDestinationId = 1;
    m_nxdnGroupCall = true;
    m_dpmrColorCode = 0;
    m_dpmrOwnId = 1;
    m_dpmrCalledId = 16777215;
    m_p25Nac = 0x293;
    m_p25TalkGroup = 1;
    m_p25SourceId = 1;
    m_p25p2Wacn = 0xBEE00;
    m_p25p2Sysid = 0x001;
    m_angel61Rate = DSEModAngel61RateAuto;
    m_angel61Source = "";
    m_angel61Destination = "CQ";
    m_angel61Path = "";
    m_angel61Message = "";
    m_sendPosition = true;
    m_angel61Demod = "";
    m_angel61Symbol = "/-";
    m_angel61FileName = "";
    m_angel61Tunnel = false;
    m_angel61TunnelPeer = "";
    m_angel61TunnelInterface = "sdrangel0";
    m_angel61TunnelAddress = "";
    m_angel61TunnelRoutes = "";
    m_angel61TunnelAllow = "";
    m_angel61TunnelGroups = "";
    m_angel61TunnelMTU = 576;
    m_m17Source = "";
    m_m17Destination = "";
    m_m17CAN = 0;
    m_m17Message = "";
    m_codecModelDirectory = "";
    m_audioFileName = "";
    m_frameSessionMaxDurationMs = 180000;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "DSE Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray DSEModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeS32(3, (int) m_mode);
    s.writeBool(4, m_invert);
    s.writeS32(5, (int) m_modAFInput);
    s.writeReal(6, m_toneFrequency);
    s.writeReal(7, m_volumeFactor);
    s.writeBool(8, m_channelMute);
    s.writeBool(9, m_playLoop);
    s.writeString(10, m_audioDeviceName);
    s.writeS32(11, (int) m_vocoder);
    s.writeS32(12, m_ambeFeatureIndex);
    s.writeBool(13, m_connectAMBE);
    s.writeS32(14, m_vocoderGain);
    s.writeBool(15, m_toneDetect);
    s.writeBool(16, m_noiseSuppress);
    s.writeString(17, m_dtmfSequence);

    s.writeString(20, m_dstarMyCall);
    s.writeString(21, m_dstarMySuffix);
    s.writeString(22, m_dstarUrCall);
    s.writeString(23, m_dstarRpt1);
    s.writeString(24, m_dstarRpt2);
    s.writeString(25, m_dstarText);
    s.writeS32(26, m_dstarFlag1);
    s.writeS32(27, m_dstarFlag2);
    s.writeS32(28, m_dstarFlag3);

    s.writeS32(30, m_dmrColorCode);
    s.writeS32(31, m_dmrSourceId);
    s.writeS32(32, m_dmrDestinationId);
    s.writeBool(33, m_dmrGroupCall);
    s.writeS32(34, m_dmrSlot);
    s.writeS32(35, m_dmrFid);
    s.writeS32(36, m_dmrServiceOptions);
    s.writeBool(37, m_dmrDirect);
    s.writeString(60, m_ysfCallsign);
    s.writeString(61, m_ysfDestination);
    s.writeString(62, m_ysfDownlink);
    s.writeString(63, m_ysfUplink);
    s.writeS32(104, (int) m_ysfMode);
    s.writeS32(64, m_nxdnRan);
    s.writeS32(65, m_nxdnSourceId);
    s.writeS32(66, m_nxdnDestinationId);
    s.writeBool(67, m_nxdnGroupCall);
    s.writeS32(68, m_dpmrColorCode);
    s.writeS32(69, m_dpmrOwnId);
    s.writeS32(70, m_dpmrCalledId);
    s.writeS32(71, m_p25Nac);
    s.writeS32(72, m_p25TalkGroup);
    s.writeS32(73, m_p25SourceId);
    s.writeS32(74, m_p25p2Wacn);
    s.writeS32(75, m_p25p2Sysid);
    s.writeS32(76, (int) m_angel61Rate);
    s.writeString(77, m_angel61Source);
    s.writeString(78, m_angel61Destination);
    s.writeString(79, m_angel61Path);
    s.writeString(80, m_angel61Message);
    s.writeBool(101, m_sendPosition);
    s.writeString(103, m_codecModelDirectory);
    s.writeString(83, m_audioFileName);
    s.writeS32(84, m_frameSessionMaxDurationMs);
    s.writeString(85, m_angel61Demod);
    s.writeString(86, m_angel61Symbol);
    s.writeString(87, m_angel61FileName);
    s.writeString(88, m_m17Source);
    s.writeString(89, m_m17Destination);
    s.writeS32(90, m_m17CAN);
    s.writeString(91, m_m17Message);
    s.writeBool(92, m_angel61Tunnel);
    s.writeString(93, m_angel61TunnelPeer);
    s.writeString(94, m_angel61TunnelInterface);
    s.writeString(95, m_angel61TunnelAddress);
    s.writeString(96, m_angel61TunnelRoutes);
    s.writeString(105, m_angel61TunnelAllow);
    s.writeString(106, m_angel61TunnelGroups);
    s.writeS32(97, m_angel61TunnelMTU);
    s.writeString(98, m_feedbackAudioDeviceName);
    s.writeReal(99, m_feedbackVolumeFactor);
    s.writeBool(100, m_feedbackAudioEnable);
    s.writeBool(107, m_spectrumAudio);

    s.writeU32(40, m_rgbColor);
    s.writeString(41, m_title);
    s.writeS32(42, m_streamIndex);
    s.writeBool(43, m_useReverseAPI);
    s.writeString(44, m_reverseAPIAddress);
    s.writeU32(45, m_reverseAPIPort);
    s.writeU32(46, m_reverseAPIDeviceIndex);
    s.writeU32(47, m_reverseAPIChannelIndex);
    s.writeS32(48, m_workspaceIndex);
    s.writeBlob(49, m_geometryBytes);
    s.writeBool(50, m_hidden);

    if (m_channelMarker) {
        s.writeBlob(51, m_channelMarker->serialize());
    }

    if (m_spectrumGUI) {
        s.writeBlob(52, m_spectrumGUI->serialize());
    }

    if (m_cwKeyerGUI) {
        s.writeBlob(53, m_cwKeyerGUI->serialize());
    } else { // standalone operation with presets
        s.writeBlob(53, m_cwKeyerSettings.serialize());
    }

    if (m_rollupState) {
        s.writeBlob(54, m_rollupState->serialize());
    }

    return s.final();
}

bool DSEModSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 12500.0f);
        d.readS32(3, &tmp, 0);
        m_mode = modeOf(tmp);
        d.readBool(4, &m_invert, false);
        d.readS32(5, &tmp, 0);
        m_modAFInput = (tmp < 0) || (tmp > (int) DSEModInputSilence) ? DSEModInputNone : (DSEModInputAF) tmp;
        d.readReal(6, &m_toneFrequency, 1000.0f);
        d.readReal(7, &m_volumeFactor, 1.0f);
        d.readBool(8, &m_channelMute, false);
        d.readBool(9, &m_playLoop, true);
        d.readString(10, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(11, &tmp, (int) DSEModVocoderMbelib);
        m_vocoder = (tmp < 0) || (tmp > (int) DSEModVocoderAMBEFeature) ? DSEModVocoderMbelib : (DSEModVocoderType) tmp;
        d.readS32(12, &m_ambeFeatureIndex, -1);
        d.readBool(13, &m_connectAMBE, false);
        d.readS32(14, &m_vocoderGain, 0);
        d.readBool(15, &m_toneDetect, true);
        d.readBool(16, &m_noiseSuppress, false);
        d.readString(17, &m_dtmfSequence, "");

        d.readString(20, &m_dstarMyCall, "");
        d.readString(21, &m_dstarMySuffix, "");
        d.readString(22, &m_dstarUrCall, "CQCQCQ");
        d.readString(23, &m_dstarRpt1, "DIRECT");
        d.readString(24, &m_dstarRpt2, "DIRECT");
        d.readString(25, &m_dstarText, "");
        d.readS32(26, &m_dstarFlag1, 0);
        d.readS32(27, &m_dstarFlag2, 0);
        d.readS32(28, &m_dstarFlag3, 0);

        d.readS32(30, &m_dmrColorCode, 1);
        d.readS32(31, &m_dmrSourceId, 1);
        d.readS32(32, &m_dmrDestinationId, 9);
        d.readBool(33, &m_dmrGroupCall, true);
        d.readS32(34, &m_dmrSlot, 1);
        d.readS32(35, &m_dmrFid, 0);
        d.readS32(36, &m_dmrServiceOptions, 0);
        d.readBool(37, &m_dmrDirect, false);
        d.readString(60, &m_ysfCallsign, "");
        d.readString(61, &m_ysfDestination, "*****");
        d.readString(62, &m_ysfDownlink, "");
        d.readString(63, &m_ysfUplink, "");
        d.readS32(104, &tmp, (int) DSEModYSFModeVD2);
        m_ysfMode = (DSEModYSFMode) std::max((int) DSEModYSFModeVD2, std::min((int) DSEModYSFModeVW, tmp));
        d.readS32(64, &m_nxdnRan, 1);
        d.readS32(65, &m_nxdnSourceId, 1);
        d.readS32(66, &m_nxdnDestinationId, 1);
        d.readBool(67, &m_nxdnGroupCall, true);
        d.readS32(68, &m_dpmrColorCode, 0);
        d.readS32(69, &m_dpmrOwnId, 1);
        d.readS32(70, &m_dpmrCalledId, 16777215);
        d.readS32(71, &m_p25Nac, 0x293);
        d.readS32(72, &m_p25TalkGroup, 1);
        d.readS32(73, &m_p25SourceId, 1);
        d.readS32(74, &m_p25p2Wacn, 0xBEE00);
        d.readS32(75, &m_p25p2Sysid, 0x001);
        d.readS32(76, &tmp, 0);
        m_angel61Rate = (tmp < 0) || (tmp > (int) DSEModAngel61RateTier3) ? DSEModAngel61RateAuto : (DSEModAngel61Rate) tmp;
        d.readString(77, &m_angel61Source, "");
        d.readString(78, &m_angel61Destination, "CQ");
        d.readString(79, &m_angel61Path, "");
        d.readString(80, &m_angel61Message, "");
        // One switch now; a preset written before it answers for both of the modes it had
        bool dstarSendPosition = false;
        bool angel61SendPosition = true;
        d.readBool(29, &dstarSendPosition, false);
        d.readBool(81, &angel61SendPosition, true);
        d.readBool(101, &m_sendPosition, dstarSendPosition || angel61SendPosition);
        d.readString(103, &m_codecModelDirectory, "");
        d.readString(83, &m_audioFileName, "");
        d.readS32(84, &m_frameSessionMaxDurationMs, 180000);
        d.readString(85, &m_angel61Demod, "");
        d.readString(86, &m_angel61Symbol, "/-");
        d.readString(87, &m_angel61FileName, "");
        d.readString(88, &m_m17Source, "");
        d.readString(89, &m_m17Destination, "");
        d.readS32(90, &m_m17CAN, 0);
        m_m17CAN = std::max(0, std::min(15, m_m17CAN));
        d.readString(91, &m_m17Message, "");
        d.readBool(92, &m_angel61Tunnel, false);
        d.readString(93, &m_angel61TunnelPeer, "");
        d.readString(94, &m_angel61TunnelInterface, "sdrangel0");
        d.readString(95, &m_angel61TunnelAddress, "");
        d.readString(96, &m_angel61TunnelRoutes, "");
        d.readString(105, &m_angel61TunnelAllow, "");
        d.readString(106, &m_angel61TunnelGroups, "");
        d.readS32(97, &tmp, 576);
        m_angel61TunnelMTU = ((tmp < 68) || (tmp > 1500)) ? 576 : tmp;
        d.readString(98, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(99, &m_feedbackVolumeFactor, 0.5f);
        d.readBool(100, &m_feedbackAudioEnable, false);
        d.readBool(107, &m_spectrumAudio, false);
        m_frameSessionMaxDurationMs = std::max(180000, std::min(86400000, m_frameSessionMaxDurationMs));

        d.readU32(40, &m_rgbColor, QColor(255, 0, 255).rgb());
        d.readString(41, &m_title, "DSE Modulator");
        d.readS32(42, &m_streamIndex, 0);
        d.readBool(43, &m_useReverseAPI, false);
        d.readString(44, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(45, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(46, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(47, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(48, &m_workspaceIndex, 0);
        d.readBlob(49, &m_geometryBytes);
        d.readBool(50, &m_hidden, false);

        if (m_channelMarker)
        {
            d.readBlob(51, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        if (m_spectrumGUI)
        {
            d.readBlob(52, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readBlob(53, &bytetmp);

        if (m_cwKeyerGUI) {
            m_cwKeyerGUI->deserialize(bytetmp);
        } else { // standalone operation with presets
            m_cwKeyerSettings.deserialize(bytetmp);
        }

        if (m_rollupState)
        {
            d.readBlob(54, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        qDebug() << "DSEModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

void DSEModSettings::applySettings(const QStringList& settingsKeys, const DSEModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("mode")) {
        m_mode = settings.m_mode;
    }
    if (settingsKeys.contains("transmit")) {
        m_transmit = settings.m_transmit;
    }
    if (settingsKeys.contains("invert")) {
        m_invert = settings.m_invert;
    }
    if (settingsKeys.contains("modAFInput")) {
        m_modAFInput = settings.m_modAFInput;
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
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("vocoder")) {
        m_vocoder = settings.m_vocoder;
    }
    if (settingsKeys.contains("ambeFeatureIndex")) {
        m_ambeFeatureIndex = settings.m_ambeFeatureIndex;
    }
    if (settingsKeys.contains("connectAMBE")) {
        m_connectAMBE = settings.m_connectAMBE;
    }
    if (settingsKeys.contains("vocoderGain")) {
        m_vocoderGain = settings.m_vocoderGain;
    }
    if (settingsKeys.contains("toneDetect")) {
        m_toneDetect = settings.m_toneDetect;
    }
    if (settingsKeys.contains("noiseSuppress")) {
        m_noiseSuppress = settings.m_noiseSuppress;
    }
    if (settingsKeys.contains("dtmfSequence")) {
        m_dtmfSequence = settings.m_dtmfSequence;
    }
    if (settingsKeys.contains("dstarMyCall")) {
        m_dstarMyCall = settings.m_dstarMyCall;
    }
    if (settingsKeys.contains("dstarMySuffix")) {
        m_dstarMySuffix = settings.m_dstarMySuffix;
    }
    if (settingsKeys.contains("dstarUrCall")) {
        m_dstarUrCall = settings.m_dstarUrCall;
    }
    if (settingsKeys.contains("dstarRpt1")) {
        m_dstarRpt1 = settings.m_dstarRpt1;
    }
    if (settingsKeys.contains("dstarRpt2")) {
        m_dstarRpt2 = settings.m_dstarRpt2;
    }
    if (settingsKeys.contains("dstarText")) {
        m_dstarText = settings.m_dstarText;
    }
    if (settingsKeys.contains("dstarFlag1")) {
        m_dstarFlag1 = settings.m_dstarFlag1;
    }
    if (settingsKeys.contains("dstarFlag2")) {
        m_dstarFlag2 = settings.m_dstarFlag2;
    }
    if (settingsKeys.contains("dstarFlag3")) {
        m_dstarFlag3 = settings.m_dstarFlag3;
    }
    if (settingsKeys.contains("dmrColorCode")) {
        m_dmrColorCode = settings.m_dmrColorCode;
    }
    if (settingsKeys.contains("dmrSourceId")) {
        m_dmrSourceId = settings.m_dmrSourceId;
    }
    if (settingsKeys.contains("dmrDestinationId")) {
        m_dmrDestinationId = settings.m_dmrDestinationId;
    }
    if (settingsKeys.contains("dmrGroupCall")) {
        m_dmrGroupCall = settings.m_dmrGroupCall;
    }
    if (settingsKeys.contains("dmrSlot")) {
        m_dmrSlot = settings.m_dmrSlot;
    }
    if (settingsKeys.contains("dmrDirect")) {
        m_dmrDirect = settings.m_dmrDirect;
    }
    if (settingsKeys.contains("dmrFid")) {
        m_dmrFid = settings.m_dmrFid;
    }
    if (settingsKeys.contains("dmrServiceOptions")) {
        m_dmrServiceOptions = settings.m_dmrServiceOptions;
    }
    if (settingsKeys.contains("ysfCallsign")) {
        m_ysfCallsign = settings.m_ysfCallsign;
    }
    if (settingsKeys.contains("ysfDestination")) {
        m_ysfDestination = settings.m_ysfDestination;
    }
    if (settingsKeys.contains("ysfDownlink")) {
        m_ysfDownlink = settings.m_ysfDownlink;
    }
    if (settingsKeys.contains("ysfUplink")) {
        m_ysfUplink = settings.m_ysfUplink;
    }
    if (settingsKeys.contains("ysfMode")) {
        m_ysfMode = settings.m_ysfMode;
    }
    if (settingsKeys.contains("nxdnRan")) {
        m_nxdnRan = settings.m_nxdnRan;
    }
    if (settingsKeys.contains("nxdnSourceId")) {
        m_nxdnSourceId = settings.m_nxdnSourceId;
    }
    if (settingsKeys.contains("nxdnDestinationId")) {
        m_nxdnDestinationId = settings.m_nxdnDestinationId;
    }
    if (settingsKeys.contains("nxdnGroupCall")) {
        m_nxdnGroupCall = settings.m_nxdnGroupCall;
    }
    if (settingsKeys.contains("dpmrColorCode")) {
        m_dpmrColorCode = settings.m_dpmrColorCode;
    }
    if (settingsKeys.contains("dpmrOwnId")) {
        m_dpmrOwnId = settings.m_dpmrOwnId;
    }
    if (settingsKeys.contains("dpmrCalledId")) {
        m_dpmrCalledId = settings.m_dpmrCalledId;
    }
    if (settingsKeys.contains("p25Nac")) {
        m_p25Nac = settings.m_p25Nac;
    }
    if (settingsKeys.contains("p25TalkGroup")) {
        m_p25TalkGroup = settings.m_p25TalkGroup;
    }
    if (settingsKeys.contains("p25SourceId")) {
        m_p25SourceId = settings.m_p25SourceId;
    }
    if (settingsKeys.contains("p25p2Wacn")) {
        m_p25p2Wacn = settings.m_p25p2Wacn;
    }
    if (settingsKeys.contains("p25p2Sysid")) {
        m_p25p2Sysid = settings.m_p25p2Sysid;
    }
    if (settingsKeys.contains("angel61Rate")) {
        m_angel61Rate = settings.m_angel61Rate;
    }
    if (settingsKeys.contains("angel61Source")) {
        m_angel61Source = settings.m_angel61Source;
    }
    if (settingsKeys.contains("angel61Destination")) {
        m_angel61Destination = settings.m_angel61Destination;
    }
    if (settingsKeys.contains("angel61Path")) {
        m_angel61Path = settings.m_angel61Path;
    }
    if (settingsKeys.contains("angel61Message")) {
        m_angel61Message = settings.m_angel61Message;
    }
    if (settingsKeys.contains("angel61Demod")) {
        m_angel61Demod = settings.m_angel61Demod;
    }
    if (settingsKeys.contains("angel61Symbol")) {
        m_angel61Symbol = settings.m_angel61Symbol;
    }
    if (settingsKeys.contains("angel61FileName")) {
        m_angel61FileName = settings.m_angel61FileName;
    }
    if (settingsKeys.contains("angel61Tunnel")) {
        m_angel61Tunnel = settings.m_angel61Tunnel;
    }
    if (settingsKeys.contains("angel61TunnelPeer")) {
        m_angel61TunnelPeer = settings.m_angel61TunnelPeer;
    }
    if (settingsKeys.contains("angel61TunnelInterface")) {
        m_angel61TunnelInterface = settings.m_angel61TunnelInterface;
    }
    if (settingsKeys.contains("angel61TunnelAddress")) {
        m_angel61TunnelAddress = settings.m_angel61TunnelAddress;
    }
    if (settingsKeys.contains("angel61TunnelRoutes")) {
        m_angel61TunnelRoutes = settings.m_angel61TunnelRoutes;
    }
    if (settingsKeys.contains("angel61TunnelAllow")) {
        m_angel61TunnelAllow = settings.m_angel61TunnelAllow;
    }
    if (settingsKeys.contains("angel61TunnelGroups")) {
        m_angel61TunnelGroups = settings.m_angel61TunnelGroups;
    }
    if (settingsKeys.contains("angel61TunnelMTU")) {
        m_angel61TunnelMTU = settings.m_angel61TunnelMTU;
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
    if (settingsKeys.contains("spectrumAudio")) {
        m_spectrumAudio = settings.m_spectrumAudio;
    }
    if (settingsKeys.contains("m17Source")) {
        m_m17Source = settings.m_m17Source;
    }
    if (settingsKeys.contains("m17Destination")) {
        m_m17Destination = settings.m_m17Destination;
    }
    if (settingsKeys.contains("m17CAN")) {
        m_m17CAN = settings.m_m17CAN;
    }
    if (settingsKeys.contains("m17Message")) {
        m_m17Message = settings.m_m17Message;
    }
    if (settingsKeys.contains("sendPosition")) {
        m_sendPosition = settings.m_sendPosition;
    }
    if (settingsKeys.contains("codecModelDirectory")) {
        m_codecModelDirectory = settings.m_codecModelDirectory;
    }
    if (settingsKeys.contains("audioFileName")) {
        m_audioFileName = settings.m_audioFileName;
    }
    if (settingsKeys.contains("frameSessionMaxDurationMs")) {
        m_frameSessionMaxDurationMs = settings.m_frameSessionMaxDurationMs;
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
    if (settingsKeys.contains("channelMarker")) {
        m_channelMarker = settings.m_channelMarker;
    }
    if (settingsKeys.contains("spectrumConfig")) {
        m_spectrumGUI = settings.m_spectrumGUI;
    }
    if (settingsKeys.contains("cwKeyer")) {
        m_cwKeyerGUI = settings.m_cwKeyerGUI;
    }
    if (settingsKeys.contains("rollupState")) {
        m_rollupState = settings.m_rollupState;
    }
}

QString DSEModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("mode") || force) {
        ostr << " m_mode: " << m_mode;
    }
    if (settingsKeys.contains("transmit") || force) {
        ostr << " m_transmit: " << m_transmit;
    }
    if (settingsKeys.contains("invert") || force) {
        ostr << " m_invert: " << m_invert;
    }
    if (settingsKeys.contains("modAFInput") || force) {
        ostr << " m_modAFInput: " << m_modAFInput;
    }
    if (settingsKeys.contains("toneFrequency") || force) {
        ostr << " m_toneFrequency: " << m_toneFrequency;
    }
    if (settingsKeys.contains("volumeFactor") || force) {
        ostr << " m_volumeFactor: " << m_volumeFactor;
    }
    if (settingsKeys.contains("channelMute") || force) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (settingsKeys.contains("playLoop") || force) {
        ostr << " m_playLoop: " << m_playLoop;
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        ostr << " m_audioDeviceName: " << m_audioDeviceName.toStdString();
    }
    if (settingsKeys.contains("vocoder") || force) {
        ostr << " m_vocoder: " << m_vocoder;
    }
    if (settingsKeys.contains("ambeFeatureIndex") || force) {
        ostr << " m_ambeFeatureIndex: " << m_ambeFeatureIndex;
    }
    if (settingsKeys.contains("connectAMBE") || force) {
        ostr << " m_connectAMBE: " << m_connectAMBE;
    }
    if (settingsKeys.contains("vocoderGain") || force) {
        ostr << " m_vocoderGain: " << m_vocoderGain;
    }
    if (settingsKeys.contains("toneDetect") || force) {
        ostr << " m_toneDetect: " << m_toneDetect;
    }
    if (settingsKeys.contains("noiseSuppress") || force) {
        ostr << " m_noiseSuppress: " << m_noiseSuppress;
    }
    if (settingsKeys.contains("dtmfSequence") || force) {
        ostr << " m_dtmfSequence: " << m_dtmfSequence.toStdString();
    }
    if (settingsKeys.contains("dstarMyCall") || force) {
        ostr << " m_dstarMyCall: " << m_dstarMyCall.toStdString();
    }
    if (settingsKeys.contains("dstarMySuffix") || force) {
        ostr << " m_dstarMySuffix: " << m_dstarMySuffix.toStdString();
    }
    if (settingsKeys.contains("dstarUrCall") || force) {
        ostr << " m_dstarUrCall: " << m_dstarUrCall.toStdString();
    }
    if (settingsKeys.contains("dstarRpt1") || force) {
        ostr << " m_dstarRpt1: " << m_dstarRpt1.toStdString();
    }
    if (settingsKeys.contains("dstarRpt2") || force) {
        ostr << " m_dstarRpt2: " << m_dstarRpt2.toStdString();
    }
    if (settingsKeys.contains("dstarText") || force) {
        ostr << " m_dstarText: " << m_dstarText.toStdString();
    }
    if (settingsKeys.contains("dstarFlag1") || force) {
        ostr << " m_dstarFlag1: " << m_dstarFlag1;
    }
    if (settingsKeys.contains("dstarFlag2") || force) {
        ostr << " m_dstarFlag2: " << m_dstarFlag2;
    }
    if (settingsKeys.contains("dstarFlag3") || force) {
        ostr << " m_dstarFlag3: " << m_dstarFlag3;
    }
    if (settingsKeys.contains("dmrColorCode") || force) {
        ostr << " m_dmrColorCode: " << m_dmrColorCode;
    }
    if (settingsKeys.contains("dmrSourceId") || force) {
        ostr << " m_dmrSourceId: " << m_dmrSourceId;
    }
    if (settingsKeys.contains("dmrDestinationId") || force) {
        ostr << " m_dmrDestinationId: " << m_dmrDestinationId;
    }
    if (settingsKeys.contains("dmrGroupCall") || force) {
        ostr << " m_dmrGroupCall: " << m_dmrGroupCall;
    }
    if (settingsKeys.contains("dmrSlot") || force) {
        ostr << " m_dmrSlot: " << m_dmrSlot;
    }
    if (settingsKeys.contains("dmrDirect") || force) {
        ostr << " m_dmrDirect: " << m_dmrDirect;
    }
    if (settingsKeys.contains("dmrFid") || force) {
        ostr << " m_dmrFid: " << m_dmrFid;
    }
    if (settingsKeys.contains("dmrServiceOptions") || force) {
        ostr << " m_dmrServiceOptions: " << m_dmrServiceOptions;
    }
    if (settingsKeys.contains("ysfCallsign") || force) {
        ostr << " m_ysfCallsign: " << m_ysfCallsign.toStdString();
    }
    if (settingsKeys.contains("ysfDestination") || force) {
        ostr << " m_ysfDestination: " << m_ysfDestination.toStdString();
    }
    if (settingsKeys.contains("ysfDownlink") || force) {
        ostr << " m_ysfDownlink: " << m_ysfDownlink.toStdString();
    }
    if (settingsKeys.contains("ysfUplink") || force) {
        ostr << " m_ysfUplink: " << m_ysfUplink.toStdString();
    }
    if (settingsKeys.contains("ysfMode") || force) {
        ostr << " m_ysfMode: " << m_ysfMode;
    }
    if (settingsKeys.contains("nxdnRan") || force) {
        ostr << " m_nxdnRan: " << m_nxdnRan;
    }
    if (settingsKeys.contains("nxdnSourceId") || force) {
        ostr << " m_nxdnSourceId: " << m_nxdnSourceId;
    }
    if (settingsKeys.contains("nxdnDestinationId") || force) {
        ostr << " m_nxdnDestinationId: " << m_nxdnDestinationId;
    }
    if (settingsKeys.contains("nxdnGroupCall") || force) {
        ostr << " m_nxdnGroupCall: " << m_nxdnGroupCall;
    }
    if (settingsKeys.contains("dpmrColorCode") || force) {
        ostr << " m_dpmrColorCode: " << m_dpmrColorCode;
    }
    if (settingsKeys.contains("dpmrOwnId") || force) {
        ostr << " m_dpmrOwnId: " << m_dpmrOwnId;
    }
    if (settingsKeys.contains("dpmrCalledId") || force) {
        ostr << " m_dpmrCalledId: " << m_dpmrCalledId;
    }
    if (settingsKeys.contains("p25Nac") || force) {
        ostr << " m_p25Nac: " << m_p25Nac;
    }
    if (settingsKeys.contains("p25TalkGroup") || force) {
        ostr << " m_p25TalkGroup: " << m_p25TalkGroup;
    }
    if (settingsKeys.contains("p25SourceId") || force) {
        ostr << " m_p25SourceId: " << m_p25SourceId;
    }
    if (settingsKeys.contains("p25p2Wacn") || force) {
        ostr << " m_p25p2Wacn: " << m_p25p2Wacn;
    }
    if (settingsKeys.contains("p25p2Sysid") || force) {
        ostr << " m_p25p2Sysid: " << m_p25p2Sysid;
    }
    if (settingsKeys.contains("angel61Rate") || force) {
        ostr << " m_angel61Rate: " << m_angel61Rate;
    }
    if (settingsKeys.contains("angel61Source") || force) {
        ostr << " m_angel61Source: " << m_angel61Source.toStdString();
    }
    if (settingsKeys.contains("angel61Destination") || force) {
        ostr << " m_angel61Destination: " << m_angel61Destination.toStdString();
    }
    if (settingsKeys.contains("angel61Path") || force) {
        ostr << " m_angel61Path: " << m_angel61Path.toStdString();
    }
    if (settingsKeys.contains("angel61Message") || force) {
        ostr << " m_angel61Message: " << m_angel61Message.toStdString();
    }
    if (settingsKeys.contains("sendPosition") || force) {
        ostr << " m_sendPosition: " << m_sendPosition;
    }
    if (settingsKeys.contains("angel61Demod") || force) {
        ostr << " m_angel61Demod: " << m_angel61Demod.toStdString();
    }
    if (settingsKeys.contains("angel61Symbol") || force) {
        ostr << " m_angel61Symbol: " << m_angel61Symbol.toStdString();
    }
    if (settingsKeys.contains("angel61FileName") || force) {
        ostr << " m_angel61FileName: " << m_angel61FileName.toStdString();
    }
    if (settingsKeys.contains("m17Source") || force) {
        ostr << " m_m17Source: " << m_m17Source.toStdString();
    }
    if (settingsKeys.contains("m17Destination") || force) {
        ostr << " m_m17Destination: " << m_m17Destination.toStdString();
    }
    if (settingsKeys.contains("m17CAN") || force) {
        ostr << " m_m17CAN: " << m_m17CAN;
    }
    if (settingsKeys.contains("m17Message") || force) {
        ostr << " m_m17Message: " << m_m17Message.toStdString();
    }
    if (settingsKeys.contains("codecModelDirectory") || force) {
        ostr << " m_codecModelDirectory: " << m_codecModelDirectory.toStdString();
    }
    if (settingsKeys.contains("audioFileName") || force) {
        ostr << " m_audioFileName: " << m_audioFileName.toStdString();
    }
    if (settingsKeys.contains("frameSessionMaxDurationMs") || force) {
        ostr << " m_frameSessionMaxDurationMs: " << m_frameSessionMaxDurationMs;
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

int DSEModSettings::getMbeRateIndex(DSEModMode mode)
{
    switch (mode)
    {
    case DSEModModeDMR:
    case DSEModModeYSF:
    case DSEModModeNXDN:
    case DSEModModeNXDN96:
    case DSEModModeDPMR:
    case DSEModModeP25P2:
        return 2; // SerialDV::DVRate3600x2450
    case DSEModModeP25:
        return 3; // SerialDV::DVRate7200x4400
    case DSEModModeDStar:
    default:
        return 1; // SerialDV::DVRate3600x2400
    }
}

const char* DSEModSettings::getModeName(DSEModMode mode)
{
    switch (mode)
    {
    case DSEModModeDMR:
        return "DMR";
    case DSEModModeYSF:
        return "YSF";
    case DSEModModeNXDN:
        return "NXDN";
    case DSEModModeNXDN96:
        return "NXDN96";
    case DSEModModeDPMR:
        return "dPMR";
    case DSEModModeP25:
        return "P25";
    case DSEModModeP25P2:
        return "P25P2";
    case DSEModModeAngel61:
        return "Angel61 NB";
    case DSEModModeAngel61WB:
        return "Angel61 WB";
    case DSEModModeAngel61HFNB:
        return "Angel61 HF-NB";
    case DSEModModeAngel61HFWB:
        return "Angel61 HF-WB";
    case DSEModModeDStar:
    default:
        return "D-STAR";
    }
}
