#include "dsp/kissengine.h"

void KissEngine::configure(int n, bool inverse)
{
	m_fft.configure(n, inverse);
	if(n > m_in.size())
		m_in.resize(n);
	if(n > m_out.size())
		m_out.resize(n);
}

void KissEngine::transform()
{
	m_fft.transform(&m_in[0], &m_out[0]);
}

Complex* KissEngine::in()
{
	return &m_in[0];
}

Complex* KissEngine::out()
{
	return &m_out[0];
}
