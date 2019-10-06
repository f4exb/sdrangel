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

#ifndef SDRBASE_INTERFEROMETERSTREAMSINK_H_
#define SDRBASE_INTERFEROMETERSTREAMSINK_H_

#include <QMutex>

#include "dsp/basebandsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"


class InterferometerStreamSink : public BasebandSampleSink
{
public:
    InterferometerStreamSink();
    virtual ~InterferometerStreamSink();

	virtual void start();
	virtual void stop();
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual bool handleMessage(const Message& cmd); //!< Processing of a message. Returns true if message has actually been processed

    void reset();
    unsigned int getStreamIndex() const { return m_streamIndex; }
    void setStreamIndex(unsigned int streamIndex) { m_streamIndex = streamIndex; }
    SampleVector& getData() { return m_data; }
    int getSize() const { return m_dataSize; }
    void setDataStart(int dataStart) { m_dataStart = dataStart; }

private:
    unsigned int m_streamIndex;
    SampleVector m_data;
    int m_dataSize;
    int m_bufferSize;
    int m_dataStart;

    int m_sampleRate;
    uint32_t m_log2Decim;
    uint32_t m_filterChainHash;
    QMutex m_settingsMutex;
};


#endif // SDRBASE_INTERFEROMETERSTREAMSINK_H_