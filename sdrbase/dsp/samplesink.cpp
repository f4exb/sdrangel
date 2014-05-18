#include "dsp/samplesink.h"

SampleSink::SampleSink()
{
}

SampleSink::~SampleSink()
{
}

#if 0
#include "samplesink.h"

SampleSink::SampleSink() :
	m_sinkBuffer(),
	m_sinkBufferFill(0)
{
}

void SampleSink::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end)
{
	size_t wus = workUnitSize();

	// make sure our buffer is big enough for at least one work unit
	if(m_sinkBuffer.size() < wus)
		m_sinkBuffer.resize(wus);

	while(begin < end) {
		// if the buffer contains something, keep filling it until it contains one complete work unit
		if((m_sinkBufferFill > 0) && (m_sinkBufferFill < wus)) {
			// check number if missing samples, but don't copy more than we have
			size_t len = wus - m_sinkBufferFill;
			if(len > (size_t)(end - begin))
				len = end - begin;
			// copy
			std::copy(begin, begin + len, m_sinkBuffer.begin() + m_sinkBufferFill);
			// adjust pointers
			m_sinkBufferFill += len;
			begin += len;
		}

		// if one complete work unit is in the buffer, feed it to the worker
		if(m_sinkBufferFill >= wus) {
			size_t done = 0;
			while(m_sinkBufferFill - done >= wus)
				done += work(m_sinkBuffer.begin() + done, m_sinkBuffer.begin() + done + wus);
			// now copy the remaining data to the front of the buffer (should be zero)
			if(m_sinkBufferFill - done > 0) {
				qDebug("error in SampleSink buffer management");
				std::copy(m_sinkBuffer.begin() + done, m_sinkBuffer.begin() + m_sinkBufferFill, m_sinkBuffer.begin());
			}
			m_sinkBufferFill -= done;
		}

		// if no remainder from last run is buffered and we have at least one work unit left, feed the data to the worker
		if(m_sinkBufferFill == 0) {
			while((size_t)(end - begin) > wus)
				begin += work(begin, begin + wus);
			// copy any remaining data to the buffer
			std::copy(begin, end, m_sinkBuffer.begin());
			m_sinkBufferFill = end - begin;
			begin += m_sinkBufferFill;
		}
	}
}
#endif
