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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

#include "mcpcodexconfig.h"

QString MCPCodexConfig::configPath()
{
    // Codex reads CODEX_HOME first, and a user with several profiles will have set it
    QString home = qEnvironmentVariable("CODEX_HOME");

    if (home.trimmed().isEmpty()) {
        home = QDir(QDir::homePath()).filePath(".codex");
    }

    return QDir(home).filePath("config.toml");
}

QString MCPCodexConfig::serverUrl(const QString& address, quint32 port)
{
    // The listener may be bound to every interface, which is not an address a client can use
    QString host = address.trimmed();

    if (host.isEmpty() || (host == "0.0.0.0") || (host == "::") || (host == "*")) {
        host = "127.0.0.1";
    }

    if (host.contains(':') && !host.startsWith('[')) {
        host = "[" + host + "]"; // bare IPv6
    }

    return QString("http://%1:%2/mcp").arg(host).arg(port);
}

QString MCPCodexConfig::tomlString(const QString& value)
{
    QString escaped = value;
    escaped.replace('\\', "\\\\");
    escaped.replace('"', "\\\"");
    escaped.replace('\n', "\\n");
    escaped.replace('\r', "\\r");
    escaped.replace('\t', "\\t");
    return "\"" + escaped + "\"";
}

QString MCPCodexConfig::tableFor(const QString& serverName, const QString& url, const QString& token,
    const QString& newline)
{
    QStringList lines;
    lines.append(QString("[mcp_servers.%1]").arg(serverName));
    // Inside the table, so that replacing the table replaces the comment with it
    lines.append(comment());
    lines.append(QString("url = %1").arg(tomlString(url)));

    if (!token.trimmed().isEmpty())
    {
        // bearer_token_env_var would keep the token out of the file, but then it would have to
        // be in the environment Codex is started from, which is not something this can arrange
        lines.append(QString("http_headers = { \"Authorization\" = %1 }")
            .arg(tomlString("Bearer " + token)));
    }

    return lines.join(newline) + newline;
}

void MCPCodexConfig::findTable(const QStringList& lines, const QString& serverName, int& start, int& end)
{
    start = -1;
    end = -1;

    // [ mcp_servers.name ] with the spacing TOML allows, and the name optionally quoted
    const QString name = QRegularExpression::escape(serverName);
    QRegularExpression header(QString("^\\s*\\[\\s*mcp_servers\\s*\\.\\s*\"?%1\"?\\s*\\]").arg(name));
    QRegularExpression child(QString("^\\s*\\[\\s*mcp_servers\\s*\\.\\s*\"?%1\"?\\s*\\.").arg(name));
    QRegularExpression anyTable("^\\s*\\[");

    for (int i = 0; i < lines.size(); i++)
    {
        if (start < 0)
        {
            if (header.match(lines[i]).hasMatch()) {
                start = i;
            }

            continue;
        }

        // Sub tables such as [mcp_servers.name.env] belong to it and go with it
        if (child.match(lines[i]).hasMatch()) {
            continue;
        }

        if (anyTable.match(lines[i]).hasMatch())
        {
            end = i;
            break;
        }
    }

    if (start >= 0)
    {
        if (end < 0) {
            end = lines.size();
        }

        // Give back any blank lines at the end of the table, so that repeated writes neither
        // grow nor lose the spacing between tables
        while ((end > start + 1) && lines[end - 1].trimmed().isEmpty()) {
            end--;
        }
    }
}

// How much deeper the brackets are at the end of the line than at the start of it. Quoted
// text and comments do not count, so that a bracket inside a path or a remark is not one.
int MCPCodexConfig::bracketDelta(const QString& line)
{
    int depth = 0;
    QChar quote;

    for (int i = 0; i < line.size(); i++)
    {
        const QChar c = line[i];

        if (!quote.isNull())
        {
            // Only the basic string form has escapes; a literal one ends at the next quote
            if ((c == '\\') && (quote == '"')) {
                i++;
            } else if (c == quote) {
                quote = QChar();
            }

            continue;
        }

        if ((c == '"') || (c == '\'')) {
            quote = c;
        } else if (c == '#') {
            break;
        } else if ((c == '[') || (c == '{')) {
            depth++;
        } else if ((c == ']') || (c == '}')) {
            depth--;
        }
    }

    return depth;
}

// Replaces only the keys this writes, so that anything else the user put in the table -
// a timeout, enabled = false, a comment - survives. The stdio keys go because they and a url
// cannot both describe one server.
QStringList MCPCodexConfig::rewriteTable(const QStringList& table, const QString& serverName,
    const QString& url, const QString& token)
{
    static const QStringList managed = {
        "url", "http_headers", "bearer_token_env_var", "command", "args", "env", "env_vars", "cwd", "type"
    };

    const QString name = QRegularExpression::escape(serverName);
    QRegularExpression child(QString("^\\s*\\[\\s*mcp_servers\\s*\\.\\s*\"?%1\"?\\s*\\.").arg(name));
    // A managed key however it is spelt: bare, quoted, or the head of a dotted key such as
    // http_headers.Authorization, which is the same table written another way
    QRegularExpression assignment("^\\s*\"?([A-Za-z0-9_-]+)\"?\\s*[=.]");

    QStringList out;

    if (!table.isEmpty()) {
        out.append(table.first()); // the header
    }

    out.append(comment());
    out.append(QString("url = %1").arg(tomlString(url)));

    if (!token.trimmed().isEmpty())
    {
        // bearer_token_env_var would keep the token out of the file, but then it would have to
        // be in the environment Codex is started from, which is not something this can arrange
        out.append(QString("http_headers = { \"Authorization\" = %1 }")
            .arg(tomlString("Bearer " + token)));
    }

    bool inChildTable = false;
    int dropping = 0; // brackets still open on a managed value spanning several lines

    for (int i = 1; i < table.size(); i++)
    {
        const QString& line = table[i];

        // A sub table can only be stdio environment, which a url based entry has no use for
        if (child.match(line).hasMatch())
        {
            inChildTable = true;
            continue;
        }

        if (inChildTable) {
            continue;
        }

        if (dropping > 0)
        {
            dropping += bracketDelta(line);
            continue;
        }

        if (line.trimmed() == comment()) {
            continue;
        }

        QRegularExpressionMatch m = assignment.match(line);

        if (m.hasMatch() && managed.contains(m.captured(1)))
        {
            // args = [ ... ] can run over several lines, and all of them have to go
            dropping = qMax(0, bracketDelta(line));
            continue;
        }

        out.append(line);
    }

    while (!out.isEmpty() && out.last().trimmed().isEmpty()) {
        out.removeLast();
    }

    return out;
}

MCPCodexConfig::Result MCPCodexConfig::addServer(const QString& serverName, const QString& url,
    const QString& token, QString& message)
{
    const QString path = configPath();
    QFileInfo info(path);

    if (!QDir().mkpath(info.absolutePath()))
    {
        message = QString("Could not create %1").arg(info.absolutePath());
        return Failed;
    }

    QString existing;
    QString newline = "\n";

    if (info.exists())
    {
        QFile file(path);

        // Not QIODevice::Text: that would translate CRLF away before it can be detected
        if (!file.open(QIODevice::ReadOnly))
        {
            message = QString("Could not read %1: %2").arg(path).arg(file.errorString());
            return Failed;
        }

        existing = QString::fromUtf8(file.readAll());
        file.close();

        if (existing.contains("\r\n")) {
            newline = "\r\n";
        }
    }

    const QString table = tableFor(serverName, url, token, newline);
    QStringList lines = existing.split(QRegularExpression("\r\n|\n"));
    bool trailingNewline = !lines.isEmpty() && lines.last().isEmpty();

    if (trailingNewline) {
        lines.removeLast();
    }

    int start, end;
    findTable(lines, serverName, start, end);
    Result result;

    // The same server written as an inline table under [mcp_servers], or as dotted keys at the
    // top level, is a table this cannot rewrite in place; adding a [mcp_servers.name] beside it
    // would define the table twice and Codex would refuse the whole file
    if (start < 0)
    {
        const QString name = QRegularExpression::escape(serverName);
        QRegularExpression dotted(QString("^\\s*mcp_servers\\s*\\.\\s*\"?%1\"?\\s*[.=]").arg(name));
        QRegularExpression inlineEntry(QString("^\\s*\"?%1\"?\\s*=").arg(name));
        QRegularExpression serversHeader("^\\s*\\[\\s*mcp_servers\\s*\\]");
        QRegularExpression anyTable("^\\s*\\[");
        bool inServers = false;

        for (const QString& line : lines)
        {
            if (anyTable.match(line).hasMatch()) {
                inServers = serversHeader.match(line).hasMatch();
            }

            if (dotted.match(line).hasMatch() || (inServers && inlineEntry.match(line).hasMatch()))
            {
                message = QString("%1 already defines %2 in a form this cannot update (%3). Remove that entry and try again.")
                    .arg(path).arg(serverName).arg(line.trimmed());
                return Failed;
            }
        }
    }

    if (start >= 0)
    {
        const QStringList before = lines.mid(start, end - start);
        const QStringList after = rewriteTable(before, serverName, url, token);

        if (before == after)
        {
            message = QString("%1 already has this server").arg(path);
            return Unchanged;
        }

        for (int i = end - 1; i >= start; i--) {
            lines.removeAt(i);
        }

        for (int i = 0; i < after.size(); i++) {
            lines.insert(start + i, after[i]);
        }

        result = Updated;
    }
    else
    {
        if (!lines.isEmpty() && !lines.last().trimmed().isEmpty()) {
            lines.append(QString());
        }

        QStringList addition = table.split(newline);

        if (!addition.isEmpty() && addition.last().isEmpty()) {
            addition.removeLast();
        }

        lines.append(addition);
        result = Added;
    }

    QString updated = lines.join(newline);

    if (trailingNewline || !info.exists() || !updated.endsWith(newline)) {
        updated += newline;
    }

    // The file is usually hand written, and this is the only thing that writes it besides its
    // owner, so keep the last copy of what was there
    if (info.exists())
    {
        const QString backup = path + ".sdrangel.bak";
        QFile::remove(backup);
        QFile::copy(path, backup);
    }

    QSaveFile out(path); // commits by rename, so a failure leaves the original alone

    if (!out.open(QIODevice::WriteOnly))
    {
        message = QString("Could not write %1: %2").arg(path).arg(out.errorString());
        return Failed;
    }

    out.write(updated.toUtf8());

    if (!out.commit())
    {
        message = QString("Could not write %1: %2").arg(path).arg(out.errorString());
        return Failed;
    }

    message = QString("%1 %2").arg(result == Added ? "Added to" : "Updated in").arg(path);
    return result;
}
