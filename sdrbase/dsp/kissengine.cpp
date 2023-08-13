#include "dsp/kissengine.h"

const QString KissEngine::m_name = "Kiss";

QString KissEngine::getName() const
{
    return m_name;
}

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
    PROFILER_START()

	m_fft.transform(&m_in[0], &m_out[0]);

    PROFILER_STOP(QString("%1 %2").arg(getName()).arg(m_out.size()))
}

Complex* KissEngine::in()
{
	return &m_in[0];
}

Complex* KissEngine::out()
{
	return &m_out[0];
}

void KissEngine::setReuse(bool reuse)
{
    (void) reuse;
}
