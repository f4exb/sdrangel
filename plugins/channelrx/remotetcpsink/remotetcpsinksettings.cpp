///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019, 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "remotetcpsinksettings.h"

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"


RemoteTCPSinkSettings::RemoteTCPSinkSettings()
{
    resetToDefaults();
}

void RemoteTCPSinkSettings::resetToDefaults()
{
    m_channelSampleRate = 48000;
    m_inputFrequencyOffset = 0;
    m_gain = 0;
    m_sampleBits = 8;
    m_dataAddress = "0.0.0.0";
    m_dataPort = 1234;
    m_protocol = SDRA;
    m_iqOnly = false;
    m_compression = FLAC;
    m_compressionLevel = 5;
    m_blockSize = 16384;
    m_squelchEnabled = false;
    m_squelch = -100.0f;
    m_squelchGate = 0.001f;
    m_remoteControl = true;
    m_maxClients = 4;
    m_timeLimit = 0;
    m_maxSampleRate = 10000000;
    m_certificate = "";
    m_key = "";
    m_public = false;
    m_publicAddress = "";
    m_publicPort = 1234;
    m_minFrequency = 0;
    m_maxFrequency = 2000000000;
    m_antenna = "";
    m_location = "";
    m_ipBlacklist = QStringList();
    m_isotropic = true;
    m_azimuth = 0.0f;
    m_elevation = 0.0f;
    m_rotator = "None";
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "Remote TCP sink";
    m_channelMarker = nullptr;
    m_rollupState = nullptr;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray RemoteTCPSinkSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_channelSampleRate);
    s.writeS32(2, m_inputFrequencyOffset);
    s.writeS32(3, m_gain);
    s.writeU32(4, m_sampleBits);
    s.writeString(5, m_dataAddress);
    s.writeU32(6, m_dataPort);
    s.writeS32(7, (int)m_protocol);
    s.writeBool(42, m_iqOnly);
    s.writeS32(29, m_compression);
    s.writeS32(38, m_compressionLevel);
    s.writeS32(39, m_blockSize);
    s.writeBool(40, m_squelchEnabled);
    s.writeFloat(41, m_squelch);
    s.writeFloat(43, m_squelchGate);
    s.writeBool(23, m_remoteControl);
    s.writeS32(24, m_maxClients);
    s.writeS32(25, m_timeLimit);
    s.writeS32(28, m_maxSampleRate);
    s.writeString(26, m_certificate);
    s.writeString(27, m_key);

    s.writeBool(30, m_public);
    s.writeString(31, m_publicAddress);
    s.writeS32(32, m_publicPort);
    s.writeS64(33, m_minFrequency);
    s.writeS64(34, m_maxFrequency);
    s.writeString(35, m_antenna);
    s.writeString(37, m_location);
    s.writeList(36, m_ipBlacklist);
    s.writeBool(44, m_isotropic);
    s.writeFloat(45, m_azimuth);
    s.writeFloat(46, m_elevation);
    s.writeString(47, m_rotator);

    s.writeU32(8, m_rgbColor);
    s.writeString(9, m_title);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIDeviceIndex);
    s.writeU32(14, m_reverseAPIChannelIndex);
    s.writeS32(17, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(18, m_rollupState->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(19, m_channelMarker->serialize());
    }

    s.writeS32(20, m_workspaceIndex);
    s.writeBlob(21, m_geometryBytes);
    s.writeBool(22, m_hidden);

    return s.final();
}

bool RemoteTCPSinkSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t tmp;
        QString strtmp;
        QByteArray bytetmp;

        d.readS32(1, &m_channelSampleRate, 48000);
        d.readS32(2, &m_inputFrequencyOffset, 0);
        d.readS32(3, &m_gain, 0);
        d.readU32(4, &m_sampleBits, 8);
        d.readString(5, &m_dataAddress, "0.0.0.0");
        d.readU32(6, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_dataPort = tmp;
        } else {
            m_dataPort = 1234;
        }
        d.readS32(7, (int *)&m_protocol, (int)SDRA);
        d.readBool(42, &m_iqOnly, false);
        d.readS32(29, (int *)&m_compression, (int)FLAC);
        d.readS32(38, &m_compressionLevel, 5);
        d.readS32(39, &m_blockSize, 16384);
        d.readBool(40, &m_squelchEnabled, false);
        d.readFloat(41, &m_squelch, -100.0f);
        d.readFloat(43, &m_squelchGate, 0.001f);
        d.readBool(23, &m_remoteControl, true);
        d.readS32(24, &m_maxClients, 4);
        d.readS32(25, &m_timeLimit, 0);
        d.readS32(28, &m_maxSampleRate, 10000000);

        d.readString(26, &m_certificate, "");
        d.readString(27, &m_key, "");

        d.readBool(30, &m_public, false);
        d.readString(31, &m_publicAddress, "");
        d.readS32(32, &m_publicPort, 1234);
        d.readS64(33, &m_minFrequency, 0);
        d.readS64(34, &m_maxFrequency, 2000000000);
        d.readString(35, &m_antenna, "");
        d.readString(37, &m_location, "");
        d.readList(36, &m_ipBlacklist);
        d.readBool(44, &m_isotropic, true);
        d.readFloat(45, &m_azimuth, 0.0f);
        d.readFloat(46, &m_elevation, 0.0f);
        d.readString(47, &m_rotator, "None");

        d.readU32(8, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readString(9, &m_title, "Remote TCP sink");
        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_reverseAPIPort = tmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(13, &tmp, 0);
        m_reverseAPIDeviceIndex = tmp > 99 ? 99 : tmp;
        d.readU32(14, &tmp, 0);
        m_reverseAPIChannelIndex = tmp > 99 ? 99 : tmp;
        d.readS32(17, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(18, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(19, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(20, &m_workspaceIndex, 0);
        d.readBlob(21, &m_geometryBytes);
        d.readBool(22, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RemoteTCPSinkSettings::applySettings(const QStringList& settingsKeys, const RemoteTCPSinkSettings& settings)
{
    if (settingsKeys.contains("channelSampleRate")) {
        m_channelSampleRate = settings.m_channelSampleRate;
    }
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("sampleBits")) {
        m_sampleBits = settings.m_sampleBits;
    }
    if (settingsKeys.contains("dataAddress")) {
        m_dataAddress = settings.m_dataAddress;
    }
    if (settingsKeys.contains("dataPort")) {
        m_dataPort = settings.m_dataPort;
    }
    if (settingsKeys.contains("protocol")) {
        m_protocol = settings.m_protocol;
    }
    if (settingsKeys.contains("iqOnly")) {
        m_iqOnly = settings.m_iqOnly;
    }
    if (settingsKeys.contains("compression")) {
        m_compression = settings.m_compression;
    }
    if (settingsKeys.contains("compressionLevel")) {
        m_compressionLevel = settings.m_compressionLevel;
    }
    if (settingsKeys.contains("blockSize")) {
        m_blockSize = settings.m_blockSize;
    }
    if (settingsKeys.contains("squelchEnabled")) {
        m_squelchEnabled = settings.m_squelchEnabled;
    }
    if (settingsKeys.contains("squelch")) {
        m_squelch = settings.m_squelch;
    }
    if (settingsKeys.contains("squelchGate")) {
        m_squelchGate = settings.m_squelchGate;
    }
    if (settingsKeys.contains("remoteControl")) {
        m_remoteControl = settings.m_remoteControl;
    }
    if (settingsKeys.contains("maxClients")) {
        m_maxClients = settings.m_maxClients;
    }
    if (settingsKeys.contains("timeLimit")) {
        m_timeLimit = settings.m_timeLimit;
    }
    if (settingsKeys.contains("maxSampleRate")) {
        m_maxSampleRate = settings.m_maxSampleRate;
    }
    if (settingsKeys.contains("certificate")) {
        m_certificate = settings.m_certificate;
    }
    if (settingsKeys.contains("key")) {
        m_key = settings.m_key;
    }
    if (settingsKeys.contains("public")) {
        m_public = settings.m_public;
    }
    if (settingsKeys.contains("publicAddress")) {
        m_publicAddress = settings.m_publicAddress;
    }
    if (settingsKeys.contains("publicPort")) {
        m_publicPort = settings.m_publicPort;
    }
    if (settingsKeys.contains("minFrequency")) {
        m_minFrequency = settings.m_minFrequency;
    }
    if (settingsKeys.contains("maxFrequency")) {
        m_maxFrequency = settings.m_maxFrequency;
    }
    if (settingsKeys.contains("antenna")) {
        m_antenna = settings.m_antenna;
    }
    if (settingsKeys.contains("ipBlacklist")) {
        m_ipBlacklist = settings.m_ipBlacklist;
    }
    if (settingsKeys.contains("isotrophic")) {
        m_isotropic = settings.m_isotropic;
    }
    if (settingsKeys.contains("azimuth")) {
        m_azimuth = settings.m_azimuth;
    }
    if (settingsKeys.contains("elevation")) {
        m_elevation = settings.m_elevation;
    }
    if (settingsKeys.contains("rotator")) {
        m_rotator = settings.m_rotator;
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
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString RemoteTCPSinkSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("channelSampleRate") || force) {
        ostr << " m_channelSampleRate: " << m_channelSampleRate;
    }
    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("sampleBits") || force) {
        ostr << " m_sampleBits: " << m_sampleBits;
    }
    if (settingsKeys.contains("dataAddress") || force) {
        ostr << " m_dataAddress: " << m_dataAddress.toStdString();
    }
    if (settingsKeys.contains("dataPort") || force) {
        ostr << " m_dataPort: " << m_dataPort;
    }
    if (settingsKeys.contains("protocol") || force) {
        ostr << " m_protocol: " << m_protocol;
    }
    if (settingsKeys.contains("iqOnly") || force) {
        ostr << " m_iqOnly: " << m_iqOnly;
    }
    if (settingsKeys.contains("compression") || force) {
        ostr << " m_compression: " << m_compression;
    }
    if (settingsKeys.contains("compressionLevel") || force) {
        ostr << " m_compressionLevel: " << m_compressionLevel;
    }
    if (settingsKeys.contains("blockSize") || force) {
        ostr << " m_blockSize: " << m_blockSize;
    }
    if (settingsKeys.contains("squelchEnabled") || force) {
        ostr << " m_squelchEnabled: " << m_squelchEnabled;
    }
    if (settingsKeys.contains("squelch") || force) {
        ostr << " m_squelch: " << m_squelch;
    }
    if (settingsKeys.contains("squelchGate") || force) {
        ostr << " m_squelchGate: " << m_squelchGate;
    }
    if (settingsKeys.contains("remoteControl") || force) {
        ostr << " m_remoteControl: " << m_remoteControl;
    }
    if (settingsKeys.contains("maxClients") || force) {
        ostr << " m_maxClients: " << m_maxClients;
    }
    if (settingsKeys.contains("timeLimit") || force) {
        ostr << " m_timeLimit: " << m_timeLimit;
    }
    if (settingsKeys.contains("maxSampleRate") || force) {
        ostr << " m_maxSampleRate: " << m_maxSampleRate;
    }
    if (settingsKeys.contains("certificate") || force) {
        ostr << " m_certificate: " << m_certificate.toStdString();
    }
    if (settingsKeys.contains("key") || force) {
        ostr << " m_key: " << m_key.toStdString();
    }
    if (settingsKeys.contains("public") || force) {
        ostr << " m_public: " << m_public;
    }
    if (settingsKeys.contains("publicAddress") || force) {
        ostr << " m_publicAddress: " << m_publicAddress.toStdString();
    }
    if (settingsKeys.contains("publicPort") || force) {
        ostr << " m_publicPort: " << m_publicPort;
    }
    if (settingsKeys.contains("minFrequency") || force) {
        ostr << " m_minFrequency: " << m_minFrequency;
    }
    if (settingsKeys.contains("maxFrequency") || force) {
        ostr << " m_maxFrequency: " << m_maxFrequency;
    }
    if (settingsKeys.contains("antenna") || force) {
        ostr << " m_antenna: " << m_antenna.toStdString();
    }
    if (settingsKeys.contains("ipBlacklist") || force) {
        ostr << " m_ipBlacklist: " << m_ipBlacklist.join(" ").toStdString();
    }
    if (settingsKeys.contains("isotrophic") || force) {
        ostr << " m_isotropic: " << m_isotropic;
    }
    if (settingsKeys.contains("azimuth") || force) {
        ostr << " m_azimuth: " << m_azimuth;
    }
    if (settingsKeys.contains("elevation") || force) {
        ostr << " m_elevation: " << m_elevation;
    }
    if (settingsKeys.contains("rotator") || force) {
        ostr << " m_rotator: " << m_rotator.toStdString();
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
