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

#ifndef SDRBASE_DSP_SAMPLESOURCEFIFO_H_
#define SDRBASE_DSP_SAMPLESOURCEFIFO_H_

#include <QObject>
#include <QMutex>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleSourceFifo : public QObject {
	Q_OBJECT
public:
    SampleSourceFifo(QObject *parent = nullptr);
    SampleSourceFifo(unsigned int size, QObject *parent = nullptr);
    ~SampleSourceFifo();
    void resize(unsigned int size);
    void reset();

    SampleVector& getData() { return m_data; }
    void read(
        unsigned int amount,
		unsigned int& ipart1Begin, unsigned int& ipart1End, // first part offsets where to read
		unsigned int& ipart2Begin, unsigned int& ipart2End  // second part offsets
    );
    void write( //!< in place write
        unsigned int amount,
		unsigned int& ipart1Begin, unsigned int& ipart1End, // first part offsets where to write
		unsigned int& ipart2Begin, unsigned int& ipart2End  // second part offsets
    );
    unsigned int remainder()
    {
        QMutexLocker mutexLocker(&m_mutex);
        return m_readCount;
    }
    /** returns ratio of off center over buffer size with sign: negative read lags and positive read leads */
    float getRWBalance() const
    {
        int delta;
        if (m_writeHead > m_readHead) {
            delta = (m_size/m_rwDivisor) - (m_writeHead - m_readHead);
        } else {
            delta = (m_readHead - m_writeHead) - (m_size/m_rwDivisor);
        }
        return delta / (float) m_size;
    }
    unsigned int size() const { return m_size; }

    static unsigned int getSizePolicy(unsigned int sampleRate);
    static const unsigned int m_rwDivisor;
    static const unsigned int m_guardDivisor;

signals:
    void dataRead();

private:
    SampleVector m_data;
    unsigned int m_size;
    unsigned int m_lowGuard;
    unsigned int m_highGuard;
    unsigned int m_midPoint;
    unsigned int m_readHead;
    unsigned int m_writeHead;
    unsigned int m_readCount;
    QMutex m_mutex;
};

#endif // SDRBASE_DSP_SAMPLESOURCEFIFO_H_