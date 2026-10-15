///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                              //
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

#ifndef INCLUDE_FEATURE_MCPCODEXCONFIG_H_
#define INCLUDE_FEATURE_MCPCODEXCONFIG_H_

#include <QString>
#include <QStringList>

// Writes this server into the OpenAI Codex configuration, so that it can be set up from the
// feature's window rather than by hand editing TOML.
//
// Codex speaks the streamable HTTP transport, so it reaches the server directly and needs no
// bridge. What it needs is four lines in its own configuration file:
//
//     [mcp_servers.sdrangel]
//     url = "http://127.0.0.1:8092/mcp"
//
// The file belongs to Codex and is usually hand written, so the whole of it is preserved apart
// from that one table, which is replaced when it is already there. There is no TOML library
// here and pulling one in for four lines would be a poor trade, so the table is found and
// replaced as text. That is safe for what is written here, and is why nothing else is touched.
class MCPCodexConfig
{
public:
    enum Result
    {
        Added,      //!< The table was not there and has been appended
        Updated,    //!< The table was there and has been replaced
        Unchanged,  //!< The table was there and already said this
        Failed
    };

    //!< $CODEX_HOME/config.toml, or ~/.codex/config.toml
    static QString configPath();

    //!< Adds or updates [mcp_servers.<serverName>]. message describes what happened either way
    static Result addServer(const QString& serverName, const QString& url, const QString& token,
        QString& message);

    //!< The URL a client on this machine should use to reach a server bound to address:port
    static QString serverUrl(const QString& address, quint32 port);

private:
    static QString comment() { return "# Written by SDRangel's MCP Server feature"; }
    static QString tomlString(const QString& value);
    //!< Net bracket depth added by the line, ignoring quoted text and comments
    static int bracketDelta(const QString& line);
    //!< The table with only the managed keys replaced, everything else the user put there kept
    static QStringList rewriteTable(const QStringList& table, const QString& serverName,
        const QString& url, const QString& token);
    static QString tableFor(const QString& serverName, const QString& url, const QString& token,
        const QString& newline);
    //!< [start, end) lines of the table and any of its sub tables, or (-1, -1) when absent
    static void findTable(const QStringList& lines, const QString& serverName, int& start, int& end);
};

#endif // INCLUDE_FEATURE_MCPCODEXCONFIG_H_
