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

#ifndef SDRBASE_DSP_SAMPLESINKVECTOR_H_
#define SDRBASE_DSP_SAMPLESINKVECTOR_H_

#include <QObject>

#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleSinkVector : public QObject {
	Q_OBJECT

public:
    SampleSinkVector(QObject* parent = nullptr);
    SampleSinkVector(const SampleSinkVector& other);
    ~SampleSinkVector();

    void write(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool signal = true);
    void append(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool signal = false);
    void read(SampleVector::iterator& begin, SampleVector::iterator& end);

signals:
	void dataReady();

private:
    SampleVector m_sampleVector;
    int m_dataSize;
    // SampleVector::const_iterator m_begin;
    // SampleVector::const_iterator m_end;
};


#endif // SDRBASE_DSP_SAMPLESINKVECTOR_H_
