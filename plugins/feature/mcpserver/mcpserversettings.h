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

#ifndef INCLUDE_FEATURE_MCPSERVERSETTINGS_H_
#define INCLUDE_FEATURE_MCPSERVERSETTINGS_H_

#include <QByteArray>
#include <QString>

class Serializable;

struct MCPServerSettings
{
    QString m_address;      //!< Address to bind the HTTP listener to
    quint32 m_port;         //!< TCP port to listen on
    QString m_token;        //!< Optional bearer token. Empty means no authentication
    QString m_captureDir;   //!< Directory IQ and audio captures are written to
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    MCPServerSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const MCPServerSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    static const quint32 m_defaultPort = 8092;
    static const quint32 m_minPort = 1024;
    static const quint32 m_maxPort = 65535;
    static bool isValidPort(quint32 port) { return (port >= m_minPort) && (port <= m_maxPort); }
    static QString getDefaultCaptureDir();
};

#endif // INCLUDE_FEATURE_MCPSERVERSETTINGS_H_
