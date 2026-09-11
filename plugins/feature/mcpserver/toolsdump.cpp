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
// The registry is metadata and handler functions: building it touches neither the Web API nor
// any device, so a null adapter is all it needs.

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "mcptools.h"

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <output file>\n", argv[0]);
        return 2;
    }

    std::fprintf(stderr, "toolsdump: application up\n");
    MCPTools tools(nullptr);
    std::fprintf(stderr, "toolsdump: registry built\n");

    QJsonObject result;
    result["tools"] = tools.listTools();
    std::fprintf(stderr, "toolsdump: listed\n");

    QFile file(QString::fromLocal8Bit(argv[1]));

    if (!file.open(QIODevice::WriteOnly))
    {
        std::fprintf(stderr, "cannot write %s: %s\n", argv[1], qPrintable(file.errorString()));
        return 1;
    }

    file.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
    file.close();
    std::fprintf(stderr, "toolsdump: written\n");

    std::fprintf(stderr, "%s: %d tools\n", argv[1], result["tools"].toArray().size());

    // The registry brings background threads up with it and they do not all stop when main
    // returns, which leaves the process, and so the build, hanging. Even _Exit does not get
    // out, since the runtime's own teardown waits on them, so end the process from the kernel
    // side. The file is written and flushed by this point
    std::fflush(nullptr);

#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 0);
#endif
    std::_Exit(0);
}
