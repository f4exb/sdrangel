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
    void init(unsigned int nbStreams, unsigned int size);
    void writeSync(const std::vector<SampleVector::const_iterator>& vbegin, unsigned int size);
    void writeAsync(unsigned int stream, const SampleVector::const_iterator& begin, unsigned int size);
    void readSync(
        std::vector<SampleVector::const_iterator>& vpart1Begin, std::vector<SampleVector::const_iterator>& vpart1End,
        std::vector<SampleVector::const_iterator>& vpart2Begin, std::vector<SampleVector::const_iterator>& vpart2End
    );
    void readSync(
        std::vector<int>& vPart1Begin, std::vector<int>& vPart1End,
        std::vector<int>& vPart2Begin, std::vector<int>& vPart2End
    );
	void readAsync(unsigned int stream,
		SampleVector::const_iterator& part1Begin, SampleVector::const_iterator& part1End,
		SampleVector::const_iterator& part2Begin, SampleVector::const_iterator& part2End);
    const std::vector<SampleVector>& getData() { return m_data; }
    unsigned int getNbStreams() const { return m_data.size(); }
    bool dataAvailable();
    bool dataAvailable(unsigned int stream);


signals:
	void dataSyncReady();
    void dataAsyncReady(int streamIndex);

private:
    std::vector<SampleVector> m_data;
    std::vector<int> m_vFill; //!< Number of samples written from beginning of samples vector
    std::vector<int> m_vHead; //!< Number of samples read from beginning of samples vector
	QMutex m_mutex;
};

#endif // INCLUDE_SAMPLEMIFIFO_H
