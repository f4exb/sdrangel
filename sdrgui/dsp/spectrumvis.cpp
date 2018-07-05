#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

#define MAX_FFT_SIZE 4096

#ifndef LINUX
inline double log2f(double n)
{
	return log(n) / log(2.0);
}
#endif

MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureSpectrumVis, Message)

const Real SpectrumVis::m_mult = (10.0f / log2f(10.0f));

SpectrumVis::SpectrumVis(Real scalef, GLSpectrum* glSpectrum) :
	BasebandSampleSink(),
	m_fft(FFTEngine::create()),
	m_fftBuffer(MAX_FFT_SIZE),
	m_powerSpectrum(MAX_FFT_SIZE),
	m_fftBufferFill(0),
	m_needMoreSamples(false),
	m_scalef(scalef),
	m_glSpectrum(glSpectrum),
	m_averageNb(0),
	m_averagingMode(AvgModeNone),
	m_linear(false),
	m_ofs(0),
    m_powFFTDiv(1.0),
	m_mutex(QMutex::Recursive)
{
	setObjectName("SpectrumVis");
	handleConfigure(1024, 0, 0, AvgModeNone, FFTWindow::BlackmanHarris, false);
}

SpectrumVis::~SpectrumVis()
{
	delete m_fft;
}

void SpectrumVis::configure(MessageQueue* msgQueue,
        int fftSize,
        int overlapPercent,
        unsigned int averagingNb,
        int averagingMode,
        FFTWindow::Function window,
        bool linear)
{
	MsgConfigureSpectrumVis* cmd = new MsgConfigureSpectrumVis(fftSize, overlapPercent, averagingNb, averagingMode, window, linear);
	msgQueue->push(cmd);
}

void SpectrumVis::feedTriggered(const SampleVector::const_iterator& triggerPoint, const SampleVector::const_iterator& end, bool positiveOnly)
{
	feed(triggerPoint, end, positiveOnly); // normal feed from trigger point
	/*
	if (triggerPoint == end)
	{
		// the following piece of code allows to terminate the FFT that ends past the end of scope captured data
		// that is the spectrum will include the captured data
		// just do nothing if you want the spectrum to be included inside the scope captured data
		// that is to drop the FFT that dangles past the end of captured data
		if (m_needMoreSamples) {
			feed(begin, end, positiveOnly);
			m_needMoreSamples = false;      // force finish
		}
	}
	else
	{
		feed(triggerPoint, end, positiveOnly); // normal feed from trigger point
	}*/
}

void SpectrumVis::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool positiveOnly)
{
	// if no visualisation is set, send the samples to /dev/null

	if(m_glSpectrum == 0)
	{
		return;
	}

	SampleVector::const_iterator begin(cbegin);

	while (begin < end)
	{
		std::size_t todo = end - begin;
		std::size_t samplesNeeded = m_refillSize - m_fftBufferFill;

		if (todo >= samplesNeeded)
		{
			QMutexLocker mutexLocker(&m_mutex);

			// fill up the buffer
			std::vector<Complex>::iterator it = m_fftBuffer.begin() + m_fftBufferFill;

			for (std::size_t i = 0; i < samplesNeeded; ++i, ++begin)
			{
				*it++ = Complex(begin->real() / m_scalef, begin->imag() / m_scalef);
			}

			// apply fft window (and copy from m_fftBuffer to m_fftIn)
			m_window.apply(&m_fftBuffer[0], m_fft->in());

			// calculate FFT
			m_fft->transform();

			// extract power spectrum and reorder buckets
			const Complex* fftOut = m_fft->out();
			Complex c;
			Real v;
			std::size_t halfSize = m_fftSize / 2;

			if (m_averagingMode == AvgModeNone)
			{
                if ( positiveOnly )
                {
                    for (std::size_t i = 0; i < halfSize; i++)
                    {
                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();
                        v = m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                        m_powerSpectrum[i * 2] = v;
                        m_powerSpectrum[i * 2 + 1] = v;
                    }
                }
                else
                {
                    for (std::size_t i = 0; i < halfSize; i++)
                    {
                        c = fftOut[i + halfSize];
                        v = c.real() * c.real() + c.imag() * c.imag();
                        v = m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                        m_powerSpectrum[i] = v;

                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();
                        v = m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                        m_powerSpectrum[i + halfSize] = v;
                    }
                }

                // send new data to visualisation
                m_glSpectrum->newSpectrum(m_powerSpectrum, m_fftSize);
			}
			else if (m_averagingMode == AvgModeMoving)
			{
	            if ( positiveOnly )
	            {
	                for (std::size_t i = 0; i < halfSize; i++)
	                {
	                    c = fftOut[i];
	                    v = c.real() * c.real() + c.imag() * c.imag();
	                    v = m_movingAverage.storeAndGetAvg(v, i);
	                    v = m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
	                    m_powerSpectrum[i * 2] = v;
	                    m_powerSpectrum[i * 2 + 1] = v;
	                }
	            }
	            else
	            {
	                for (std::size_t i = 0; i < halfSize; i++)
	                {
	                    c = fftOut[i + halfSize];
	                    v = c.real() * c.real() + c.imag() * c.imag();
	                    v = m_movingAverage.storeAndGetAvg(v, i+halfSize);
	                    v = m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
	                    m_powerSpectrum[i] = v;

	                    c = fftOut[i];
	                    v = c.real() * c.real() + c.imag() * c.imag();
	                    v = m_movingAverage.storeAndGetAvg(v, i);
	                    v = m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
	                    m_powerSpectrum[i + halfSize] = v;
	                }
	            }

	            // send new data to visualisation
	            m_glSpectrum->newSpectrum(m_powerSpectrum, m_fftSize);
	            m_movingAverage.nextAverage();
			}
			else if (m_averagingMode == AvgModeFixed)
			{
			    double avg;

                if ( positiveOnly )
                {
                    for (std::size_t i = 0; i < halfSize; i++)
                    {
                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();

                        if (m_fixedAverage.storeAndGetAvg(avg, v, i))
                        {
                            avg = m_linear ? v/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                            m_powerSpectrum[i * 2] = avg;
                            m_powerSpectrum[i * 2 + 1] = avg;
                        }
                    }
                }
                else
                {
                    for (std::size_t i = 0; i < halfSize; i++)
                    {
                        c = fftOut[i + halfSize];
                        v = c.real() * c.real() + c.imag() * c.imag();

                        if (m_fixedAverage.storeAndGetAvg(avg, v, i+halfSize))
                        { // result available
                            avg = m_linear ? v/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                            m_powerSpectrum[i] = avg;
                        }

                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();

                        if (m_fixedAverage.storeAndGetAvg(avg, v, i))
                        { // result available
                            avg = m_linear ? v/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                            m_powerSpectrum[i + halfSize] = avg;
                        }
                    }
                }

                if (m_fixedAverage.nextAverage())
                { // result available
                    // send new data to visualisation
                    m_glSpectrum->newSpectrum(m_powerSpectrum, m_fftSize);
                }
			}

			// advance buffer respecting the fft overlap factor
			std::copy(m_fftBuffer.begin() + m_refillSize, m_fftBuffer.end(), m_fftBuffer.begin());

			// start over
			m_fftBufferFill = m_overlapSize;
			m_needMoreSamples = false;
		}
		else
		{
			// not enough samples for FFT - just fill in new data and return
			for(std::vector<Complex>::iterator it = m_fftBuffer.begin() + m_fftBufferFill; begin < end; ++begin)
			{
				*it++ = Complex(begin->real() / m_scalef, begin->imag() / m_scalef);
			}

			m_fftBufferFill += todo;
			m_needMoreSamples = true;
		}
	}
}

void SpectrumVis::start()
{
}

void SpectrumVis::stop()
{
}

bool SpectrumVis::handleMessage(const Message& message)
{
	if (MsgConfigureSpectrumVis::match(message))
	{
		MsgConfigureSpectrumVis& conf = (MsgConfigureSpectrumVis&) message;
		handleConfigure(conf.getFFTSize(),
		        conf.getOverlapPercent(),
		        conf.getAverageNb(),
		        conf.getAveragingMode(),
		        conf.getWindow(),
		        conf.getLinear());
		return true;
	}
	else
	{
		return false;
	}
}

void SpectrumVis::handleConfigure(int fftSize,
        int overlapPercent,
        unsigned int averageNb,
        AveragingMode averagingMode,
        FFTWindow::Function window,
        bool linear)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (fftSize > MAX_FFT_SIZE)
	{
		fftSize = MAX_FFT_SIZE;
	}
	else if (fftSize < 64)
	{
		fftSize = 64;
	}

	if (overlapPercent > 100)
	{
		m_overlapPercent = 100;
	}
	else if (overlapPercent < 0)
	{
		m_overlapPercent = 0;
	}
	else
	{
        m_overlapPercent = overlapPercent;
	}

	m_fftSize = fftSize;
	m_fft->configure(m_fftSize, false);
	m_window.create(window, m_fftSize);
	m_overlapSize = (m_fftSize * m_overlapPercent) / 100;
	m_refillSize = m_fftSize - m_overlapSize;
	m_fftBufferFill = m_overlapSize;
	m_movingAverage.resize(fftSize, averageNb);
	m_fixedAverage.resize(fftSize, averageNb);
	m_averageNb = averageNb;
	m_averagingMode = averagingMode;
	m_linear = linear;
	m_ofs = 20.0f * log10f(1.0f / m_fftSize);
	m_powFFTDiv = m_fftSize*m_fftSize;
}
