///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_UTIL_PSK31_H
#define INCLUDE_UTIL_PSK31_H

#include <QString>

#include "export.h"

class SDRBASE_API PSK31Varicode {

public:

    static const QStringList m_varicode; // Index with 8-bit extended-ASCII

};

class SDRBASE_API PSK31Encoder {

public:

    PSK31Encoder();
    bool encode(QChar c, unsigned& bits, unsigned int &bitCount);

private:

    void addCode(unsigned& bits, unsigned int& bitCount, const QString& code) const;
    void addStartBits(unsigned int& bits, unsigned int& bitCount) const;
    void addStopBits(unsigned int& bits, unsigned int& bitCount) const;
    void addBits(unsigned int& bits, unsigned int& bitCount, int data, int count) const;

};

#endif // INCLUDE_UTIL_PSK31_H

