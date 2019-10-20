///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_UTIL_INCREMENTALVECTOR_H_
#define SDRBASE_UTIL_INCREMENTALVECTOR_H_

#include <stdint.h>
#include <vector>

template<typename T>
class IncrementalVector
{
public:
    std::vector<T> m_vector;

    IncrementalVector();
    ~IncrementalVector();

    void allocate(uint32_t size);
    void allocate(uint32_t size, const T& init);

private:
    uint32_t m_size;
};

template<typename T>
IncrementalVector<T>::IncrementalVector() :
    m_size(0)
{
}

template<typename T>
IncrementalVector<T>::~IncrementalVector()
{
}

template<typename T>
void IncrementalVector<T>::allocate(uint32_t size)
{
    if (size <= m_size) { return; }
    m_vector.resize(size);
    m_size = size;
}

template<typename T>
void IncrementalVector<T>::allocate(uint32_t size, const T& init)
{
    if (size <= m_size) { return; }
    m_vector.resize(size);
    std::fill(m_vector.begin(), m_vector.end(), init);
    m_size = size;
}

#endif /* SDRBASE_UTIL_INCREMENTALVECTOR_H_ */
