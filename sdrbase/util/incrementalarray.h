///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_UTIL_INCREMENTALARRAY_H_
#define SDRBASE_UTIL_INCREMENTALARRAY_H_

#include <stdint.h>

template<typename T>
class IncrementalArray
{
public:
    T *m_array;

    IncrementalArray();
    ~IncrementalArray();

    void allocate(uint32_t size);

private:
    uint32_t m_size;
};

template<typename T>
IncrementalArray<T>::IncrementalArray() :
    m_array(0),
    m_size(0)
{
}

template<typename T>
IncrementalArray<T>::~IncrementalArray()
{
    if (m_array) { delete[] m_array; }
}

template<typename T>
void IncrementalArray<T>::allocate(uint32_t size)
{
    if (size <= m_size) { return; }
    if (m_array) { delete[] m_array; }
    m_array = new T[size];
    m_size = size;
}

#endif /* SDRBASE_UTIL_INCREMENTALARRAY_H_ */
