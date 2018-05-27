///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DSP_SAMPLESOURCEFIFO_H_
#define SDRBASE_DSP_SAMPLESOURCEFIFO_H_

#include <QObject>
#include <QMutex>
#include <stdint.h>
#include <assert.h>
#include "export.h"
#include "dsp/dsptypes.h"

class SDRBASE_API SampleSourceFifo : public QObject {
    Q_OBJECT

public:
    SampleSourceFifo(uint32_t size);
    ~SampleSourceFifo();

    void resize(uint32_t size);
    uint32_t size() const { return m_size; }
    void init();
    /** advance read pointer for the given length and activate R/W signals */
    void readAdvance(SampleVector::iterator& readUntil, unsigned int nbSamples);

    void getReadIterator(SampleVector::iterator& readUntil); //!< get iterator past the last sample of a read advance operation (i.e. current read iterator)
    void getWriteIterator(SampleVector::iterator& writeAt);  //!< get iterator to current item for update - write phase 1
    void bumpIndex(SampleVector::iterator& writeAt);         //!< copy current item to second buffer and bump write index - write phase 2

    void write(const Sample& sample);                        //!< write directly - phase 1 + phase 2

    /** returns ratio of off center over buffer size with sign: negative real lags and positive read leads */
    float getRWBalance() const
    {
        int delta;
        if (m_iw > m_ir) {
            delta = (m_size/2) - (m_iw - m_ir);
        } else {
            delta = (m_ir - m_iw) - (m_size/2);
        }
        return delta / (float) m_size;
    }

private:
    uint32_t m_size;
    SampleVector m_data;
    uint32_t m_iw;
    uint32_t m_ir;
    bool m_init;
    QMutex m_mutex;

signals:
    void dataWrite(int nbSamples); // signal data is read past a threshold and writing new samples to fill in is needed
    void dataRead(int nbSamples);  // signal a read has been done for a number of samples
};

#endif /* SDRBASE_DSP_SAMPLESOURCEFIFO_H_ */
