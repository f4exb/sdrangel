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

#include <QMutexLocker>
#include <QDebug>

#include "interferometerstreamsink.h"

InterferometerStreamSink::InterferometerStreamSink() :
    m_streamIndex(0),
    m_dataSize(0),
    m_bufferSize(0),
    m_dataStart(0)
{}

InterferometerStreamSink::~InterferometerStreamSink()
{}

void InterferometerStreamSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_dataSize = (end - begin) + m_dataStart;

    if (m_dataSize > m_bufferSize)
    {
        m_data.resize(m_dataSize);
        m_bufferSize = m_dataSize;
    }

    std::copy(begin, end, m_data.begin() + m_dataStart);
}

void InterferometerStreamSink::reset()
{
    m_dataStart = 0;
}
