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

#include <algorithm>
#include <assert.h>
#include "samplesourcefifodb.h"

SampleSourceFifoDB::SampleSourceFifoDB(uint32_t size, QObject* parent) :
    QObject(parent),
    m_size(size),
    m_init(false)
{
    m_data.resize(2*m_size);
    init();
}

SampleSourceFifoDB::SampleSourceFifoDB(const SampleSourceFifoDB& other) :
    QObject(other.parent()),
    m_size(other.m_size),
    m_data(other.m_data)
{
    init();
}

SampleSourceFifoDB::~SampleSourceFifoDB()
{}

void SampleSourceFifoDB::resize(uint32_t size)
{
    qDebug("SampleSourceFifoDB::resize: %d", size);

    m_size = size;
    m_data.resize(2*m_size);
    init();
}

void SampleSourceFifoDB::init()
{
    static Sample zero = {0,0};
    std::fill(m_data.begin(), m_data.end(), zero);
    m_ir = 0;
    m_iw = m_size/2;
    m_init = true;
}

void SampleSourceFifoDB::readAdvance(SampleVector::iterator& readUntil, unsigned int nbSamples)
{
//    QMutexLocker mutexLocker(&m_mutex);
    assert(nbSamples <= m_size/2);
    emit dataWrite(nbSamples);

    m_ir = (m_ir + nbSamples) % m_size;
    readUntil =  m_data.begin() + m_size + m_ir;
    emit dataRead(nbSamples);
}

void SampleSourceFifoDB::readAdvance(SampleVector::const_iterator& readUntil, unsigned int nbSamples)
{
//    QMutexLocker mutexLocker(&m_mutex);
    assert(nbSamples <= m_size/2);
    emit dataWrite(nbSamples);

    m_ir = (m_ir + nbSamples) % m_size;
    readUntil =  m_data.begin() + m_size + m_ir;
    emit dataRead(nbSamples);
}

void SampleSourceFifoDB::write(const Sample& sample)
{
    m_data[m_iw] = sample;
    m_data[m_iw+m_size] = sample;

    {
//        QMutexLocker mutexLocker(&m_mutex);
        m_iw = (m_iw+1) % m_size;
    }
}

void SampleSourceFifoDB::getReadIterator(SampleVector::iterator& readUntil)
{
    readUntil = m_data.begin() + m_size + m_ir;
}

void SampleSourceFifoDB::getWriteIterator(SampleVector::iterator& writeAt)
{
    writeAt = m_data.begin() + m_iw;
}

void SampleSourceFifoDB::bumpIndex(SampleVector::iterator& writeAt)
{
    m_data[m_iw+m_size] = m_data[m_iw];

    {
//        QMutexLocker mutexLocker(&m_mutex);
        m_iw = (m_iw+1) % m_size;
    }

    writeAt = m_data.begin() + m_iw;
}

int SampleSourceFifoDB::getIteratorOffset(const SampleVector::iterator& iterator)
{
    return iterator - m_data.begin();
}

void SampleSourceFifoDB::setIteratorFromOffset(SampleVector::iterator& iterator, int offset)
{
    iterator = m_data.begin() + offset;
}
