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

SpectrumVis::SpectrumVis(GLSpectrum* glSpectrum) :
	BasebandSampleSink(),
	m_fft(FFTEngine::create()),
	m_fftBuffer(MAX_FFT_SIZE),
	m_logPowerSpectrum(MAX_FFT_SIZE),
	m_fftBufferFill(0),
	m_needMoreSamples(false),
	m_glSpectrum(glSpectrum),
	m_mutex(QMutex::Recursive)
{
	setObjectName("SpectrumVis");
	handleConfigure(1024, 0, FFTWindow::BlackmanHarris);
}

SpectrumVis::~SpectrumVis()
{
	delete m_fft;
}

void SpectrumVis::configure(MessageQueue* msgQueue, int fftSize, int overlapPercent, FFTWindow::Function window)
{
	MsgConfigureSpectrumVis* cmd = new MsgConfigureSpectrumVis(fftSize, overlapPercent, window);
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
				*it++ = Complex(begin->real() / 32768.0f, begin->imag() / 32768.0f);
			}

			// apply fft window (and copy from m_fftBuffer to m_fftIn)
			m_window.apply(&m_fftBuffer[0], m_fft->in());

			// calculate FFT
			m_fft->transform();

			// extract power spectrum and reorder buckets
			Real ofs = 20.0f * log10f(1.0f / m_fftSize);
			Real mult = (10.0f / log2f(10.0f));
			const Complex* fftOut = m_fft->out();
			Complex c;
			Real v;
			std::size_t halfSize = m_fftSize / 2;

			if ( positiveOnly )
			{
				for (std::size_t i = 0; i < halfSize; i++)
				{
					c = fftOut[i];
					v = c.real() * c.real() + c.imag() * c.imag();
					v = mult * log2f(v) + ofs;
					m_logPowerSpectrum[i * 2] = v;
					m_logPowerSpectrum[i * 2 + 1] = v;
				}
			}
			else
			{
				for (std::size_t i = 0; i < halfSize; i++)
				{
					c = fftOut[i + halfSize];
					v = c.real() * c.real() + c.imag() * c.imag();
					v = mult * log2f(v) + ofs;
					m_logPowerSpectrum[i] = v;

					c = fftOut[i];
					v = c.real() * c.real() + c.imag() * c.imag();
					v = mult * log2f(v) + ofs;
					m_logPowerSpectrum[i + halfSize] = v;
				}
			}

			// send new data to visualisation
			m_glSpectrum->newSpectrum(m_logPowerSpectrum, m_fftSize);

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
				*it++ = Complex(begin->real() / 32768.0f, begin->imag() / 32768.0f);
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
		handleConfigure(conf.getFFTSize(), conf.getOverlapPercent(), conf.getWindow());
		return true;
	}
	else
	{
		return false;
	}
}

void SpectrumVis::handleConfigure(int fftSize, int overlapPercent, FFTWindow::Function window)
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

	m_fftSize = fftSize;
	m_overlapPercent = overlapPercent;
	m_fft->configure(m_fftSize, false);
	m_window.create(window, m_fftSize);
	m_overlapSize = (m_fftSize * m_overlapPercent) / 100;
	m_refillSize = m_fftSize - m_overlapSize;
	m_fftBufferFill = m_overlapSize;
}
