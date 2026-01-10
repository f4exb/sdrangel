///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "audio/audiodevicemanager.h"

#include "denoisersettings.h"

const QStringList DenoiserSettings::m_channelURIs = {
    QStringLiteral("sdrangel.channel.amdemod"),
    QStringLiteral("sdrangel.channel.bfm"),
    QStringLiteral("sdrangel.channel.dabdemod"),
    QStringLiteral("sdrangel.channel.dsddemod"),
    QStringLiteral("sdrangel.channel.m17demod"),
    QStringLiteral("sdrangel.channel.nfmdemod"),
    QStringLiteral("sdrangel.channel.ssbdemod"),
    QStringLiteral("sdrangel.channel.wfmdemod"),
    QStringLiteral("sdrangel.channel.wdsprx"),
};

DenoiserSettings::DenoiserSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DenoiserSettings::resetToDefaults()
{
    m_denoiserType = DenoiserType::DenoiserType_RNnoise;
    m_title = "Denoiser";
    m_enableDenoiser = true;
    m_audioMute = false;
    m_volumeTenths = 10;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_rgbColor = 0xffd700; // gold
    m_useReverseAPI = false;
    m_reverseAPIAddress = "localhost";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_fileRecordName = "denoiser_record.wav";
    m_recordToFile = false;
    m_workspaceIndex = -1;
    m_geometryBytes.clear();
}

QByteArray DenoiserSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, static_cast<qint32>(m_denoiserType));
    s.writeString(2, m_title);
    s.writeU32(3, m_rgbColor);
    s.writeBool(4, m_useReverseAPI);
    s.writeString(5, m_reverseAPIAddress);
    s.writeU32(6, m_reverseAPIPort);
    s.writeU32(7, m_reverseAPIFeatureSetIndex);
    s.writeU32(8, m_reverseAPIFeatureIndex);
    s.writeString(9, m_fileRecordName);
    s.writeBool(10, m_recordToFile);
    s.writeS32(11, m_workspaceIndex);
    s.writeBlob(12, m_geometryBytes);

    if (m_rollupState) {
        s.writeBlob(13, m_rollupState->serialize());
    }

    s.writeBool(14, m_audioMute);
    s.writeString(15, m_audioDeviceName);
    s.writeBool(16, m_enableDenoiser);
    s.writeS32(17, m_volumeTenths);

    return s.final();
}

bool DenoiserSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        qint32 itmp;
        QByteArray bytetmp;
        uint32_t utmp;

        d.readS32(1, &itmp, 1);
        m_denoiserType = static_cast<DenoiserType>(itmp);
        d.readString(2, &m_title, "Denoiser");
        d.readU32(3, &m_rgbColor, 0xffd700); // gold
        d.readBool(4, &m_useReverseAPI, false);
        d.readString(5, &m_reverseAPIAddress, "localhost");
        d.readU32(6, &utmp, 8888);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(7, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(8, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readString(9, &m_fileRecordName, "denoiser_record.wav");
        d.readBool(10, &m_recordToFile, false);
        d.readS32(11, &m_workspaceIndex, -1);
        d.readBlob(12, &m_geometryBytes);

        if (m_rollupState)
        {
            d.readBlob(13, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readBool(14, &m_audioMute, false);
        d.readString(15, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(16, &m_enableDenoiser, true);
        d.readS32(17, &m_volumeTenths, 10);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void DenoiserSettings::applySettings(const QStringList& settingsKeys, const DenoiserSettings& settings)
{
    if (settingsKeys.contains("denoiserType")) {
        m_denoiserType = settings.m_denoiserType;
    }
    if (settingsKeys.contains("enableDenoiser")) {
        m_enableDenoiser = settings.m_enableDenoiser;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("volumeTenths")) {
        m_volumeTenths = settings.m_volumeTenths;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("fileRecordName")) {
        m_fileRecordName = settings.m_fileRecordName;
    }
    if (settingsKeys.contains("recordToFile")) {
        m_recordToFile = settings.m_recordToFile;
    }
}

QString DenoiserSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    QString debugString;

    if (settingsKeys.contains("denoiserType") || force) {
        debugString += QString("DenoiserType: %1 ").arg(static_cast<qint32>(m_denoiserType));
    }
    if (settingsKeys.contains("enableDenoiser") || force) {
        debugString += QString("Denoiser Enable: %1 ").arg(m_enableDenoiser ? "true" : "false");
    }
    if (settingsKeys.contains("audioMute") || force) {
        debugString += QString("Audio Mute: %1 ").arg(m_audioMute ? "true" : "false");
    }
    if (settingsKeys.contains("volumeTenths") || force) {
        debugString += QString("Volume : %1 ").arg(m_volumeTenths/10.0);
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        debugString += QString("Audio Device Name: %1 ").arg(m_audioDeviceName);
    }
    if (settingsKeys.contains("title") || force) {
        debugString += QString("Title: %1 ").arg(m_title);
    }
    if (settingsKeys.contains("rgbColor") || force) {
        debugString += QString("RGB Color: 0x%1 ").arg(QString::number(m_rgbColor, 16).rightJustified(6, '0'));
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        debugString += QString("Use Reverse API: %1 ").arg(m_useReverseAPI ? "true" : "false");
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        debugString += QString("Reverse API Address: %1 ").arg(m_reverseAPIAddress);
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        debugString += QString("Reverse API Port: %1 ").arg(m_reverseAPIPort);
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        debugString += QString("Reverse API Feature Set Index: %1 ").arg(m_reverseAPIFeatureSetIndex);
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex") || force) {
        debugString += QString("Reverse API Feature Index: %1 ").arg(m_reverseAPIFeatureIndex);
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        debugString += QString("Workspace Index: %1 ").arg(m_workspaceIndex);
    }
    if (settingsKeys.contains("fileRecordName") || force) {
        debugString += QString("File Record Name: %1 ").arg(m_fileRecordName);
    }
    if (settingsKeys.contains("recordToFile") || force) {
        debugString += QString("Record To File: %1 ").arg(m_recordToFile ? "true" : "false");
    }

    return debugString;
}
