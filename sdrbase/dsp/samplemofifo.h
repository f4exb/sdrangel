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

#ifndef INCLUDE_SAMPLEMOFIFO_H
#define INCLUDE_SAMPLEMOFIFO_H

#include <QObject>
#include <QMutex>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleMOFifo : public QObject {
	Q_OBJECT

public:
    SampleMOFifo(QObject *parent = nullptr);
    SampleMOFifo(unsigned int nbStreams, unsigned int size, QObject *parent = nullptr);
    ~SampleMOFifo();

    void init(unsigned int nbStreams, unsigned int size);
    void resize(unsigned int size);
    void reset();

    void readSync(
        unsigned int amount,
		unsigned int& ipart1Begin, unsigned int& ipart1End, // first part offsets where to read
		unsigned int& ipart2Begin, unsigned int& ipart2End  // second part offsets
    );

    void writeSync( //!< in place write with given amount
        unsigned int amount,
		unsigned int& ipart1Begin, unsigned int& ipart1End, // first part offsets where to write
		unsigned int& ipart2Begin, unsigned int& ipart2End  // second part offsets
    );

    void readAsync(
        unsigned int amount,
		unsigned int& ipart1Begin, unsigned int& ipart1End,
		unsigned int& ipart2Begin, unsigned int& ipart2End,
        unsigned int stream
    );

    void writeAsync( //!< in place write with given amount
        unsigned int amount,
		unsigned int& ipart1Begin, unsigned int& ipart1End,
		unsigned int& ipart2Begin, unsigned int& ipart2End,
        unsigned int stream
    );

    std::vector<SampleVector>& getData() { return m_data; }
    SampleVector& getData(unsigned int stream) { return m_data[stream]; }
    unsigned int getNbStreams() const { return m_data.size(); }

    unsigned int remainderSync()
    {
        QMutexLocker mutexLocker(&m_mutex);
        return m_readCount;
    }

    unsigned int remainderAsync(unsigned int stream)
    {
        if (stream >= m_nbStreams) {
            return 0;
        }

        QMutexLocker mutexLocker(&m_mutex);
        return m_vReadCount[stream];
    }

    static unsigned int getSizePolicy(unsigned int sampleRate);
    static const unsigned int m_rwDivisor;
    static const unsigned int m_guardDivisor;

signals:
	void dataReadSync();
    void dataReadAsync(int streamIndex);

private:
    std::vector<SampleVector> m_data;
    unsigned int m_nbStreams;
    unsigned int m_size;
    unsigned int m_lowGuard;
    unsigned int m_highGuard;
    unsigned int m_midPoint;
    unsigned int m_readCount;
    unsigned int m_readHead;
    unsigned int m_writeHead;
    std::vector<unsigned int> m_vReadCount;
    std::vector<unsigned int> m_vReadHead;
    std::vector<unsigned int> m_vWriteHead;
	QMutex m_mutex;
};

#endif // INCLUDE_SAMPLEMOFIFO_H