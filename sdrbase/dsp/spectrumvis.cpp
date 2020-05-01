///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
//                                                                               //
// Symbol synchronizer or symbol clock recovery mostly encapsulating             //
// liquid-dsp's symsync "object"                                                 //
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

#include "glspectruminterface.h"
#include "dspcommands.h"
#include "dspengine.h"
#include "fftfactory.h"
#include "util/messagequeue.h"

#include "spectrumvis.h"

#define MAX_FFT_SIZE 4096

#ifndef LINUX
inline double log2f(double n)
{
	return log(n) / log(2.0);
}
#endif

MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureSpectrumVis, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureDSP, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureScalingFactor, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureWSpectrumOpenClose, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureWSpectrum, Message)

const Real SpectrumVis::m_mult = (10.0f / log2f(10.0f));

SpectrumVis::SpectrumVis(Real scalef) :
	BasebandSampleSink(),
	m_fft(nullptr),
    m_fftEngineSequence(0),
	m_fftBuffer(MAX_FFT_SIZE),
	m_powerSpectrum(MAX_FFT_SIZE),
	m_fftBufferFill(0),
	m_needMoreSamples(false),
	m_scalef(scalef),
	m_glSpectrum(nullptr),
    m_centerFrequency(0),
    m_sampleRate(48000),
	m_ofs(0),
    m_powFFTDiv(1.0),
	m_mutex(QMutex::Recursive)
{
	setObjectName("SpectrumVis");
    applySettings(m_settings, true);
    //m_wsSpectrum.openSocket(); // FIXME: conditional
}

SpectrumVis::~SpectrumVis()
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    fftFactory->releaseEngine(m_settings.m_fftSize, false, m_fftEngineSequence);
}

void SpectrumVis::openWSSpectrum()
{
    MsgConfigureWSpectrumOpenClose *cmd = new MsgConfigureWSpectrumOpenClose(true);
    getInputMessageQueue()->push(cmd);
}

void SpectrumVis::closeWSSpectrum()
{
    MsgConfigureWSpectrumOpenClose *cmd = new MsgConfigureWSpectrumOpenClose(false);
    getInputMessageQueue()->push(cmd);
}

void SpectrumVis::configure(MessageQueue* msgQueue,
        int fftSize,
        float refLevel,
        float powerRange,
        int overlapPercent,
        unsigned int averagingNb,
        AvgMode averagingMode,
        FFTWindow::Function window,
        bool linear)
{
    GLSpectrumSettings settings = m_settings;
    settings.m_fftSize = fftSize;
    settings.m_refLevel = refLevel;
    settings.m_powerRange = powerRange;
    settings.m_fftOverlap = overlapPercent;
    settings.m_averagingMode = (GLSpectrumSettings::AveragingMode) averagingMode;
    settings.m_averagingIndex = GLSpectrumSettings::getAveragingIndex(averagingNb, settings.m_averagingMode);
    settings.m_fftWindow = window;
    settings.m_linear = linear;

    MsgConfigureSpectrumVis* cmd = MsgConfigureSpectrumVis::create(settings, false);

	msgQueue->push(cmd);
}

void SpectrumVis::configureDSP(uint64_t centerFrequency, int sampleRate)
{
    MsgConfigureDSP* cmd = new MsgConfigureDSP(centerFrequency, sampleRate);
    getInputMessageQueue()->push(cmd);
}

void SpectrumVis::setScalef(Real scalef)
{
    MsgConfigureScalingFactor* cmd = new MsgConfigureScalingFactor(scalef);
    getInputMessageQueue()->push(cmd);
}

void SpectrumVis::configureWSSpectrum(const QString& address, uint16_t port)
{
    MsgConfigureWSpectrum* cmd = new MsgConfigureWSpectrum(address, port);
    getInputMessageQueue()->push(cmd);
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

    if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeNone)
    {
        for (unsigned int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();
            v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
            m_powerSpectrum[i] = v;
        }

        // send new data to visualisation
        if (m_glSpectrum) {
            m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
        }

        // web socket spectrum connections
        if (m_wsSpectrum.socketOpened())
        {
            m_wsSpectrum.newSpectrum(
                m_powerSpectrum,
                m_settings.m_fftSize,
                m_settings.m_refLevel,
                m_settings.m_powerRange,
                m_centerFrequency,
                m_sampleRate,
                m_settings.m_linear
            );
        }
    }
    else if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeMoving)
    {
        for (unsigned int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();
            v = m_movingAverage.storeAndGetAvg(v, i);
            v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
            m_powerSpectrum[i] = v;
        }

        // send new data to visualisation
        if (m_glSpectrum) {
            m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
        }

        // web socket spectrum connections
        if (m_wsSpectrum.socketOpened())
        {
            m_wsSpectrum.newSpectrum(
                m_powerSpectrum,
                m_settings.m_fftSize,
                m_settings.m_refLevel,
                m_settings.m_powerRange,
                m_centerFrequency,
                m_sampleRate,
                m_settings.m_linear
            );
        }

        m_movingAverage.nextAverage();
    }
    else if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeFixed)
    {
        double avg;

        for (unsigned int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();

            // result available
            if (m_fixedAverage.storeAndGetAvg(avg, v, i))
            {
                avg = m_settings.m_linear ? avg/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                m_powerSpectrum[i] = avg;
            }
        }

        // result available
        if (m_fixedAverage.nextAverage())
        {
            // send new data to visualisation
            if (m_glSpectrum) {
                m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
            }

            // web socket spectrum connections
            if (m_wsSpectrum.socketOpened())
            {
                m_wsSpectrum.newSpectrum(
                    m_powerSpectrum,
                    m_settings.m_fftSize,
                    m_settings.m_refLevel,
                    m_settings.m_powerRange,
                    m_centerFrequency,
                    m_sampleRate,
                    m_settings.m_linear
                );
            }
        }
    }
    else if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeMax)
    {
        double max;

        for (unsigned int i = 0; i < m_settings.m_fftSize; i++)
        {
            if (i < length) {
                c = begin[i];
            } else {
                c = Complex{0,0};
            }

            v = c.real() * c.real() + c.imag() * c.imag();

            // result available
            if (m_max.storeAndGetMax(max, v, i))
            {
                max = m_settings.m_linear ? max/m_powFFTDiv : m_mult * log2f(max) + m_ofs;
                m_powerSpectrum[i] = max;
            }
        }

        // result available
        if (m_max.nextMax())
        {
            // send new data to visualisation
            if (m_glSpectrum) {
                m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
            }

            // web socket spectrum connections
            if (m_wsSpectrum.socketOpened())
            {
                m_wsSpectrum.newSpectrum(
                    m_powerSpectrum,
                    m_settings.m_fftSize,
                    m_settings.m_refLevel,
                    m_settings.m_powerRange,
                    m_centerFrequency,
                    m_sampleRate,
                    m_settings.m_linear
                );
            }
        }
    }

    m_mutex.unlock();
}

void SpectrumVis::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool positiveOnly)
{
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
			std::size_t halfSize = m_settings.m_fftSize / 2;

			if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeNone)
			{
                if ( positiveOnly )
                {
                    for (std::size_t i = 0; i < halfSize; i++)
                    {
                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();
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
                        v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                        m_powerSpectrum[i] = v;

                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();
                        v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
                        m_powerSpectrum[i + halfSize] = v;
                    }
                }

                // send new data to visualisation
                if (m_glSpectrum) {
                    m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
                }

                // web socket spectrum connections
                if (m_wsSpectrum.socketOpened())
                {
                    m_wsSpectrum.newSpectrum(
                        m_powerSpectrum,
                        m_settings.m_fftSize,
                        m_settings.m_refLevel,
                        m_settings.m_powerRange,
                        m_centerFrequency,
                        m_sampleRate,
                        m_settings.m_linear
                    );
                }
			}
			else if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeMoving)
			{
	            if ( positiveOnly )
	            {
	                for (std::size_t i = 0; i < halfSize; i++)
	                {
	                    c = fftOut[i];
	                    v = c.real() * c.real() + c.imag() * c.imag();
	                    v = m_movingAverage.storeAndGetAvg(v, i);
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
	                    v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
	                    m_powerSpectrum[i] = v;

	                    c = fftOut[i];
	                    v = c.real() * c.real() + c.imag() * c.imag();
	                    v = m_movingAverage.storeAndGetAvg(v, i);
	                    v = m_settings.m_linear ? v/m_powFFTDiv : m_mult * log2f(v) + m_ofs;
	                    m_powerSpectrum[i + halfSize] = v;
	                }
	            }

	            // send new data to visualisation
                if (m_glSpectrum) {
    	            m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
                }

                // web socket spectrum connections
                if (m_wsSpectrum.socketOpened())
                {
                    m_wsSpectrum.newSpectrum(
                        m_powerSpectrum,
                        m_settings.m_fftSize,
                        m_settings.m_refLevel,
                        m_settings.m_powerRange,
                        m_centerFrequency,
                        m_sampleRate,
                        m_settings.m_linear
                    );
                }

	            m_movingAverage.nextAverage();
			}
			else if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeFixed)
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
                            avg = m_settings.m_linear ? avg/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                            m_powerSpectrum[i] = avg;
                        }

                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();

                        // result available
                        if (m_fixedAverage.storeAndGetAvg(avg, v, i))
                        {
                            avg = m_settings.m_linear ? avg/m_powFFTDiv : m_mult * log2f(avg) + m_ofs;
                            m_powerSpectrum[i + halfSize] = avg;
                        }
                    }
                }

                // result available
                if (m_fixedAverage.nextAverage())
                {
                    // send new data to visualisation
                    if (m_glSpectrum) {
                        m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
                    }

                    // web socket spectrum connections
                    if (m_wsSpectrum.socketOpened())
                    {
                        m_wsSpectrum.newSpectrum(
                            m_powerSpectrum,
                            m_settings.m_fftSize,
                            m_settings.m_refLevel,
                            m_settings.m_powerRange,
                            m_centerFrequency,
                            m_sampleRate,
                            m_settings.m_linear
                        );
                    }
                }
			}
			else if (m_settings.m_averagingMode == GLSpectrumSettings::AvgModeMax)
			{
			    double max;

                if ( positiveOnly )
                {
                    for (std::size_t i = 0; i < halfSize; i++)
                    {
                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();

                        if (m_max.storeAndGetMax(max, v, i))
                        {
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
                            max = m_settings.m_linear ? max/m_powFFTDiv : m_mult * log2f(max) + m_ofs;
                            m_powerSpectrum[i] = max;
                        }

                        c = fftOut[i];
                        v = c.real() * c.real() + c.imag() * c.imag();

                        // result available
                        if (m_max.storeAndGetMax(max, v, i))
                        {
                            max = m_settings.m_linear ? max/m_powFFTDiv : m_mult * log2f(max) + m_ofs;
                            m_powerSpectrum[i + halfSize] = max;
                        }
                    }
                }

                // result available
                if (m_max.nextMax())
                {
                    // send new data to visualisation
                    if (m_glSpectrum) {
                        m_glSpectrum->newSpectrum(m_powerSpectrum, m_settings.m_fftSize);
                    }

                    // web socket spectrum connections
                    if (m_wsSpectrum.socketOpened())
                    {
                        m_wsSpectrum.newSpectrum(
                            m_powerSpectrum,
                            m_settings.m_fftSize,
                            m_settings.m_refLevel,
                            m_settings.m_powerRange,
                            m_centerFrequency,
                            m_sampleRate,
                            m_settings.m_linear
                        );
                    }
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

	 m_mutex.unlock();
}

void SpectrumVis::start()
{
}

void SpectrumVis::stop()
{
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
    else if (MsgConfigureDSP::match(message))
    {
        // This is coming from plugins GUI via configureDSP for auxiliary spectra
        MsgConfigureDSP& conf = (MsgConfigureDSP&) message;
        handleConfigureDSP(conf.getCenterFrequency(), conf.getSampleRate());
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
	else
	{
		return false;
	}
}

void SpectrumVis::applySettings(const GLSpectrumSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);

    int fftSize = settings.m_fftSize > MAX_FFT_SIZE ?
        MAX_FFT_SIZE :
        settings.m_fftSize < 64 ?
            64 :
            settings.m_fftSize;

    int overlapPercent = settings.m_fftOverlap > 100 ?
        100 :
        settings.m_fftOverlap < 0 ?
            0 :
            settings.m_fftOverlap;

    qDebug() << "SpectrumVis::applySettings:"
        << " m_fftSize: " << fftSize
        << " m_fftWindow: " << settings.m_fftWindow
        << " m_fftOverlap: " << overlapPercent
        << " m_averagingIndex: " << settings.m_averagingIndex
        << " m_averagingMode: " << settings.m_averagingMode
        << " m_refLevel: " << settings.m_refLevel
        << " m_powerRange: " << settings.m_powerRange
        << " m_linear: " << settings.m_linear
        << " force: " << force;

    if ((fftSize != m_settings.m_fftSize) || force)
    {
        FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
        fftFactory->releaseEngine(m_settings.m_fftSize, false, m_fftEngineSequence);
        m_fftEngineSequence = fftFactory->getEngine(fftSize, false, &m_fft);
        m_ofs = 20.0f * log10f(1.0f / fftSize);
        m_powFFTDiv = fftSize * fftSize;
    }

    if ((fftSize != m_settings.m_fftSize)
     || (settings.m_fftWindow != m_settings.m_fftWindow) || force)
    {
        m_window.create(settings.m_fftWindow, fftSize);
    }

    if ((fftSize != m_settings.m_fftSize)
     || (overlapPercent != m_settings.m_fftOverlap) || force)
    {
        m_overlapSize = (fftSize * overlapPercent) / 100;
        m_refillSize = fftSize - m_overlapSize;
        m_fftBufferFill = m_overlapSize;
    }

    if ((fftSize != m_settings.m_fftSize)
     || (settings.m_averagingIndex != m_settings.m_averagingIndex)
     || (settings.m_averagingMode != m_settings.m_averagingMode) || force)
    {
        unsigned int averagingValue = GLSpectrumSettings::getAveragingValue(settings.m_averagingIndex, settings.m_averagingMode);
        m_movingAverage.resize(fftSize, averagingValue > 1000 ? 1000 : averagingValue); // Capping to avoid out of memory condition
        m_fixedAverage.resize(fftSize, averagingValue);
        m_max.resize(fftSize, averagingValue);
    }

    m_settings = settings;
    m_settings.m_fftSize = fftSize;
    m_settings.m_fftOverlap = overlapPercent;
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
    QMutexLocker mutexLocker(&m_mutex);
    bool wsSpectrumWasOpen = false;

    if (m_wsSpectrum.socketOpened())
    {
        m_wsSpectrum.closeSocket();
        wsSpectrumWasOpen = true;
    }

    m_wsSpectrum.setListeningAddress(address);
    m_wsSpectrum.setPort(port);

    if (wsSpectrumWasOpen) {
        m_wsSpectrum.openSocket();
    }
}