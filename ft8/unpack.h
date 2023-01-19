///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
// reformatted and adapted to Qt and SDRangel context                            //
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
#ifndef unpack_h
#define unpack_h

#include <string>
#include <map>

#include <QRecursiveMutex>

#include "export.h"

namespace FT8 {

class FT8_API Packing
{
public:
    std::string unpack(int a91[], std::string& call1str, std::string& call2str, std::string& locstr);

private:
    static int ihashcall(std::string call, int m);
    std::string unpackcall(int x);
    std::string unpackgrid(int ng, int ir, int i3);
    void remember_call(std::string call);
    std::string unpack_4(int a77[], std::string& call1str, std::string& call2str, std::string& locstr);
    std::string unpack_1(int a77[], std::string& call1str, std::string& call2str, std::string& locstr);
    std::string unpack_0_0(int a77[], std::string& call1str, std::string& call2str, std::string& locstr);
    std::string unpack_3(int a77[], std::string& call1str, std::string& call2str, std::string& locstr);
    std::string unpack_0_3(int a77[], int n3, std::string& call1str, std::string& call2str, std::string& locstr);

    QRecursiveMutex hashes_mu;
    std::map<int, std::string> hashes12;
    std::map<int, std::string> hashes22;

    static const int NGBASE = (180 * 180);
    static const int NTOKENS = 2063592;
    static const int MAX22 = 4194304;
    static const char *ru_states[];
    static const char *sections[];
};


} // namespace FT8

#endif
