///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "leansdr/dvb.h"
#include "leansdr/sdr.h"

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "datvdemodsettings.h"

DATVDemodSettings::DATVDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DATVDemodSettings::resetToDefaults()
{
    m_rgbColor = QColor(Qt::magenta).rgb();
    m_title = "DATV Demodulator";
    m_rfBandwidth = 512000;
    m_centerFrequency = 0;
    m_standard = DVB_S;
    m_modulation = BPSK;
    m_fec = FEC12;
    m_softLDPC = false;
    m_softLDPCMaxTrials = 8;
    m_maxBitflips = 0;
    m_symbolRate = 250000;
    m_notchFilters = 0;
    m_allowDrift = false;
    m_fastLock = false;
    m_filter = SAMP_LINEAR;
    m_hardMetric = false;
    m_rollOff = 0.35;
    m_viterbi = false;
    m_excursion = 10;
    m_audioMute = false;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_audioVolume = 0;
    m_videoMute = false;
    m_udpTSAddress = "127.0.0.1";
    m_udpTSPort = 8882;
    m_udpTS = false;
    m_playerEnable = true;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray DATVDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(2, m_rfBandwidth);
    s.writeS32(3, m_centerFrequency);
    s.writeS32(4, (int) m_standard);
    s.writeS32(5, (int) m_modulation);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeString(8, m_title);
    s.writeS32(9, (int) m_fec);
    s.writeBool(10, m_audioMute);
    s.writeS32(11, m_symbolRate);
    s.writeS32(12, m_notchFilters);
    s.writeBool(13, m_allowDrift);
    s.writeBool(14, m_fastLock);
    s.writeS32(15, (int) m_filter);
    s.writeBool(16, m_hardMetric);
    s.writeFloat(17, m_rollOff);
    s.writeBool(18, m_viterbi);
    s.writeS32(19, m_excursion);
    s.writeString(20, m_audioDeviceName);
    s.writeS32(21, m_audioVolume);
    s.writeBool(22, m_videoMute);
    s.writeString(23, m_udpTSAddress);
    s.writeU32(24, m_udpTSPort);
    s.writeBool(25, m_udpTS);
    s.writeS32(26, m_streamIndex);
    s.writeBool(27, m_useReverseAPI);
    s.writeString(28, m_reverseAPIAddress);
    s.writeU32(29, m_reverseAPIPort);
    s.writeU32(30, m_reverseAPIDeviceIndex);
    s.writeU32(31, m_reverseAPIChannelIndex);
    s.writeBool(32, m_softLDPC);
    s.writeS32(33, m_maxBitflips);
    s.writeS32(35, m_softLDPCMaxTrials);
    s.writeBool(36, m_playerEnable);

    if (m_rollupState) {
        s.writeBlob(37, m_rollupState->serialize());
    }

    s.writeS32(38, m_workspaceIndex);
    s.writeBlob(39, m_geometryBytes);
    s.writeBool(40, m_hidden);

    return s.final();
}

bool DATVDemodSettings::deserialize(const QByteArray& data)
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
        quint32 utmp;
        QString strtmp;

        d.readS32(2, &m_rfBandwidth, 512000);
        d.readS32(3, &m_centerFrequency, 0);

        d.readS32(4, &tmp, (int) DVB_S);
        tmp = tmp < 0 ? 0 : tmp > (int) DVB_S2 ? (int) DVB_S2 : tmp;
        m_standard = (dvb_version) tmp;

        d.readS32(5, &tmp, (int) BPSK);
        tmp = tmp < 0 ? 0 : tmp >= (int) MOD_UNSET ? (int) MOD_UNSET - 1 : tmp;
        m_modulation = (DATVModulation) tmp;

        if (m_channelMarker)
        {
            d.readBlob(6, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(7, &m_rgbColor, QColor(Qt::magenta).rgb());
        d.readString(8, &m_title, "DATV Demodulator");

        d.readS32(9, &tmp, (int) FEC12);
        tmp = tmp < 0 ? 0 : tmp >= (int) RATE_UNSET ? (int) RATE_UNSET - 1 : tmp;
        m_fec = (DATVCodeRate) tmp;

        d.readBool(10, &m_audioMute, false);
        d.readS32(11, &m_symbolRate, 250000);
        d.readS32(12, &m_notchFilters, 0);
        d.readBool(13, &m_allowDrift, false);
        d.readBool(14, &m_fastLock, false);

        d.readS32(15, &tmp, (int) SAMP_LINEAR);
        tmp = tmp < 0 ? 0 : tmp > (int) SAMP_RRC ? (int) SAMP_RRC : tmp;
        m_filter = (dvb_sampler) tmp;

        d.readBool(16, &m_hardMetric, false);
        d.readFloat(17, &m_rollOff, 0.35);
        d.readBool(18, &m_viterbi, false);
        d.readS32(19, &m_excursion, 10);
        d.readString(20, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(21, &m_audioVolume, 0);
        d.readBool(22, &m_videoMute, false);
        d.readString(23, &m_udpTSAddress, "127.0.0.1");
        d.readU32(24, &utmp, 8882);
        m_udpTSPort = utmp < 1024 ? 1024 : utmp > 65536 ? 65535 : utmp;
        d.readBool(25, &m_udpTS, false);
        d.readS32(26, &m_streamIndex, 0);
        d.readBool(27, &m_useReverseAPI, false);
        d.readString(28, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(29, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(30, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(31, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        d.readBool(32, &m_softLDPC, false);
        d.readS32(33, &m_maxBitflips, 0);
        d.readS32(35, &tmp, 8);
        m_softLDPCMaxTrials = tmp < 1 ? 1 : tmp > m_softLDPCMaxMaxTrials ? m_softLDPCMaxMaxTrials : tmp;
        d.readBool(36, &m_playerEnable, true);

        if (m_rollupState)
        {
            d.readBlob(37, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(38, &m_workspaceIndex, 0);
        d.readBlob(39, &m_geometryBytes);
        d.readBool(40, &m_hidden, false);

        validateSystemConfiguration();

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void DATVDemodSettings::debug(const QString& msg) const
{
    qDebug() << msg
        << " m_standard: " << m_standard
        << " m_allowDrift: " << m_allowDrift
        << " m_rfBandwidth: " << m_rfBandwidth
        << " m_centerFrequency: " << m_centerFrequency
        << " m_fastLock: " << m_fastLock
        << " m_hardMetric: " << m_hardMetric
        << " m_filter: " << m_filter
        << " m_rollOff: " << m_rollOff
        << " m_viterbi: " << m_viterbi
        << " m_fec: " << m_fec
        << " m_softLDPC: " << m_softLDPC
        << " m_softLDPCMaxTrials: " << m_softLDPCMaxTrials
        << " m_maxBitflips: " << m_maxBitflips
        << " m_modulation: " << m_modulation
        << " m_standard: " << m_standard
        << " m_notchFilters: " << m_notchFilters
        << " m_symbolRate: " << m_symbolRate
        << " m_excursion: " << m_excursion
        << " m_audioMute: " << m_audioMute
        << " m_audioDeviceName: " << m_audioDeviceName
        << " m_audioVolume: " << m_audioVolume
        << " m_videoMute: " << m_videoMute
        << " m_udpTS: " << m_udpTS
        << " m_udpTSAddress: " << m_udpTSAddress
        << " m_udpTSPort: " << m_udpTSPort
        << " m_playerEnable: " << m_playerEnable;
}

bool DATVDemodSettings::isDifferent(const DATVDemodSettings& other)
{
    return ((m_allowDrift != other.m_allowDrift)
        || (m_fastLock != other.m_fastLock)
        || (m_hardMetric != other.m_hardMetric)
        || (m_filter != other.m_filter)
        || (m_rollOff != other.m_rollOff)
        || (m_viterbi != other.m_viterbi)
        || (m_fec != other.m_fec)
        || (m_softLDPC != other.m_softLDPC)
        || (m_softLDPCMaxTrials != other.m_softLDPCMaxTrials)
        || (m_maxBitflips != other.m_maxBitflips)
        || (m_modulation != other.m_modulation)
        || (m_notchFilters != other.m_notchFilters)
        || (m_symbolRate != other.m_symbolRate)
        || (m_excursion != other.m_excursion)
        || (m_standard != other.m_standard)
        || (m_playerEnable != other.m_playerEnable));
}

void DATVDemodSettings::validateSystemConfiguration()
{
    qDebug("DATVDemodSettings::validateSystemConfiguration: m_standard: %d m_modulation: %d m_fec: %d",
        (int) m_standard, (int) m_modulation, (int) m_fec);

    if (m_standard == DVB_S)
    {
        // The ETSI standard for DVB-S specify only QPSK (EN 300 421) with a later extension to BPSK (TR 101 198)
        // but amateur radio also use extra modes thus only the modes very specific to DVB-S2(X) are restricted
        if ((m_modulation == APSK16) || (m_modulation == APSK32) || (m_modulation == APSK64E))
        {
            m_modulation = QPSK; // Fall back to QPSK
        }
        // Here we follow ETSI standard for DVB-S retaining only the valod code rates
        if ((m_fec != FEC12) && (m_fec!= FEC23) && (m_fec!= FEC34) && (m_fec!= FEC56) && (m_fec!= FEC78)) {
            m_fec = FEC12;
        }
    }
    else if (m_standard == DVB_S2)
    {
        // Here we try to follow ETSI standard for DVB-S2 (EN 300 307 1) and DVB-S2X (EN 302 307 2) together for the available modes
        if ((m_modulation == BPSK) || (m_modulation == QAM16) || (m_modulation == QAM64) || (m_modulation == QAM256))
        {
            m_modulation = QPSK; // Fall back to QPSK
        }
        // Here we also try to follow ETSI standard depending on the modulation (EN 300 307 1 Table 1)
        if (m_modulation == QPSK)
        {
            if ((m_fec != FEC14) && (m_fec != FEC13) && (m_fec != FEC25)
             && (m_fec != FEC12) && (m_fec != FEC35) && (m_fec != FEC23)
             && (m_fec != FEC34) && (m_fec != FEC45) && (m_fec != FEC56)
             && (m_fec != FEC89) && (m_fec != FEC910)) {
                 m_fec = FEC12;
             }
        }
        else if (m_modulation == PSK8)
        {
            if ((m_fec != FEC35) && (m_fec != FEC23) && (m_fec != FEC34)
             && (m_fec != FEC56) && (m_fec != FEC89) && (m_fec != FEC910)) {
                 m_fec = FEC34;
             }
        }
        else if (m_modulation == APSK16)
        {
            if ((m_fec != FEC23) && (m_fec != FEC34) && (m_fec != FEC45)
             && (m_fec != FEC56) && (m_fec != FEC89) && (m_fec != FEC910)) {
                 m_fec = FEC34;
             }
        }
        else if (m_modulation == APSK32)
        {
            if ((m_fec != FEC34) && (m_fec != FEC45) && (m_fec != FEC56)
             && (m_fec != FEC89) && (m_fec != FEC910)) {
                 m_fec = FEC34;
             }
        }
        // DVB-S2X has many mode code rates but here we deal only with the ones available
        else if (m_modulation == APSK64E)
        {
            if ((m_fec != FEC45) && (m_fec != FEC56)) {
                m_fec = FEC45;
            }
        }
    }
}

void DATVDemodSettings::applySettings(const QStringList& settingsKeys, const DATVDemodSettings& settings)
{
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("standard")) {
        m_standard = settings.m_standard;
    }
    if (settingsKeys.contains("modulation")) {
        m_modulation = settings.m_modulation;
    }
    if (settingsKeys.contains("fec")) {
        m_fec = settings.m_fec;
    }
    if (settingsKeys.contains("softLDPC")) {
        m_softLDPC = settings.m_softLDPC;
    }
    if (settingsKeys.contains("softLDPCMaxTrials")) {
        m_softLDPCMaxTrials = settings.m_softLDPCMaxTrials;
    }
    if (settingsKeys.contains("maxBitflips")) {
        m_maxBitflips = settings.m_maxBitflips;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("symbolRate")) {
        m_symbolRate = settings.m_symbolRate;
    }
    if (settingsKeys.contains("notchFilters")) {
        m_notchFilters = settings.m_notchFilters;
    }
    if (settingsKeys.contains("allowDrift")) {
        m_allowDrift = settings.m_allowDrift;
    }
    if (settingsKeys.contains("fastLock")) {
        m_fastLock = settings.m_fastLock;
    }
    if (settingsKeys.contains("filter")) {
        m_filter = settings.m_filter;
    }
    if (settingsKeys.contains("hardMetric")) {
        m_hardMetric = settings.m_hardMetric;
    }
    if (settingsKeys.contains("rollOff")) {
        m_rollOff = settings.m_rollOff;
    }
    if (settingsKeys.contains("viterbi")) {
        m_viterbi = settings.m_viterbi;
    }
    if (settingsKeys.contains("excursion")) {
        m_excursion = settings.m_excursion;
    }
    if (settingsKeys.contains("audioVolume")) {
        m_audioVolume = settings.m_audioVolume;
    }
    if (settingsKeys.contains("videoMute")) {
        m_videoMute = settings.m_videoMute;
    }
    if (settingsKeys.contains("udpTSAddress")) {
        m_udpTSAddress = settings.m_udpTSAddress;
    }
    if (settingsKeys.contains("udpTSPort")) {
        m_udpTSPort = settings.m_udpTSPort;
    }
    if (settingsKeys.contains("udpTS")) {
        m_udpTS = settings.m_udpTS;
    }
    if (settingsKeys.contains("playerEnable")) {
        m_playerEnable = settings.m_playerEnable;
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

QString DATVDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("standard") || force) {
        ostr << " m_standard: " << m_standard;
    }
    if (settingsKeys.contains("modulation") || force) {
        ostr << " m_modulation: " << m_modulation;
    }
    if (settingsKeys.contains("fec") || force) {
        ostr << " m_fec: " << m_fec;
    }
    if (settingsKeys.contains("softLDPC") || force) {
        ostr << " m_softLDPC: " << m_softLDPC;
    }
    if (settingsKeys.contains("softLDPCMaxTrials") || force) {
        ostr << " m_softLDPCMaxTrials: " << m_softLDPCMaxTrials;
    }
    if (settingsKeys.contains("maxBitflips") || force) {
        ostr << " m_maxBitflips: " << m_maxBitflips;
    }
    if (settingsKeys.contains("audioMute") || force) {
        ostr << " m_audioMute: " << m_audioMute;
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        ostr << " m_audioDeviceName: " << m_audioDeviceName.toStdString();
    }
    if (settingsKeys.contains("symbolRate") || force) {
        ostr << " m_symbolRate: " << m_symbolRate;
    }
    if (settingsKeys.contains("notchFilters") || force) {
        ostr << " m_notchFilters: " << m_notchFilters;
    }
    if (settingsKeys.contains("allowDrift") || force) {
        ostr << " m_allowDrift: " << m_allowDrift;
    }
    if (settingsKeys.contains("fastLock") || force) {
        ostr << " m_fastLock: " << m_fastLock;
    }
    if (settingsKeys.contains("filter") || force) {
        ostr << " m_filter: " << m_filter;
    }
    if (settingsKeys.contains("hardMetric") || force) {
        ostr << " m_hardMetric: " << m_hardMetric;
    }
    if (settingsKeys.contains("rollOff") || force) {
        ostr << " m_rollOff: " << m_rollOff;
    }
    if (settingsKeys.contains("viterbi") || force) {
        ostr << " m_viterbi: " << m_viterbi;
    }
    if (settingsKeys.contains("excursion") || force) {
        ostr << " m_excursion: " << m_excursion;
    }
    if (settingsKeys.contains("audioVolume") || force) {
        ostr << " m_audioVolume: " << m_audioVolume;
    }
    if (settingsKeys.contains("videoMute") || force) {
        ostr << " m_videoMute: " << m_videoMute;
    }
    if (settingsKeys.contains("udpTSAddress") || force) {
        ostr << " m_udpTSAddress: " << m_udpTSAddress.toStdString();
    }
    if (settingsKeys.contains("udpTSPort") || force) {
        ostr << " m_udpTSPort: " << m_udpTSPort;
    }
    if (settingsKeys.contains("udpTS") || force) {
        ostr << " m_udpTS: " << m_udpTS;
    }
    if (settingsKeys.contains("playerEnable") || force) {
        ostr << " m_playerEnable: " << m_playerEnable;
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

DATVDemodSettings::DATVModulation DATVDemodSettings::getModulationFromStr(const QString& str)
{
    if (str == "BPSK") {
        return BPSK;
    } else if (str == "QPSK") {
        return QPSK;
    } else if (str == "PSK8") {
        return PSK8;
    } else if (str == "APSK16") {
        return APSK16;
    } else if (str == "APSK32") {
        return APSK32;
    } else if (str == "APSK64E") {
        return APSK64E;
    } else if (str == "QAM16") {
        return QAM16;
    } else if (str == "QAM64") {
        return QAM64;
    } else if (str == "QAM256") {
        return QAM256;
    } else {
        return MOD_UNSET;
    }

}

DATVDemodSettings::DATVCodeRate DATVDemodSettings::getCodeRateFromStr(const QString& str)
{
    if (str == "1/4") {
        return FEC14;
    } else if (str == "1/3") {
        return FEC13;
    } else if (str == "2/5") {
        return FEC25;
    } else if (str == "1/2") {
        return FEC12;
    } else if (str == "3/5") {
        return FEC35;
    } else if (str == "2/3") {
        return FEC23;
    } else if (str == "3/4") {
        return FEC34;
    } else if (str == "4/5") {
        return FEC45;
    } else if (str == "5/6") {
        return FEC56;
    } else if (str == "7/8") {
        return FEC78;
    } else if (str == "8/9") {
        return FEC89;
    } else if (str == "9/10") {
        return FEC910;
    } else {
        return RATE_UNSET;
    }
}

QString DATVDemodSettings::getStrFromModulation(const DATVModulation modulation)
{
    if (modulation == BPSK) {
        return "BPSK";
    } else if (modulation == QPSK) {
        return "QPSK";
    } else if (modulation == PSK8) {
        return "PSK8";
    } else if (modulation == APSK16) {
        return "APSK16";
    } else if (modulation == APSK32) {
        return "APSK32";
    } else if (modulation == APSK64E) {
        return "APSK64E";
    } else if (modulation == QAM16) {
        return "QAM16";
    } else if (modulation == QAM64) {
        return "QAM64";
    } else if (modulation == QAM256) {
        return "QAM256";
    } else {
        return "N/A";
    }
}

QString DATVDemodSettings::getStrFromCodeRate(const DATVCodeRate codeRate)
{
    if (codeRate == FEC14) {
        return "1/4";
    } else if (codeRate == FEC13) {
        return "1/3";
    } else if (codeRate == FEC25) {
        return "2/5";
    } else if (codeRate == FEC12) {
        return "1/2";
    } else if (codeRate == FEC35) {
        return "3/5";
    } else if (codeRate == FEC23) {
        return "2/3";
    } else if (codeRate == FEC34) {
        return "3/4";
    } else if (codeRate == FEC45) {
        return "4/5";
    } else if (codeRate == FEC56) {
        return "5/6";
    } else if (codeRate == FEC78) {
        return "7/8";
    } else if (codeRate == FEC89) {
        return "8/9";
    } else if (codeRate == FEC910) {
        return "9/10";
    } else {
        return "N/A";
    }
}

void DATVDemodSettings::getAvailableModulations(dvb_version dvbStandard, std::vector<DATVModulation>& modulations)
{
    modulations.clear();

    if (dvbStandard == DVB_S)
    {
        modulations.push_back(BPSK);
        modulations.push_back(QPSK);
        modulations.push_back(PSK8);
        modulations.push_back(QAM16);
        modulations.push_back(QAM64);
        modulations.push_back(QAM256);
    }
    else if (dvbStandard == DVB_S2)
    {
        modulations.push_back(QPSK);
        modulations.push_back(PSK8);
        modulations.push_back(APSK16);
        modulations.push_back(APSK32);
        modulations.push_back(APSK64E);
    }
}

void DATVDemodSettings::getAvailableCodeRates(dvb_version dvbStandard, DATVModulation modulation, std::vector<DATVCodeRate>& codeRates)
{
    codeRates.clear();

    if (dvbStandard == DVB_S)
    {
        codeRates.push_back(FEC12);
        codeRates.push_back(FEC23);
        codeRates.push_back(FEC34);
        codeRates.push_back(FEC56);
        codeRates.push_back(FEC78);
    }
    else if (dvbStandard == DVB_S2)
    {
        if (modulation == QPSK)
        {
            codeRates.push_back(FEC14);
            codeRates.push_back(FEC13);
            codeRates.push_back(FEC25);
            codeRates.push_back(FEC12);
        }
        if ((modulation == QPSK) || (modulation == PSK8))
        {
            codeRates.push_back(FEC35);
        }
        if ((modulation == QPSK) || (modulation == PSK8) || (modulation == APSK16))
        {
            codeRates.push_back(FEC23);
        }
        if ((modulation == QPSK) || (modulation == PSK8) || (modulation == APSK16) || (modulation == APSK32))
        {
            codeRates.push_back(FEC34);
        }
        if ((modulation == QPSK) || (modulation == APSK16) || (modulation == APSK32) || (modulation == APSK64E))
        {
            codeRates.push_back(FEC45);
        }
        if ((modulation == QPSK) || (modulation == PSK8) || (modulation == APSK16) || (modulation == APSK32) || (modulation == APSK64E))
        {
            codeRates.push_back(FEC56);
        }
        if ((modulation == QPSK) || (modulation == PSK8) || (modulation == APSK16) || (modulation == APSK32))
        {
            codeRates.push_back(FEC89);
            codeRates.push_back(FEC910);
        }
    }
}

DATVDemodSettings::DATVCodeRate DATVDemodSettings::getCodeRateFromLeanDVBCode(int leanDVBCodeRate)
{
    if (leanDVBCodeRate == leansdr::code_rate::FEC12) {
        return DATVDemodSettings::DATVCodeRate::FEC12;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC13) {
        return DATVDemodSettings::DATVCodeRate::FEC13;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC14) {
        return DATVDemodSettings::DATVCodeRate::FEC14;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC23) {
        return DATVDemodSettings::DATVCodeRate::FEC23;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC25) {
        return DATVDemodSettings::DATVCodeRate::FEC25;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC34) {
        return DATVDemodSettings::DATVCodeRate::FEC34;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC35) {
        return DATVDemodSettings::DATVCodeRate::FEC35;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC45) {
        return DATVDemodSettings::DATVCodeRate::FEC45;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC46) {
        return DATVDemodSettings::DATVCodeRate::FEC46;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC56) {
        return DATVDemodSettings::DATVCodeRate::FEC56;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC78) {
        return DATVDemodSettings::DATVCodeRate::FEC78;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC89) {
        return DATVDemodSettings::DATVCodeRate::FEC89;
    } else if (leanDVBCodeRate == leansdr::code_rate::FEC910) {
        return DATVDemodSettings::DATVCodeRate::FEC910;
    } else {
        return DATVDemodSettings::DATVCodeRate::RATE_UNSET;
    }
}

DATVDemodSettings::DATVModulation DATVDemodSettings::getModulationFromLeanDVBCode(int leanDVBModulation)
{
    if (leanDVBModulation == leansdr::cstln_base::predef::APSK16) {
        return DATVDemodSettings::DATVModulation::APSK16;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::APSK32) {
        return DATVDemodSettings::DATVModulation::APSK32;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::APSK64E) {
        return DATVDemodSettings::DATVModulation::APSK64E;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::BPSK) {
        return DATVDemodSettings::DATVModulation::BPSK;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::PSK8) {
        return DATVDemodSettings::DATVModulation::PSK8;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::QAM16) {
        return DATVDemodSettings::DATVModulation::QAM16;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::QAM64) {
        return DATVDemodSettings::DATVModulation::QAM64;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::QAM256) {
        return DATVDemodSettings::DATVModulation::QAM256;
    } else if (leanDVBModulation == leansdr::cstln_base::predef::QPSK) {
        return DATVDemodSettings::DATVModulation::QPSK;
    } else {
        return DATVDemodSettings::DATVModulation::MOD_UNSET;
    }
}
