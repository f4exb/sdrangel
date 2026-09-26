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

#ifndef INCLUDE_FEATURE_MCPERROR_H_
#define INCLUDE_FEATURE_MCPERROR_H_

#include <QString>

// Protocol level failure: becomes a JSON-RPC error response
struct MCPError
{
    int code;
    QString message;

    MCPError(int aCode, const QString& aMessage) : code(aCode), message(aMessage) {}
};

// Tool level failure: becomes a tools/call result with isError set, so the model can
// read the message and try again
struct MCPToolError
{
    QString message;

    explicit MCPToolError(const QString& aMessage) : message(aMessage) {}
};

#endif // INCLUDE_FEATURE_MCPERROR_H_
