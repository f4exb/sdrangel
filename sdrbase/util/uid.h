///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Object unique id calculator loosely inspired by MongoDB object id             //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////
#ifndef SDRBASE_UTIL_UID_H_
#define SDRBASE_UTIL_UID_H_

#include <stdint.h>

class UidCalculator
{
public:
    /**
     * Get a new object unique Id. It is the addition of:
     *   - 6 digit microseconds in current second
     *   - 4 byte Unix epoch multiplied by 1000000
     */
    static uint64_t getNewObjectId();

    /**
     * Get a new instance unique Id. It is made of from LSB to MSB:
     *   - 2 byte process id
     *   - 2 byte hashed host name
     */
    static uint32_t getNewInstanceId();

private:
    static uint64_t getCurrentMiroseconds();
};

#endif /* SDRBASE_UTIL_UID_H_ */
