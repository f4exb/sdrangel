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

#ifndef INCLUDE_SAMPLEMIFIFO_H
#define INCLUDE_SAMPLEMIFIFO_H

#include <QObject>
#include <QMutex>
#include <vector>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleMIFifo : public QObject {
	Q_OBJECT

public:
    SampleMIFifo(QObject *parent = nullptr);
    SampleMIFifo(unsigned int nbStreams, unsigned int size, QObject *parent = nullptr);
    ~SampleMIFifo();
    void init(unsigned int nbStreams, unsigned int size);
    void reset();

    void writeSync(const quint8* data, unsigned int count); //!< de-interleaved data in input with count bytes for each stream
    void writeSync(const std::vector<SampleVector::const_iterator>& vbegin, unsigned int size);
    void readSync(
		std::vector<SampleVector::const_iterator*> vpart1Begin, std::vector<SampleVector::const_iterator*> vpart1End,
		std::vector<SampleVector::const_iterator*> vpart2Begin, std::vector<SampleVector::const_iterator*> vpart2End
    );
    void readSync(
        std::vector<unsigned int>& vPart1Begin, std::vector<unsigned int>& vPart1End,
        std::vector<unsigned int>& vPart2Begin, std::vector<unsigned int>& vPart2End
    );
    void readSync(
		unsigned int& ipart1Begin, unsigned int& ipart1End,
		unsigned int& ipart2Begin, unsigned int& ipart2End
    );

    void writeAsync(const quint8* data, unsigned int count, unsigned int stream);
    void writeAsync(const SampleVector::const_iterator& begin, unsigned int inputSize, unsigned int stream);
    void readAsync(
		SampleVector::const_iterator* part1Begin, SampleVector::const_iterator* part1End,
		SampleVector::const_iterator* part2Begin, SampleVector::const_iterator* part2End,
        unsigned int stream);
    void readAsync(
		unsigned int& ipart1Begin, unsigned int& ipart1End,
		unsigned int& ipart2Begin, unsigned int& ipart2End,
        unsigned int stream);


    const std::vector<SampleVector>& getData() { return m_data; }
    const SampleVector& getData(unsigned int stream) { return m_data[stream]; }
    unsigned int getNbStreams() const { return m_data.size(); }

    inline unsigned int fillSync()
    {
        QMutexLocker mutexLocker(&m_mutex);
        unsigned int fill = m_head <= m_fill ? m_fill - m_head : m_size - (m_head - m_fill);
        return fill;
    }

    inline unsigned int fillAsync(unsigned int stream)
    {
        if (stream >= m_nbStreams) {
            return 0;
        }

        QMutexLocker mutexLocker(&m_mutex);
        unsigned int fill = m_vHead[stream] <= m_vFill[stream] ? m_vFill[stream] - m_vHead[stream] : m_size - (m_vHead[stream] - m_vFill[stream]);
        return fill;
    }

signals:
	void dataSyncReady();
    void dataAsyncReady(int streamIndex);

private:
    std::vector<SampleVector> m_data;
    unsigned int m_nbStreams;
    unsigned int m_size;
    unsigned int m_fill;               //!< Number of samples written from beginning of samples vector (sync)
    unsigned int m_head;               //!< Number of samples read from beginning of samples vector (sync)
    std::vector<unsigned int> m_vFill; //!< Number of samples written from beginning of samples vector (async)
    std::vector<unsigned int> m_vHead; //!< Number of samples read from beginning of samples vector (async)
	QMutex m_mutex;
};

#endif // INCLUDE_SAMPLEMIFIFO_H