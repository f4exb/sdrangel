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

#ifndef INCLUDE_INTERFEROMETERSINK_H
#define INCLUDE_INTERFEROMETERSINK_H

#include "dsp/basebandsamplesink.h"

class InterferometerCorrelator;

class InterferometerSink : public BasebandSampleSink
{
public:
    InterferometerSink(InterferometerCorrelator *correlator);
    virtual ~InterferometerSink();

	virtual void start();
	virtual void stop();
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual bool handleMessage(const Message& cmd);

    void setProcessingUnit(bool) { m_processingUnit = m_processingUnit; }
    bool isProcessingUnit() const { return m_processingUnit; }

private:
    bool m_processingUnit; //!< True if it is responsible of starting the correlation process
    InterferometerCorrelator *m_correlator;
};

#endif // INCLUDE_INTERFEROMETERSINK_H
