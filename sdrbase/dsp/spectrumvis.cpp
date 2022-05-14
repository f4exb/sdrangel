///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
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

#include "SWGGLSpectrum.h"
#include "SWGSpectrumServer.h"
#include "SWGSuccessResponse.h"

#include "glspectruminterface.h"
#include "dspcommands.h"
#include "dspengine.h"
#include "fftfactory.h"
#include "util/messagequeue.h"

#include "spectrumvis.h"

MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureSpectrumVis, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureScalingFactor, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureWSpectrumOpenClose, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureWSpectrum, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgFrequencyZooming, Message)

const Real SpectrumVis::m_mult = (10.0f / log2(10.0f));

SpectrumVis::SpectrumVis(Real scalef) :
	BasebandSampleSink(),
    m_running(true),
	m_fft(nullptr),
    m_fftEngineSequence(0),
	m_fftBuffer(4096),
	m_powerSpectrum(4096),
    m_psd(4096),
	m_fftBufferFill(0),
	m_needMoreSamples(false),
    m_frequencyZoomFactor(1.0f),
    m_frequencyZoomPos(0.5f),
	m_scalef(scalef),
	m_glSpectrum(nullptr),
    m_specMax(0.0f),
    m_centerFrequency(0),
    m_sampleRate(48000),
	m_ofs(0),
    m_powFFTDiv(1.0),
    m_guiMessageQueue(nullptr),
	m_mutex(QMutex::Recursive)
{
	setObjectName("SpectrumVis");
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    applySettings(m_settings, true);
}

SpectrumVis::~SpectrumVis()
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    fftFactory->releaseEngine(m_settings.m_fftSize, false, m_fftEngineSequence);
}

void SpectrumVis::setScalef(Real scalef)
{
    MsgConfigureScalingFactor* cmd = new MsgConfigureScalingFactor(scalef);
    m_inputMessageQueue.push(cmd);
}

void SpectrumVis::configureWSSpectrum(const QString& address, uint16_t port)
{
    MsgConfigureWSpectrum* cmd = new MsgConfigureWSpectrum(address, port);
    m_inputMessageQueue.push(cmd);
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

void SpectrumVis::feed(const Complex *begin, unsigned int length)
{
	if (!m_glSpectrum && !m_wsSpectrum.socketOpened()) {
		return;
	}

    if (!m_mutex.tryLock(0)) { // prevent conflicts with configuration process
        return;
    }

    Complex c;
    Real v;
    int fftMin = (m_frequencyZoomFactor == 1.0f) ?
        0 : (m_frequencyZoomPos - (0.5f / m_frequencyZoomFactor)) * m_settings.m_fftSize;
    int fftMax = (m_frequencyZoomFactor == 1.0f) ?
        m_settings.m_fftSize : (m_frequencyZoomPos + (0.5f / m_frequencyZoomFactor)) * m_settings.m_fftSize;

    if (m_settings.m_averagingMode == SpectrumSettings::AvgModeNone)
    {
        for (int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < (int) length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();
            m_psd[i] = v/m_powFFTDiv;
            v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
            m_powerSpectrum[i] = v;
        }

        // send new data to visualisation
        if (m_glSpectrum)
        {
            m_glSpectrum->newSpectrum(
                &m_powerSpectrum.data()[fftMin],
                fftMax - fftMin,
                m_settings.m_fftSize
            );
        }

        // web socket spectrum connections
        if (m_wsSpectrum.socketOpened())
        {
            m_wsSpectrum.newSpectrum(
                m_powerSpectrum,
                m_settings.m_fftSize,
                m_centerFrequency,
                m_sampleRate,
                m_settings.m_linear,
                m_settings.m_ssb,
                m_settings.m_usb
            );
        }
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeMoving)
    {
        for (int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < (int) length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();
            v = m_movingAverage.storeAndGetAvg(v, i);
            m_psd[i] = v/m_powFFTDiv;
            v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
            m_powerSpectrum[i] = v;
        }

        // send new data to visualisation
        if (m_glSpectrum)
        {
            m_glSpectrum->newSpectrum(
                &m_powerSpectrum.data()[fftMin],
                fftMax - fftMin,
                m_settings.m_fftSize
            );
        }

        // web socket spectrum connections
        if (m_wsSpectrum.socketOpened())
        {
            m_wsSpectrum.newSpectrum(
                m_powerSpectrum,
                m_settings.m_fftSize,
                m_centerFrequency,
                m_sampleRate,
                m_settings.m_linear,
                m_settings.m_ssb,
                m_settings.m_usb
            );
        }

        m_movingAverage.nextAverage();
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeFixed)
    {
        double avg;

        for (int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < (int) length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();

            // result available
            if (m_fixedAverage.storeAndGetAvg(avg, v, i))
            {
                m_psd[i] = avg/m_powFFTDiv;
                avg = m_settings.m_linear ? avg/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                m_powerSpectrum[i] = avg;
            }
        }

        // result available
        if (m_fixedAverage.nextAverage())
        {
            // send new data to visualisation
            if (m_glSpectrum)
            {
                m_glSpectrum->newSpectrum(
                    &m_powerSpectrum.data()[fftMin],
                    fftMax - fftMin,
                    m_settings.m_fftSize
                );
            }

            // web socket spectrum connections
            if (m_wsSpectrum.socketOpened())
            {
                m_wsSpectrum.newSpectrum(
                    m_powerSpectrum,
                    m_settings.m_fftSize,
                    m_centerFrequency,
                    m_sampleRate,
                    m_settings.m_linear,
                    m_settings.m_ssb,
                    m_settings.m_usb
                );
            }
        }
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeMax)
    {
        double max;

        for (int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < (int) length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();

            // result available
            if (m_max.storeAndGetMax(max, v, i))
            {
                m_psd[i] = max/m_powFFTDiv;
                max = m_settings.m_linear ? max/m_powFFTDiv : m_mult * log2f(max) + m_ofs;
                m_powerSpectrum[i] = max;
            }
        }

        // result available
        if (m_max.nextMax())
        {
            // send new data to visualisation
            if (m_glSpectrum)
            {
                m_glSpectrum->newSpectrum(
                    &m_powerSpectrum.data()[fftMin],
                    fftMax - fftMin,
                    m_settings.m_fftSize
                );
            }

            // web socket spectrum connections
            if (m_wsSpectrum.socketOpened())
            {
                m_wsSpectrum.newSpectrum(
                    m_powerSpectrum,
                    m_settings.m_fftSize,
                    m_centerFrequency,
                    m_sampleRate,
                    m_settings.m_linear,
                    m_settings.m_ssb,
                    m_settings.m_usb
                );
            }
        }
    }

    m_mutex.unlock();
}

void SpectrumVis::feed(const ComplexVector::const_iterator& cbegin, const ComplexVector::const_iterator& end, bool positiveOnly)
{
    if (!m_running) {
        return;
    }

	// if no visualisation is set, send the samples to /dev/null
	if (!m_glSpectrum && !m_wsSpectrum.socketOpened()) {
		return;
	}

    if (!m_mutex.tryLock(0)) { // prevent conflicts with configuration process
        return;
    }

	ComplexVector::const_iterator begin(cbegin);

	while (begin < end)
	{
		std::size_t todo = end - begin;
		std::size_t samplesNeeded = m_refillSize - m_fftBufferFill;

		if (todo >= samplesNeeded)
		{
			// fill up the buffer
            std::copy(begin, begin + samplesNeeded, m_fftBuffer.begin() + m_fftBufferFill);
            begin += samplesNeeded;

            processFFT(positiveOnly);

			// advance buffer respecting the fft overlap factor
			std::copy(m_fftBuffer.begin() + m_refillSize, m_fftBuffer.end(), m_fftBuffer.begin());

			// start over
			m_fftBufferFill = m_overlapSize;
			m_needMoreSamples = false;
		}
		else
		{
			// not enough samples for FFT - just fill in new data and return
            std::copy(begin, end, m_fftBuffer.begin() + m_fftBufferFill);
            begin = end;
			m_fftBufferFill += todo;
			m_needMoreSamples = true;
		}
	}

	m_mutex.unlock();
}

void SpectrumVis::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    if (!m_running) {
        return;
    }

	// if no visualisation is set, send the samples to /dev/null
	if (!m_glSpectrum && !m_wsSpectrum.socketOpened()) {
		return;
	}

    if (!m_mutex.tryLock(0)) { // prevent conflicts with configuration process
        return;
    }

	SampleVector::const_iterator begin(cbegin);

	while (begin < end)
	{
		std::size_t todo = end - begin;
		std::size_t samplesNeeded = m_refillSize - m_fftBufferFill;

		if (todo >= samplesNeeded)
		{
			// fill up the buffer
			std::vector<Complex>::iterator it = m_fftBuffer.begin() + m_fftBufferFill;

			for (std::size_t i = 0; i < samplesNeeded; ++i, ++begin) {
				*it++ = Complex(begin->real() / m_scalef, begin->imag() / m_scalef);
			}

            processFFT(positiveOnly);

			// advance buffer respecting the fft overlap factor
			std::copy(m_fftBuffer.begin() + m_refillSize, m_fftBuffer.end(), m_fftBuffer.begin());

			// start over
			m_fftBufferFill = m_overlapSize;
			m_needMoreSamples = false;
		}
		else
		{
			// not enough samples for FFT - just fill in new data and return
			for (std::vector<Complex>::iterator it = m_fftBuffer.begin() + m_fftBufferFill; begin < end; ++begin) {
				*it++ = Complex(begin->real() / m_scalef, begin->imag() / m_scalef);
			}

			m_fftBufferFill += todo;
			m_needMoreSamples = true;
		}
	}

	m_mutex.unlock();
}

void SpectrumVis::processFFT(bool positiveOnly)
{
    int fftMin = (m_frequencyZoomFactor == 1.0f) ?
        0 : (m_frequencyZoomPos - (0.5f / m_frequencyZoomFactor)) * m_settings.m_fftSize;
    int fftMax = (m_frequencyZoomFactor == 1.0f) ?
        m_settings.m_fftSize : (m_frequencyZoomPos + (0.5f / m_frequencyZoomFactor)) * m_settings.m_fftSize;

    // apply fft window (and copy from m_fftBuffer to m_fftIn)
    m_window.apply(&m_fftBuffer[0], m_fft->in());

    // calculate FFT
    m_fft->transform();

    // extract power spectrum and reorder buckets
    const Complex* fftOut = m_fft->out();
    Complex c;
    Real v;
    std::size_t halfSize = m_settings.m_fftSize / 2;

    if (m_settings.m_averagingMode == SpectrumSettings::AvgModeNone)
    {
        m_specMax = 0.0f;

        if ( positiveOnly )
        {
            for (std::size_t i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                m_psd[i] = v/m_powFFTDiv;
                m_specMax = v > m_specMax ? v : m_specMax;
                v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
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
                m_psd[i] = v/m_powFFTDiv;
                m_specMax = v > m_specMax ? v : m_specMax;
                v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                m_powerSpectrum[i] = v;

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                m_psd[i + halfSize] = v/m_powFFTDiv;
                m_specMax = v > m_specMax ? v : m_specMax;
                v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                m_powerSpectrum[i + halfSize] = v;
            }
        }

        // send new data to visualisation
        if (m_glSpectrum)
        {
            m_glSpectrum->newSpectrum(
                &m_powerSpectrum.data()[fftMin],
                fftMax - fftMin,
                m_settings.m_fftSize
            );
        }

        // web socket spectrum connections
        if (m_wsSpectrum.socketOpened())
        {
            m_wsSpectrum.newSpectrum(
                m_powerSpectrum,
                m_settings.m_fftSize,
                m_centerFrequency,
                m_sampleRate,
                m_settings.m_linear,
                m_settings.m_ssb,
                m_settings.m_usb
            );
        }
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeMoving)
    {
        m_specMax = 0.0f;

        if ( positiveOnly )
        {
            for (std::size_t i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v = m_movingAverage.storeAndGetAvg(v, i);
                m_psd[i] = v/m_powFFTDiv;
                m_specMax = v > m_specMax ? v : m_specMax;
                v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
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
                m_psd[i] = v/m_powFFTDiv;
                m_specMax = v > m_specMax ? v : m_specMax;
                v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                m_powerSpectrum[i] = v;

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v = m_movingAverage.storeAndGetAvg(v, i);
                m_psd[i + halfSize] = v/m_powFFTDiv;
                m_specMax = v > m_specMax ? v : m_specMax;
                v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                m_powerSpectrum[i + halfSize] = v;
            }
        }

        // send new data to visualisation
        if (m_glSpectrum)
        {
            m_glSpectrum->newSpectrum(
                &m_powerSpectrum.data()[fftMin],
                fftMax - fftMin,
                m_settings.m_fftSize
            );
        }

        // web socket spectrum connections
        if (m_wsSpectrum.socketOpened())
        {
            m_wsSpectrum.newSpectrum(
                m_powerSpectrum,
                m_settings.m_fftSize,
                m_centerFrequency,
                m_sampleRate,
                m_settings.m_linear,
                m_settings.m_ssb,
                m_settings.m_usb
            );
        }

        m_movingAverage.nextAverage();
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeFixed)
    {
        double avg;
        Real specMax = 0.0f;

        if ( positiveOnly )
        {
            for (std::size_t i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();

                // result available
                if (m_fixedAverage.storeAndGetAvg(avg, v, i))
                {
                    m_psd[i] = avg/m_powFFTDiv;
                    specMax = avg > specMax ? avg : specMax;
                    avg = m_settings.m_linear ? avg/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
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

                // result available
                if (m_fixedAverage.storeAndGetAvg(avg, v, i+halfSize))
                {
                    m_psd[i] = avg/m_powFFTDiv;
                    specMax = avg > specMax ? avg : specMax;
                    avg = m_settings.m_linear ? avg/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                    m_powerSpectrum[i] = avg;
                }

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();

                // result available
                if (m_fixedAverage.storeAndGetAvg(avg, v, i))
                {
                    m_psd[i + halfSize] = avg/m_powFFTDiv;
                    specMax = avg > specMax ? avg : specMax;
                    avg = m_settings.m_linear ? avg/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                    m_powerSpectrum[i + halfSize] = avg;
                }
            }
        }

        // result available
        if (m_fixedAverage.nextAverage())
        {
            m_specMax = specMax;

            // send new data to visualisation
            if (m_glSpectrum)
            {
                m_glSpectrum->newSpectrum(
                    &m_powerSpectrum.data()[fftMin],
                    fftMax - fftMin,
                    m_settings.m_fftSize
                );
            }

            // web socket spectrum connections
            if (m_wsSpectrum.socketOpened())
            {
                m_wsSpectrum.newSpectrum(
                    m_powerSpectrum,
                    m_settings.m_fftSize,
                    m_centerFrequency,
                    m_sampleRate,
                    m_settings.m_linear,
                    m_settings.m_ssb,
                    m_settings.m_usb
                );
            }
        }
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeMax)
    {
        double max;
        Real specMax = 0.0f;

        if ( positiveOnly )
        {
            for (std::size_t i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();

                // result available
                if (m_max.storeAndGetMax(max, v, i))
                {
                    m_psd[i] = max/m_powFFTDiv;
                    specMax = max > specMax ? max : specMax;
                    max = m_settings.m_linear ? max/m_powFFTDiv : m_mult * log2f(max) + m_ofs;
                    m_powerSpectrum[i * 2] = max;
                    m_powerSpectrum[i * 2 + 1] = max;
                }
            }
        }
        else
        {
            for (std::size_t i = 0; i < halfSize; i++)
            {
                c = fftOut[i + halfSize];
                v = c.real() * c.real() + c.imag() * c.imag();

                // result available
                if (m_max.storeAndGetMax(max, v, i+halfSize))
                {
                    m_psd[i] = max/m_powFFTDiv;
                    specMax = max > specMax ? max : specMax;
                    max = m_settings.m_linear ? max/m_powFFTDiv : m_mult * log2f(max) + m_ofs;
                    m_powerSpectrum[i] = max;
                }

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();

                // result available
                if (m_max.storeAndGetMax(max, v, i))
                {
                    m_psd[i + halfSize] = max/m_powFFTDiv;
                    specMax = max > specMax ? max : specMax;
                    max = m_settings.m_linear ? max/m_powFFTDiv : m_mult * log2f(max) + m_ofs;
                    m_powerSpectrum[i + halfSize] = max;
                }
            }
        }

        // result available
        if (m_max.nextMax())
        {
            m_specMax = specMax;

            // send new data to visualisation
            if (m_glSpectrum)
            {
                m_glSpectrum->newSpectrum(
                    &m_powerSpectrum.data()[fftMin],
                    fftMax - fftMin,
                    m_settings.m_fftSize
                );
            }

            // web socket spectrum connections
            if (m_wsSpectrum.socketOpened())
            {
                m_wsSpectrum.newSpectrum(
                    m_powerSpectrum,
                    m_settings.m_fftSize,
                    m_centerFrequency,
                    m_sampleRate,
                    m_settings.m_linear,
                    m_settings.m_ssb,
                    m_settings.m_usb
                );
            }
        }
    }
}

void SpectrumVis::getZoomedPSDCopy(std::vector<Real>& copy) const
{
    int fftMin = (m_frequencyZoomFactor == 1.0f) ?
        0 : (m_frequencyZoomPos - (0.5f / m_frequencyZoomFactor)) * m_settings.m_fftSize;
    int fftMax = (m_frequencyZoomFactor == 1.0f) ?
        m_settings.m_fftSize : (m_frequencyZoomPos + (0.5f / m_frequencyZoomFactor)) * m_settings.m_fftSize;
    copy.assign(m_psd.begin() + fftMin, m_psd.begin() + fftMax);
}

void SpectrumVis::start()
{
    setRunning(true);

    if (getMessageQueueToGUI()) // propagate to GUI if any
    {
        MsgStartStop *msg = MsgStartStop::create(true);
        getMessageQueueToGUI()->push(msg);
    }
}

void SpectrumVis::stop()
{
    setRunning(false);

    if (getMessageQueueToGUI()) // propagate to GUI if any
    {
        MsgStartStop *msg = MsgStartStop::create(false);
        getMessageQueueToGUI()->push(msg);
    }
}

void SpectrumVis::pushMessage(Message *msg)
{
    m_inputMessageQueue.push(msg);
}

QString SpectrumVis::getSinkName()
{
    return objectName();
}

void SpectrumVis::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool SpectrumVis::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        // This is coming from device engine and will apply to main spectrum
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        qDebug() << "SpectrumVis::handleMessage: DSPSignalNotification:"
            << " centerFrequency: " << notif.getCenterFrequency()
            << " sampleRate: " << notif.getSampleRate();
        handleConfigureDSP(notif.getCenterFrequency(), notif.getSampleRate());
        return true;
    }
	else if (MsgConfigureSpectrumVis::match(message))
	{
        MsgConfigureSpectrumVis& cfg = (MsgConfigureSpectrumVis&) message;
        qDebug() << "SpectrumVis::handleMessage: MsgConfigureSpectrumVis";
        applySettings(cfg.getSettings(), cfg.getForce());
		return true;
	}
    else if (MsgConfigureScalingFactor::match(message))
    {
        MsgConfigureScalingFactor& conf = (MsgConfigureScalingFactor&) message;
        handleScalef(conf.getScalef());
        return true;
    }
    else if (MsgConfigureWSpectrumOpenClose::match(message))
    {
        MsgConfigureWSpectrumOpenClose& conf = (MsgConfigureWSpectrumOpenClose&) message;
        handleWSOpenClose(conf.getOpenClose());
        return true;
    }
    else if (MsgConfigureWSpectrum::match(message)) {
        MsgConfigureWSpectrum& conf = (MsgConfigureWSpectrum&) message;
        handleConfigureWSSpectrum(conf.getAddress(), conf.getPort());
        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        setRunning(cmd.getStartStop());
        return true;
    }
    else if (MsgFrequencyZooming::match(message))
    {
        MsgFrequencyZooming& cmd = (MsgFrequencyZooming&) message;
        m_frequencyZoomFactor = cmd.getFrequencyZoomFactor();
        m_frequencyZoomPos = cmd.getFrequencyZoomPos();
        return true;
    }
	else
	{
		return false;
	}
}

void SpectrumVis::applySettings(const SpectrumSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);

    int fftSize = settings.m_fftSize > (1<<SpectrumSettings::m_log2FFTSizeMax) ?
        (1<<SpectrumSettings::m_log2FFTSizeMax) :
        settings.m_fftSize < (1<<SpectrumSettings::m_log2FFTSizeMin) ?
            (1<<SpectrumSettings::m_log2FFTSizeMin) :
            settings.m_fftSize;

    qDebug() << "SpectrumVis::applySettings:"
        << " m_fftSize: " << fftSize
        << " m_fftWindow: " << settings.m_fftWindow
        << " m_fftOverlap: " << settings.m_fftOverlap
        << " m_averagingIndex: " << settings.m_averagingIndex
        << " m_averagingMode: " << settings.m_averagingMode
        << " m_refLevel: " << settings.m_refLevel
        << " m_powerRange: " << settings.m_powerRange
        << " m_fpsPeriodMs: " << settings.m_fpsPeriodMs
        << " m_linear: " << settings.m_linear
        << " m_ssb: " << settings.m_ssb
        << " m_usb: " << settings.m_usb
        << " m_wsSpectrumAddress: " << settings.m_wsSpectrumAddress
        << " m_wsSpectrumPort: " << settings.m_wsSpectrumPort
        << " force: " << force;

    if ((fftSize != m_settings.m_fftSize) || force)
    {
        FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();

        // release previous engine allocation if any
        if (m_fft) {
            fftFactory->releaseEngine(m_settings.m_fftSize, false, m_fftEngineSequence);
        }

        m_fftEngineSequence = fftFactory->getEngine(fftSize, false, &m_fft);
        m_ofs = 20.0f * log10f(1.0f / fftSize);
        m_powFFTDiv = fftSize * fftSize;

        if (fftSize > m_settings.m_fftSize)
        {
            m_fftBuffer.resize(fftSize);
            m_powerSpectrum.resize(fftSize);
            m_psd.resize(fftSize);
        }
    }

    if ((fftSize != m_settings.m_fftSize)
     || (settings.m_fftWindow != m_settings.m_fftWindow) || force)
    {
        m_window.create(settings.m_fftWindow, fftSize);
    }

    if ((fftSize != m_settings.m_fftSize)
     || (settings.m_fftOverlap != m_settings.m_fftOverlap) || force)
    {
        m_overlapSize = settings.m_fftOverlap < 0 ? 0 :
            settings.m_fftOverlap < fftSize/2 ? settings.m_fftOverlap : (fftSize/2) - 1;
        m_refillSize = fftSize - m_overlapSize;
        m_fftBufferFill = m_overlapSize;
    }

    if ((fftSize != m_settings.m_fftSize)
     || (settings.m_averagingIndex != m_settings.m_averagingIndex)
     || (settings.m_averagingMode != m_settings.m_averagingMode) || force)
    {
        unsigned int averagingValue = SpectrumSettings::getAveragingValue(settings.m_averagingIndex, settings.m_averagingMode);
        averagingValue = averagingValue > SpectrumSettings::getMaxAveragingValue(fftSize, settings.m_averagingMode) ?
            SpectrumSettings::getMaxAveragingValue(fftSize, settings.m_averagingMode) : averagingValue; // Capping to avoid out of memory condition
        m_movingAverage.resize(fftSize, averagingValue);
        m_fixedAverage.resize(fftSize, averagingValue);
        m_max.resize(fftSize, averagingValue);
    }

    if ((settings.m_wsSpectrumAddress != m_settings.m_wsSpectrumAddress)
     || (settings.m_wsSpectrumPort != m_settings.m_wsSpectrumPort) || force) {
         handleConfigureWSSpectrum(settings.m_wsSpectrumAddress, settings.m_wsSpectrumPort);
    }

    m_settings = settings;
    m_settings.m_fftSize = fftSize;
}

void SpectrumVis::handleConfigureDSP(uint64_t centerFrequency, int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_centerFrequency = centerFrequency;
    m_sampleRate = sampleRate;
}

void SpectrumVis::handleScalef(Real scalef)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_scalef = scalef;
}

void SpectrumVis::handleWSOpenClose(bool openClose)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (openClose) {
        m_wsSpectrum.openSocket();
    } else {
        m_wsSpectrum.closeSocket();
    }
}

void SpectrumVis::handleConfigureWSSpectrum(const QString& address, uint16_t port)
{
    m_wsSpectrum.setListeningAddress(address);
    m_wsSpectrum.setPort(port);

    if (m_wsSpectrum.socketOpened())
    {
        m_wsSpectrum.closeSocket();
        m_wsSpectrum.openSocket();
    }
}

int SpectrumVis::webapiSpectrumSettingsGet(SWGSDRangel::SWGGLSpectrum& response, QString& errorMessage) const
{
    (void) errorMessage;
    response.init();
    webapiFormatSpectrumSettings(response, m_settings);
    return 200;
}

int SpectrumVis::webapiSpectrumSettingsPutPatch(
    bool force,
    const QStringList& spectrumSettingsKeys,
    SWGSDRangel::SWGGLSpectrum& response, // query + response
    QString& errorMessage)
{
    (void) errorMessage;
    SpectrumSettings settings = m_settings;
    webapiUpdateSpectrumSettings(settings, spectrumSettingsKeys, response);

    MsgConfigureSpectrumVis *msg = MsgConfigureSpectrumVis::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureSpectrumVis *msgToGUI = MsgConfigureSpectrumVis::create(settings, force);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    webapiFormatSpectrumSettings(response, settings);
    return 200;
}

int SpectrumVis::webapiSpectrumServerGet(SWGSDRangel::SWGSpectrumServer& response, QString& errorMessage) const
{
    (void) errorMessage;
    bool serverRunning = m_wsSpectrum.socketOpened();
    QList<QHostAddress> peerHosts;
    QList<quint16> peerPorts;
    m_wsSpectrum.getPeers(peerHosts, peerPorts);
    response.init();
    response.setRun(serverRunning ? 1 : 0);

    QHostAddress serverAddress = m_wsSpectrum.getListeningAddress();

    if (serverAddress != QHostAddress::Null) {
        response.setListeningAddress(new QString(serverAddress.toString()));
    }

    uint16_t serverPort = m_wsSpectrum.getListeningPort();

    if (serverPort != 0) {
        response.setListeningPort(serverPort);
    }

    if (peerHosts.size() > 0)
    {
        response.setClients(new QList<SWGSDRangel::SWGSpectrumServer_clients*>);

        for (int i = 0; i < peerHosts.size(); i++)
        {
            response.getClients()->push_back(new SWGSDRangel::SWGSpectrumServer_clients);
            response.getClients()->back()->setAddress(new QString(peerHosts.at(i).toString()));
            response.getClients()->back()->setPort(peerPorts.at(i));
        }
    }

    return 200;
}

int SpectrumVis::webapiSpectrumServerPost(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage)
{
    (void) errorMessage;
    MsgConfigureWSpectrumOpenClose *msg = MsgConfigureWSpectrumOpenClose::create(true);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureWSpectrumOpenClose *msgToGui = MsgConfigureWSpectrumOpenClose::create(true);
        getMessageQueueToGUI()->push(msgToGui);
    }

    response.setMessage(new QString("Websocket spectrum server started"));
    return 200;
}

int SpectrumVis::webapiSpectrumServerDelete(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage)
{
    (void) errorMessage;
    MsgConfigureWSpectrumOpenClose *msg = MsgConfigureWSpectrumOpenClose::create(false);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureWSpectrumOpenClose *msgToGui = MsgConfigureWSpectrumOpenClose::create(false);
        getMessageQueueToGUI()->push(msgToGui);
    }

    response.setMessage(new QString("Websocket spectrum server stopped"));
    return 200;
}

void SpectrumVis::webapiFormatSpectrumSettings(SWGSDRangel::SWGGLSpectrum& response, const SpectrumSettings& settings)
{
    settings.formatTo(&response);
}

void SpectrumVis::webapiUpdateSpectrumSettings(
    SpectrumSettings& settings,
    const QStringList& spectrumSettingsKeys,
    SWGSDRangel::SWGGLSpectrum& response)
{
    QStringList prefixedKeys;

    for (const auto &key : spectrumSettingsKeys) {
        prefixedKeys.append(tr("spectrumConfig.%1").arg(key));
    }

    settings.updateFrom(prefixedKeys, &response);
}
