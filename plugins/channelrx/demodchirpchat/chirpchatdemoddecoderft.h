///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_CHIRPCHATDEMODDECODERFT_H
#define INCLUDE_CHIRPCHATDEMODDECODERFT_H

#include <vector>
#include <string>

class ChirpChatDemodDecoderFT
{
public:
    enum ParityStatus
    {
        ParityUndefined,
        ParityError,
        ParityCorrected,
        ParityOK
    };

    static void decodeSymbols(
        const std::vector<std::vector<float>>& mags, // vector of symbols magnitudes
        int nbSymbolBits, //!< number of bits per symbol
        std::string& msg,     //!< formatted message
        std::string& call1,   //!< 1st callsign or shorthand
        std::string& call2,   //!< 2nd callsign
        std::string& loc,     //!< locator, report or shorthand
        bool& reply ,         //!< true if message is a reply report
        int& payloadParityStatus,
        bool& payloadCRCStatus
    );
};


#endif
