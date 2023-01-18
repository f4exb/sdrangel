///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#include <QMutexLocker>

#include "ft8demodsettings.h"
#include "ft8buffer.h"

FT8Buffer::FT8Buffer() :
    m_bufferSize(FT8DemodSettings::m_ft8SampleRate*15),
    m_sampleIndex(0)
{
    m_buffer = new int16_t[2*m_bufferSize];
}

FT8Buffer::~FT8Buffer()
{
    delete[] m_buffer;
}

void FT8Buffer::feed(int16_t sample)
{
    QMutexLocker mlock(&m_mutex);
    m_buffer[m_sampleIndex] = sample;
    m_buffer[m_sampleIndex + m_bufferSize] = sample;
    m_sampleIndex++;

    if (m_sampleIndex == m_bufferSize) {
        m_sampleIndex = 0;
    }
}

void FT8Buffer::getCurrentBuffer(int16_t *bufferCopy)
{
    QMutexLocker mlock(&m_mutex);
    std::copy(&m_buffer[m_sampleIndex], &m_buffer[m_sampleIndex + m_bufferSize], bufferCopy);
}
