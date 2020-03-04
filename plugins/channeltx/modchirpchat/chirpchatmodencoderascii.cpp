///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "chirpchatmodencoderascii.h"

void ChirpChatModEncoderASCII::encodeString(const QString& str, std::vector<unsigned short>& symbols)
{
    QByteArray asciiStr = str.toUtf8();
    QByteArray::const_iterator it = asciiStr.begin();

    for (; it != asciiStr.end(); ++it) {
        symbols.push_back(*it & 0x7F);
    }
}
