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

#ifndef INCLUDE_MCPBRIDGE_JSONSCAN_H_
#define INCLUDE_MCPBRIDGE_JSONSCAN_H_

#include <string>

//!< The bridge passes JSON through without understanding it, and needs only two things out
//!< of a message: the id, so a transport failure can be reported against the right request,
//!< and the negotiated protocol version. Both are members of an object, so rather than carry
//!< a JSON library this returns the raw text of one member of the outermost object. A nested
//!< member of the same name is not mistaken for it.

//!< Raw value text, quotes and all for a string, empty when the key is not a member of the
//!< outermost object or the text is not an object
std::string jsonMember(const std::string& json, const std::string& key);

//!< Same, with the surrounding quotes and escapes of a JSON string removed
std::string jsonStringMember(const std::string& json, const std::string& key);

#endif // INCLUDE_MCPBRIDGE_JSONSCAN_H_
