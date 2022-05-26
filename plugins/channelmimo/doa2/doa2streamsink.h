///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DOA2STREAMSINK_H_
#define SDRBASE_DOA2STREAMSINK_H_

#include "dsp/channelsamplesink.h"


class DOA2StreamSink : public ChannelSampleSink
{
public:
    DOA2StreamSink();
    virtual ~DOA2StreamSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

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

    uint32_t m_log2Decim;
    uint32_t m_filterChainHash;
};


#endif // SDRBASE_DOA2STREAMSINK_H_
