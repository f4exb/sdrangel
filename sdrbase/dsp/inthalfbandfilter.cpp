#include "dsp/inthalfbandfilter.h"

IntHalfbandFilter::IntHalfbandFilter()
{
	for(int i = 0; i < HB_FILTERORDER + 1; i++) {
		m_samples[i][0] = 0;
		m_samples[i][1] = 0;
	}
	m_ptr = 0;
	m_state = 0;
}
