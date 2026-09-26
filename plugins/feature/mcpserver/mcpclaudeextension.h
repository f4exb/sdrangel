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

#ifndef INCLUDE_FEATURE_MCPCLAUDEEXTENSION_H_
#define INCLUDE_FEATURE_MCPCLAUDEEXTENSION_H_

#include <QString>

// Hands the Claude Desktop extension bundle to Claude Desktop, so that it can be installed
// from the feature's window rather than by finding the file and opening it by hand.
//
// Claude Desktop cannot reach a server on 127.0.0.1, because its custom connectors are
// fetched from Anthropic's cloud. The .mcpb bundle beside SDRangel carries a small bridge
// that relays the stdio transport Claude Desktop does support to this server, and installing
// the bundle is what puts that bridge in place.
//
// There is no documented directory a bundle can be dropped into: the supported way to install
// one is to open it with Claude Desktop, which then shows its own dialog with its own warning
// about what an extension may do. So that is what this does, which is also what the Windows
// installer offers at the end of a fresh install.
class MCPClaudeExtension
{
public:
    enum Result
    {
        Launched,   //!< Claude Desktop has been asked to open the bundle
        NoBundle,   //!< This build was made without the extension bundle
        NoClaude,   //!< The bundle is there but Claude Desktop could not be found
        Failed      //!< Claude Desktop is there but would not start
    };

    //!< The newest sdrangel-*.mcpb beside the application, or empty when there is none
    static QString bundlePath();

    //!< The Claude Desktop executable, or empty when it does not appear to be installed
    static QString claudePath();

    //!< Opens the bundle with Claude Desktop. message describes what happened either way
    static Result install(QString& message);
};

#endif // INCLUDE_FEATURE_MCPCLAUDEEXTENSION_H_
