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

#ifndef PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODERTTY_H_
#define PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODERTTY_H_

#include <vector>
#include <QString>

class ChirpChatModEncoderTTY
{
public:
    static void encodeString(const QString& str, std::vector<unsigned short>& symbols);

private:
    enum TTYState
    {
        TTYLetters,
        TTYFigures
    };

    static const signed char asciiToTTYLetters[128];
    static const signed char asciiToTTYFigures[128];
    static const char ttyLetters = 0x1f;
    static const char ttyFigures = 0x1b;
};

#endif // PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODERTTY_H_