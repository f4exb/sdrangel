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

#ifndef SDRBASE_DSP_SAMPLESINKFIFODOUBLEBUFFERED_H_
#define SDRBASE_DSP_SAMPLESINKFIFODOUBLEBUFFERED_H_

#include <QObject>
#include <QMutex>
#include <stdint.h>
#include <assert.h>
#include "export.h"
#include "dsp/dsptypes.h"

class SDRBASE_API SampleSinkFifoDoubleBuffered : public QObject {
    Q_OBJECT

public:
    SampleSinkFifoDoubleBuffered(uint32_t size, uint32_t signalThreshold);
    ~SampleSinkFifoDoubleBuffered();

    void getWriteIterator(SampleVector::iterator& it1);
    void bumpIndex(SampleVector::iterator& it1);
    void read(SampleVector::iterator& begin, SampleVector::iterator& end);

private:
    uint32_t m_size;
    uint32_t m_signalThreshold;
    SampleVector m_data;
    uint32_t m_i;
    uint32_t m_count;
    uint32_t m_readIndex;
    uint32_t m_readCount;
    QMutex m_mutex;

signals:
    void dataReady();
};

#endif /* SDRBASE_DSP_SAMPLESINKFIFODOUBLEBUFFERED_H_ */
