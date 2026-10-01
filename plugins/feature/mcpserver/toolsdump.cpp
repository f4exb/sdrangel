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

// Writes the MCP tool list to a file at build time, so that the Claude Desktop extension can
// carry the real list rather than inventing one. A client that starts before SDRangel reads
// the tool list once and does not ask again, so the bridge has to be able to answer with the
// tools this very build of the server provides, before it has ever spoken to it.
//
// A null adapter prevents Web API calls, but MCPTools members still initialize MainCore,
// including its pipe workers and platform location/permission services. Log that separately
// from registry construction, since it can block on a headless CI runner.

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "maincore.h"
#include "mcptools.h"

int main(int argc, char *argv[])
{
    // Ninja and CI capture stderr through pipes; publish each checkpoint immediately.
    std::setvbuf(stderr, nullptr, _IONBF, 0);
#ifdef _WIN32
    std::fprintf(stderr, "toolsdump: entered main (pid %lu)\n", GetCurrentProcessId());
#else
    std::fprintf(stderr, "toolsdump: entered main\n");
#endif
    std::fprintf(stderr, "toolsdump: creating QCoreApplication\n");
    QCoreApplication application(argc, argv);

    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <output file>\n", argv[0]);
        return 2;
    }

    std::fprintf(stderr, "toolsdump: application up\n");

    // MainCore starts position updates as it is constructed, and the Windows position plugin
    // asks the system for location access and spins until it has an answer. On a machine where
    // nobody has ever answered, such as a CI runner, that is for ever. Nothing here needs a
    // plugin, so leave Qt nowhere to find one and MainCore carries on without a position source
    QCoreApplication::setLibraryPaths(QStringList());

    std::fprintf(stderr, "toolsdump: initializing MainCore (pipe workers, permissions, positioning)\n");
    MainCore::instance();
    std::fprintf(stderr, "toolsdump: MainCore ready; constructing registry\n");
    MCPTools tools(nullptr);
    std::fprintf(stderr, "toolsdump: registry built\n");

    QJsonObject result;
    std::fprintf(stderr, "toolsdump: listing tools\n");
    result["tools"] = tools.listTools();
    std::fprintf(stderr, "toolsdump: listed\n");

    QFile file(QString::fromLocal8Bit(argv[1]));

    std::fprintf(stderr, "toolsdump: opening %s\n", argv[1]);

    if (!file.open(QIODevice::WriteOnly))
    {
        std::fprintf(stderr, "cannot write %s: %s\n", argv[1], qPrintable(file.errorString()));
        return 1;
    }

    std::fprintf(stderr, "toolsdump: writing JSON\n");
    file.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
    std::fprintf(stderr, "toolsdump: closing output\n");
    file.close();
    std::fprintf(stderr, "toolsdump: written\n");

    std::fprintf(stderr, "%s: %lld tools\n", argv[1],
        static_cast<long long>(result["tools"].toArray().size()));

    // Preserve the existing teardown workaround: MainCore has background workers, and
    // this one-shot helper never runs the Qt event loop. This does not protect against
    // a hang during initialization; the build target applies a separate process timeout.
    std::fprintf(stderr, "toolsdump: flushing streams before process exit\n");
    std::fflush(nullptr);

#ifdef _WIN32
    std::fprintf(stderr, "toolsdump: calling TerminateProcess\n");
    if (!TerminateProcess(GetCurrentProcess(), 0)) {
        std::fprintf(stderr, "toolsdump: TerminateProcess failed: %lu\n", GetLastError());
    }
#endif
    std::fprintf(stderr, "toolsdump: calling _Exit\n");
    std::_Exit(0);
}
