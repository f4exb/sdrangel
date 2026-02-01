///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
    m_fmDeviation = 3500.0;
    m_demodGain = 1.0;
    m_volume = 2.0;
    m_baudRate = 4800;
    m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
    m_squelch = -40.0;
    m_audioMute = false;
    m_enableCosineFiltering = false;
    m_syncOrConstellation = false;
    m_slot1On = true;
    m_slot2On = false;
    m_tdmaStereo = false;
    m_pllLock = true;
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

QByteArray DSDDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth/100.0);
    s.writeS32(3, m_demodGain*100.0);
    s.writeS32(4, m_fmDeviation/100.0);
    s.writeS32(5, m_squelch);
    s.writeBool(6, m_pllLock);
    s.writeU32(7, m_rgbColor);
    s.writeS32(8, m_squelchGate);
    s.writeS32(9, m_volume*10.0);
    s.writeS32(11, m_baudRate);
    s.writeBool(12, m_enableCosineFiltering);
    s.writeBool(13, m_syncOrConstellation);
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
        d.readS32(3, &tmp, 125);
        m_demodGain = tmp / 100.0;
        d.readS32(4, &tmp, 50);
        m_fmDeviation = tmp * 100.0;
        d.readS32(5, &tmp, -40);
        m_squelch = tmp < -100 ? tmp / 10.0 : tmp;
        d.readBool(6, &m_pllLock, true);
        d.readU32(7, &m_rgbColor);
        d.readS32(8, &m_squelchGate, 5);
        d.readS32(9, &tmp, 20);
        m_volume = tmp / 10.0;
        d.readS32(11, &m_baudRate, 4800);
        d.readBool(12, &m_enableCosineFiltering, false);
        d.readBool(13, &m_syncOrConstellation, false);
        d.readBool(14, &m_slot1On, false);
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
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (settingsKeys.contains("demodGain")) {
        m_demodGain = settings.m_demodGain;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("baudRate")) {
        m_baudRate = settings.m_baudRate;
    }
    if (settingsKeys.contains("squelchGate")) {
        m_squelchGate = settings.m_squelchGate;
    }
    if (settingsKeys.contains("squelch")) {
        m_squelch = settings.m_squelch;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("enableCosineFiltering")) {
        m_enableCosineFiltering = settings.m_enableCosineFiltering;
    }
    if (settingsKeys.contains("syncOrConstellation")) {
        m_syncOrConstellation = settings.m_syncOrConstellation;
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
    if (settingsKeys.contains("pllLock")) {
        m_pllLock = settings.m_pllLock;
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
    if (settingsKeys.contains("fmDeviation") || force) {
        ostr << " m_fmDeviation: " << m_fmDeviation;
    }
    if (settingsKeys.contains("demodGain") || force) {
        ostr << " m_demodGain: " << m_demodGain;
    }
    if (settingsKeys.contains("volume") || force) {
        ostr << " m_volume: " << m_volume;
    }
    if (settingsKeys.contains("baudRate") || force) {
        ostr << " m_baudRate: " << m_baudRate;
    }
    if (settingsKeys.contains("squelchGate") || force) {
        ostr << " m_squelchGate: " << m_squelchGate;
    }
    if (settingsKeys.contains("squelch") || force) {
        ostr << " m_squelch: " << m_squelch;
    }
    if (settingsKeys.contains("audioMute") || force) {
        ostr << " m_audioMute: " << m_audioMute;
    }
    if (settingsKeys.contains("enableCosineFiltering") || force) {
        ostr << " m_enableCosineFiltering: " << m_enableCosineFiltering;
    }
    if (settingsKeys.contains("syncOrConstellation") || force) {
        ostr << " m_syncOrConstellation: " << m_syncOrConstellation;
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
    if (settingsKeys.contains("pllLock") || force) {
        ostr << " m_pllLock: " << m_pllLock;
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
