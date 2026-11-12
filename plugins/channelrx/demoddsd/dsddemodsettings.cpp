///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021, 2026 Jon Beniston, M7RCE <jon@beniston.com>               //
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

#include <algorithm>
#include <cmath>

#include <QColor>

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "dsddemodsettings.h"

DSDDemodSettings::DSDDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DSDDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500.0;
    m_volume = 2.0;
    m_protocol = ProtocolAuto;
    m_p25p2Wacn = 0xBEE00;
    m_p25p2Sysid = 0x001;
    m_p25p2Nac = 0x293;
    m_coherent = true;
    m_audioMute = false;
    m_slot1On = true;
    m_slot2On = false;
    m_tdmaStereo = false;
    m_dmrColorCode = -1;
    m_angel61FileDirectory = "";
    m_codecModelDirectory = "";
    m_angel61FileMaxKB = 1024;
    m_angel61FileConfirm = false;
    m_angel61Groups = "";
    m_contactReportEnabled = false;
    m_contactReportCallsign = "";
    m_rgbColor = QColor(0, 255, 255).rgb();
    m_title = "DSD Demodulator";
    m_highPassFilter = false;
    m_traceLengthMutliplier = 6; // 300 ms
    m_traceStroke = 100;
    m_traceDecay = 200;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_ambeFeatureIndex = -1;
    m_connectAMBE = false;
}

const char *DSDDemodSettings::getProtocolName(int protocol)
{
    static const char *names[NbProtocols] = { "Auto", "DMR", "D-STAR", "YSF", "NXDN48", "NXDN96", "dPMR", "P25", "P25P2", "M17", "Angel61 NB", "Angel61 WB",
        "Angel61 HF-NB", "Angel61 HF-WB" };
    return ((protocol >= 0) && (protocol < NbProtocols)) ? names[protocol] : "Auto";
}

QByteArray DSDDemodSettings::serialize() const
{
    // Version 2: the discriminator, filter, PLL, AFC, baud rate, squelch and squelch gate settings of the
    // DSDcc based receiver are gone; the protocol selection replaces the baud rate
    SimpleSerializer s(2);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth/100.0);
    s.writeU32(7, m_rgbColor);
    s.writeS32(9, (int) std::lround(m_volume * 10.0));   // (rounded: 0.7 was saved as 6)
    s.writeBool(14, m_slot1On);
    s.writeBool(15, m_slot2On);
    s.writeBool(16, m_tdmaStereo);

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
    s.writeS32(35, m_ambeFeatureIndex);
    s.writeBool(36, m_connectAMBE);
    s.writeS32(38, m_protocol);
    s.writeS32(39, m_p25p2Wacn);
    s.writeS32(40, m_p25p2Sysid);
    s.writeS32(41, m_p25p2Nac);
    s.writeBool(42, m_coherent);
    s.writeS32(43, m_dmrColorCode);
    s.writeString(44, m_angel61FileDirectory);
    s.writeString(51, m_codecModelDirectory);
    s.writeS32(45, m_angel61FileMaxKB);
    s.writeBool(52, m_angel61FileConfirm);
    s.writeString(46, m_angel61Groups);
    s.writeBool(47, m_contactReportEnabled);
    s.writeString(50, m_contactReportCallsign);

    return s.final();
}

bool DSDDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if ((d.getVersion() == 1) || (d.getVersion() == 2))
    {
        // Version 1 presets carry the removed settings under keys that are simply not read
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
        m_rfBandwidth = std::max(1000.0, std::min(50000.0, tmp * 100.0));   // the GUI's range
        d.readU32(7, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readS32(9, &tmp, 20);
        m_volume = std::max(0.0, std::min(10.0, tmp / 10.0));
        d.readBool(14, &m_slot1On, true);
        d.readBool(15, &m_slot2On, false);
        d.readBool(16, &m_tdmaStereo, false);
        d.readString(18, &m_title, "DSD Demodulator");
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
        d.readS32(35, &m_ambeFeatureIndex, -1);
        d.readBool(36, &m_connectAMBE, false);
        d.readS32(38, &tmp, ProtocolAuto);
        m_protocol = protocolOf(tmp);
        d.readS32(39, &m_p25p2Wacn, 0xBEE00);
        d.readS32(40, &m_p25p2Sysid, 0x001);
        d.readS32(41, &m_p25p2Nac, 0x293);
        d.readBool(42, &m_coherent, true);
        d.readS32(43, &tmp, -1);
        m_dmrColorCode = ((tmp >= 0) && (tmp <= 63)) ? tmp : -1;
        d.readString(44, &m_angel61FileDirectory, "");
        d.readString(51, &m_codecModelDirectory, "");
        d.readS32(45, &m_angel61FileMaxKB, 1024);
        m_angel61FileMaxKB = std::max(0, std::min(65536, m_angel61FileMaxKB));
        d.readBool(52, &m_angel61FileConfirm, false);
        d.readString(46, &m_angel61Groups, "");
        d.readBool(47, &m_contactReportEnabled, false);
        d.readString(50, &m_contactReportCallsign, "");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void DSDDemodSettings::applySettings(const QStringList& settingsKeys, const DSDDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("protocol")) {
        m_protocol = settings.m_protocol;
    }
    if (settingsKeys.contains("p25p2Wacn")) {
        m_p25p2Wacn = settings.m_p25p2Wacn;
    }
    if (settingsKeys.contains("p25p2Sysid")) {
        m_p25p2Sysid = settings.m_p25p2Sysid;
    }
    if (settingsKeys.contains("p25p2Nac")) {
        m_p25p2Nac = settings.m_p25p2Nac;
    }
    if (settingsKeys.contains("coherent")) {
        m_coherent = settings.m_coherent;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("slot1On")) {
        m_slot1On = settings.m_slot1On;
    }
    if (settingsKeys.contains("slot2On")) {
        m_slot2On = settings.m_slot2On;
    }
    if (settingsKeys.contains("tdmaStereo")) {
        m_tdmaStereo = settings.m_tdmaStereo;
    }
    if (settingsKeys.contains("angel61FileDirectory")) {
        m_angel61FileDirectory = settings.m_angel61FileDirectory;
    }
    if (settingsKeys.contains("codecModelDirectory")) {
        m_codecModelDirectory = settings.m_codecModelDirectory;
    }
    if (settingsKeys.contains("angel61FileMaxKB")) {
        m_angel61FileMaxKB = settings.m_angel61FileMaxKB;
    }
    if (settingsKeys.contains("angel61FileConfirm")) {
        m_angel61FileConfirm = settings.m_angel61FileConfirm;
    }
    if (settingsKeys.contains("angel61Groups")) {
        m_angel61Groups = settings.m_angel61Groups;
    }
    if (settingsKeys.contains("contactReportEnabled")) m_contactReportEnabled = settings.m_contactReportEnabled;
    if (settingsKeys.contains("contactReportCallsign")) m_contactReportCallsign = settings.m_contactReportCallsign;
    if (settingsKeys.contains("dmrColorCode")) {
        m_dmrColorCode = settings.m_dmrColorCode;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("highPassFilter")) {
        m_highPassFilter = settings.m_highPassFilter;
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
    if (settingsKeys.contains("ambeFeatureIndex")) {
        m_ambeFeatureIndex = settings.m_ambeFeatureIndex;
    }
    if (settingsKeys.contains("connectAMBE")) {
        m_connectAMBE = settings.m_connectAMBE;
    }
}

QString DSDDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("volume") || force) {
        ostr << " m_volume: " << m_volume;
    }
    if (settingsKeys.contains("protocol") || force) {
        ostr << " m_protocol: " << getProtocolName(m_protocol);
    }
    if (settingsKeys.contains("p25p2Wacn") || force) {
        ostr << " m_p25p2Wacn: " << m_p25p2Wacn;
    }
    if (settingsKeys.contains("p25p2Sysid") || force) {
        ostr << " m_p25p2Sysid: " << m_p25p2Sysid;
    }
    if (settingsKeys.contains("p25p2Nac") || force) {
        ostr << " m_p25p2Nac: " << m_p25p2Nac;
    }
    if (settingsKeys.contains("coherent") || force) {
        ostr << " m_coherent: " << m_coherent;
    }
    if (settingsKeys.contains("audioMute") || force) {
        ostr << " m_audioMute: " << m_audioMute;
    }
    if (settingsKeys.contains("slot1On") || force) {
        ostr << " m_slot1On: " << m_slot1On;
    }
    if (settingsKeys.contains("slot2On") || force) {
        ostr << " m_slot2On: " << m_slot2On;
    }
    if (settingsKeys.contains("tdmaStereo") || force) {
        ostr << " m_tdmaStereo: " << m_tdmaStereo;
    }
    if (settingsKeys.contains("angel61FileConfirm") || force) {
        ostr << " m_angel61FileConfirm: " << m_angel61FileConfirm;
    }
    if (settingsKeys.contains("angel61FileDirectory") || force) {
        ostr << " m_angel61FileDirectory: " << m_angel61FileDirectory.toStdString();
    }
    if (settingsKeys.contains("codecModelDirectory") || force) {
        ostr << " m_codecModelDirectory: " << m_codecModelDirectory.toStdString();
    }
    if (settingsKeys.contains("angel61FileMaxKB") || force) {
        ostr << " m_angel61FileMaxKB: " << m_angel61FileMaxKB;
    }
    if (settingsKeys.contains("angel61Groups") || force) {
        ostr << " m_angel61Groups: " << m_angel61Groups.toStdString();
    }
    if (settingsKeys.contains("contactReportEnabled") || force) ostr << " m_contactReportEnabled: " << m_contactReportEnabled;
    if (settingsKeys.contains("contactReportCallsign") || force) ostr << " m_contactReportCallsign: " << m_contactReportCallsign.toStdString();
    if (settingsKeys.contains("dmrColorCode") || force) {
        ostr << " m_dmrColorCode: " << m_dmrColorCode;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("highPassFilter") || force) {
        ostr << " m_highPassFilter: " << m_highPassFilter;
    }
    if (settingsKeys.contains("traceLengthMutliplier") || force) {
        ostr << " m_traceLengthMutliplier: " << m_traceLengthMutliplier;
    }
    if (settingsKeys.contains("traceStroke") || force) {
        ostr << " m_traceStroke: " << m_traceStroke;
    }
    if (settingsKeys.contains("traceDecay") || force) {
        ostr << " m_traceDecay: " << m_traceDecay;
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
    if (settingsKeys.contains("ambeFeatureIndex") || force) {
        ostr << " m_ambeFeatureIndex: " << m_ambeFeatureIndex;
    }
    if (settingsKeys.contains("connectAMBE") || force) {
        ostr << " m_connectAMBE: " << m_connectAMBE;
    }

    return QString(ostr.str().c_str());
}
