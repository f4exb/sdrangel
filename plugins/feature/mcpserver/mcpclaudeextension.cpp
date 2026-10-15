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

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef _WIN32
#include <QSettings>
#endif

#include "mcpclaudeextension.h"

QString MCPClaudeExtension::bundlePath()
{
    // The bundle is installed beside the executable on every platform, and lands beside it in
    // a build tree too. Its name carries the version, so it is matched rather than named
    QDir dir(QCoreApplication::applicationDirPath());
    QFileInfoList bundles = dir.entryInfoList({"sdrangel-*.mcpb"}, QDir::Files, QDir::Name);

    if (bundles.isEmpty()) {
        return QString();
    }

    // Several can be left in a build tree, as each build makes a version of its own. Sorting
    // by name is not enough to order versions, so take the one most recently written
    QFileInfo newest = bundles.first();

    for (const QFileInfo& bundle : bundles)
    {
        if (bundle.lastModified() > newest.lastModified()) {
            newest = bundle;
        }
    }

    return newest.absoluteFilePath();
}

QString MCPClaudeExtension::claudePath()
{
#if defined(_WIN32)
    // Claude registers no .mcpb file association, and the packaged (MSIX) build has no
    // execution alias, so the only handle on it is the claude:// protocol handler, which both
    // it and the .exe build write to HKCU with a real path to the executable
    QSettings handler("HKEY_CURRENT_USER\\Software\\Classes\\claude\\shell\\open\\command",
        QSettings::NativeFormat);
    const QString command = handler.value("Default").toString();
    const int open = command.indexOf('"');
    const int close = (open < 0) ? -1 : command.indexOf('"', open + 1);

    if (close < 0) {
        return QString(); // "<path>" "%1" is the only shape understood, leave anything else
    }

    const QString path = command.mid(open + 1, close - open - 1);

    // The path carries the version of the Claude that wrote it, so an update that has not
    // been launched since can leave it pointing at a directory that is gone
    return QFile::exists(path) ? path : QString();
#elif defined(__APPLE__)
    const QString app = "/Applications/Claude.app";
    return QFileInfo(app).isDir() ? app : QString();
#else
    // There is no official Claude Desktop for Linux. Community builds install claude-desktop;
    // plain "claude" is Claude Code, which is not what a bundle should be handed to
    return QStandardPaths::findExecutable("claude-desktop");
#endif
}

MCPClaudeExtension::Result MCPClaudeExtension::install(QString& message)
{
    const QString bundle = bundlePath();

    if (bundle.isEmpty())
    {
        message = QString("No extension bundle was found in %1.\n\n"
            "This build of SDRangel was made without one.")
            .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));
        return NoBundle;
    }

    const QString claude = claudePath();

    if (claude.isEmpty())
    {
        message = QString("Claude Desktop could not be found.\n\n"
            "The extension is at %1. Open it with Claude Desktop, or drag it onto "
            "Settings > Extensions.").arg(QDir::toNativeSeparators(bundle));
        return NoClaude;
    }

#ifdef __APPLE__
    // claudePath gives the application bundle, which is opened rather than executed
    const bool started = QProcess::startDetached("open", {"-a", claude, bundle});
#else
    const bool started = QProcess::startDetached(claude, {bundle});
#endif

    if (!started)
    {
        message = QString("Could not start %1.").arg(QDir::toNativeSeparators(claude));
        return Failed;
    }

    // Claude shows its own install dialog, with its own warning about what an extension can
    // do, so there is nothing further to confirm here
    message = QString("Claude Desktop has been asked to install %1.\n\n"
        "Confirm the installation in Claude Desktop.").arg(QFileInfo(bundle).fileName());
    return Launched;
}
