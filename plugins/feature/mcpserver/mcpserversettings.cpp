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
#include <QStandardPaths>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "mcpserversettings.h"

MCPServerSettings::MCPServerSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void MCPServerSettings::resetToDefaults()
{
    m_address = "127.0.0.1";
    m_port = m_defaultPort;
    m_token = "";
    m_captureDir = getDefaultCaptureDir();
    m_title = "MCP Server";
    m_rgbColor = QColor(0, 140, 190).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray MCPServerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_address);
    s.writeU32(2, m_port);
    s.writeString(3, m_token);
    s.writeString(4, m_captureDir);
    s.writeString(5, m_title);
    s.writeU32(6, m_rgbColor);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIFeatureSetIndex);
    s.writeU32(11, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(12, m_rollupState->serialize());
    }

    s.writeS32(13, m_workspaceIndex);
    s.writeBlob(14, m_geometryBytes);

    return s.final();
}

bool MCPServerSettings::deserialize(const QByteArray& data)
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
        uint32_t utmp;

        d.readString(1, &m_address, "127.0.0.1");
        d.readU32(2, &utmp, m_defaultPort);

        if (isValidPort(utmp)) {
            m_port = utmp;
        } else {
            m_port = m_defaultPort;
        }

        d.readString(3, &m_token, "");
        d.readString(4, &m_captureDir, getDefaultCaptureDir());

        if (m_captureDir.trimmed().isEmpty()) {
            m_captureDir = getDefaultCaptureDir();
        }

        d.readString(5, &m_title, "MCP Server");
        d.readU32(6, &m_rgbColor, QColor(0, 140, 190).rgb());
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(11, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(12, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(13, &m_workspaceIndex, 0);
        d.readBlob(14, &m_geometryBytes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void MCPServerSettings::applySettings(const QStringList& settingsKeys, const MCPServerSettings& settings)
{
    if (settingsKeys.contains("address")) {
        m_address = settings.m_address;
    }
    if (settingsKeys.contains("port")) {
        m_port = settings.m_port;
    }
    if (settingsKeys.contains("token")) {
        m_token = settings.m_token;
    }
    if (settingsKeys.contains("captureDir")) {
        m_captureDir = settings.m_captureDir;
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
}

QString MCPServerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("address") || force) {
        ostr << " m_address: " << m_address.toStdString();
    }
    if (settingsKeys.contains("port") || force) {
        ostr << " m_port: " << m_port;
    }
    if (settingsKeys.contains("token") || force) {
        ostr << " m_token: " << (m_token.isEmpty() ? "(none)" : "(set)");
    }
    if (settingsKeys.contains("captureDir") || force) {
        ostr << " m_captureDir: " << m_captureDir.toStdString();
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        ostr << " m_reverseAPIFeatureSetIndex: " << m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex") || force) {
        ostr << " m_reverseAPIFeatureIndex: " << m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }

    return QString(ostr.str().c_str());
}

QString MCPServerSettings::getDefaultCaptureDir()
{
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    if (documents.isEmpty()) {
        documents = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }

    return documents + "/SDRangel/captures";
}
