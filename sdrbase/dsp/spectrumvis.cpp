///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <karikoa@One.greyskull>                          //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
// Copyright (C) 2023 Arne Jünemann <das-iro@das-iro.de>                         //
// Copyright (C) 2023 Vladimir Pleskonjic                                        //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include "SWGGLSpectrumReport.h"
#include "SWGSpectrumActions.h"
#include "SWGGLSpectrumData.h"
#include "SWGGLSpectrumHistory.h"
#include "SWGSpectrumHistorySignal.h"
#include <QImage>
#include <QBuffer>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
#include "SWGSpectrumPeak.h"
#include "SWGSpectrumServer.h"
#include "SWGSuccessResponse.h"

#include "glspectruminterface.h"
#include "dspcommands.h"
#include "dspengine.h"
#include "fftfactory.h"
#include "util/messagequeue.h"
#include "util/profiler.h"

#include "spectrumvis.h"

MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureSpectrumVis, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureScalingFactor, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureWSpectrumOpenClose, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgConfigureWSpectrum, Message)
MESSAGE_CLASS_DEFINITION(SpectrumVis::MsgStartStop, Message)

const Real SpectrumVis::m_mult = (10.0f / log2(10.0f));

SpectrumVis::SpectrumVis(Real scalef) :
	BasebandSampleSink(),
    m_running(true),
	m_fft(nullptr),
    m_fftEngineSequence(0),
	m_fftBuffer(4096),
	m_powerSpectrum(4096),
    m_mathMemory(4096),
	m_fftBufferFill(0),
	m_needMoreSamples(false),
	m_scalef(scalef),
	m_glSpectrum(nullptr),
    m_specMax(0.0f),
    m_centerFrequency(0),
    m_sampleRate(48000),
    m_powFFTMul(1.0f),
    m_guiMessageQueue(nullptr)
{
	setObjectName("SpectrumVis");
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    applySettings(m_settings, true);
}

SpectrumVis::~SpectrumVis()
{
    if (m_glSpectrum) {
        m_glSpectrum->setSpectrumVis(nullptr);
    }

    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    fftFactory->releaseEngine(m_settings.m_fftSize, false, m_fftEngineSequence);
}

void SpectrumVis::setGLSpectrum(GLSpectrumInterface* glSpectrum)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_glSpectrum && (m_glSpectrum != glSpectrum)) {
        m_glSpectrum->setSpectrumVis(nullptr);
    }

    m_glSpectrum = glSpectrum;

    if (m_glSpectrum) {
        m_glSpectrum->setSpectrumVis(this);
    }
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

    processFFT(begin, false, false, length);

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
		std::size_t samplesNeeded = m_settings.m_fftSize - m_fftBufferFill;

		if (todo >= samplesNeeded)
		{
			// fill up the buffer
            std::copy(begin, begin + samplesNeeded, m_fftBuffer.begin() + m_fftBufferFill);
            begin += samplesNeeded;

            performFFT(positiveOnly);

			// advance buffer respecting the fft overlap factor
			// undefined behavior if the memory regions overlap, valid code for 50% overlap
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
		std::size_t samplesNeeded = m_settings.m_fftSize - m_fftBufferFill;

		if (todo >= samplesNeeded)
		{
			// fill up the buffer
			std::vector<Complex>::iterator it = m_fftBuffer.begin() + m_fftBufferFill;

			for (std::size_t i = 0; i < samplesNeeded; ++i, ++begin) {
				*it++ = Complex(begin->real() / m_scalef, begin->imag() / m_scalef);
			}

            performFFT(positiveOnly);

			// advance buffer respecting the fft overlap factor
			// undefined behavior if the memory regions overlap, valid code for 50% overlap
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

// Compute math operation on linear spectrum values
void SpectrumVis::mathLinear(std::vector<Real> &spectrum)
{
    if (m_settings.m_mathMode == SpectrumSettings::MathModeXMinusAvg)
    {
        for (std::size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] = std::max(spectrum[i] - (Real) m_mathMovingAverage.storeAndGetAvg(spectrum[i], i), 0.0f);
        }
    }
    else if ((m_settings.m_mathMode == SpectrumSettings::MathModeXMinusM1) || (m_settings.m_mathMode == SpectrumSettings::MathModeXMinusM2))
    {
        for (std::size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] = std::max(spectrum[i] - m_mathMemory[i], 0.0f);
        }
    }
}

// Compute math operation on dB spectrum values
void SpectrumVis::mathDB(std::vector<Real> &spectrum)
{
    if (m_settings.m_mathMode == SpectrumSettings::MathModeXMinusAvgDB)
    {
        for (std::size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] -= m_mathMovingAverage.storeAndGetAvg(spectrum[i], i);
        }
    }
    else if (m_settings.m_mathMode == SpectrumSettings::MathModeXMinusAvgPlusMinAvgDB)
    {
        Real minAvg = m_mathMovingAverage.getMin();
        for (std::size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] = spectrum[i] - m_mathMovingAverage.storeAndGetAvg(spectrum[i], i) + minAvg;
        }
    }
    else if (m_settings.m_mathMode == SpectrumSettings::MathModeAbsXMinusAvgDB)
    {
        for (std::size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] = abs(spectrum[i] - m_mathMovingAverage.storeAndGetAvg(spectrum[i], i));
        }
    }
    else if ((m_settings.m_mathMode == SpectrumSettings::MathModeXMinusM1DB) || (m_settings.m_mathMode == SpectrumSettings::MathModeXMinusM2DB))
    {
        for (std::size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] -= m_mathMemory[i];
        }
    }
    else if ((m_settings.m_mathMode == SpectrumSettings::MathModeAbsXMinusM1DB) || (m_settings.m_mathMode == SpectrumSettings::MathModeAbsXMinusM2DB))
    {
        for (std::size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] = abs(spectrum[i] - m_mathMemory[i]);
        }
    }
}

void SpectrumVis::performFFT(bool positiveOnly)
{
    // apply fft window (and copy from m_fftBuffer to m_fftIn)
    m_window.apply(&m_fftBuffer[0], m_fft->in());

    // calculate FFT
    m_fft->transform();

    // extract power spectrum and reorder buckets
    processFFT(m_fft->out(), true, positiveOnly, m_settings.m_fftSize);
}

void SpectrumVis::processFFT(const Complex* fftOut, bool reorder, bool positiveOnly, int fftSize)
{
    PROFILER_START();

    Complex c;
    Real v;
    int halfSize = fftSize / 2;
    bool ready = false;

    if (m_settings.m_averagingMode == SpectrumSettings::AvgModeNone)
    {
        if (positiveOnly)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                m_powerSpectrum[i * 2] = v;
                m_powerSpectrum[i * 2 + 1] = v;
            }
        }
        else if (reorder)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i + halfSize];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                m_powerSpectrum[i] = v;

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                m_powerSpectrum[i + halfSize] = v;
            }
        }
        else
        {
            for (int i = 0; i < fftSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                m_powerSpectrum[i] = v;
            }
        }

        ready = true;
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeMoving)
    {
        double avg;

        if (positiveOnly)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                avg = m_movingAverage.storeAndGetAvg(v, i);
                m_powerSpectrum[i * 2] = (Real) avg;
                m_powerSpectrum[i * 2 + 1] = (Real) avg;
            }
        }
        else if (reorder)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i + halfSize];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                avg = m_movingAverage.storeAndGetAvg(v, i+halfSize);
                m_powerSpectrum[i] = (Real) avg;

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                avg = m_movingAverage.storeAndGetAvg(v, i);
                m_powerSpectrum[i + halfSize] = (Real) avg;
            }
        }
        else
        {
            for (int i = 0; i < fftSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;
                avg = m_movingAverage.storeAndGetAvg(v, i);
                m_powerSpectrum[i] = (Real) avg;
            }
        }

        m_movingAverage.nextAverage();
        ready = true;
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeFixed)
    {
        double avg;

        if (positiveOnly)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_fixedAverage.storeAndGetAvg(avg, v, i))
                {
                    m_powerSpectrum[i * 2] = avg;
                    m_powerSpectrum[i * 2 + 1] = avg;
                }
            }
        }
        else if (reorder)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i + halfSize];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_fixedAverage.storeAndGetAvg(avg, v, i+halfSize)) {
                    m_powerSpectrum[i] = avg;
                }

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_fixedAverage.storeAndGetAvg(avg, v, i)) {
                    m_powerSpectrum[i + halfSize] = avg;
                }
            }
        }
        else
        {
            for (int i = 0; i < fftSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_fixedAverage.storeAndGetAvg(avg, v, i)) {
                    m_powerSpectrum[i] = avg;
                }
            }
        }

        // result available
        if (m_fixedAverage.nextAverage()) {
            ready = true;
        }
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeMax)
    {
        Real max;

        if (positiveOnly)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_max.storeAndGetMax(max, v, i))
                {
                    m_powerSpectrum[i * 2] = max;
                    m_powerSpectrum[i * 2 + 1] = max;
                }
            }
        }
        else if (reorder)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i + halfSize];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_max.storeAndGetMax(max, v, i+halfSize)) {
                    m_powerSpectrum[i] = max;
                }

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_max.storeAndGetMax(max, v, i)) {
                    m_powerSpectrum[i + halfSize] = max;
                }
            }
        }
        else
        {
            for (int i = 0; i < fftSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_max.storeAndGetMax(max, v, i)) {
                    m_powerSpectrum[i] = max;
                }
            }
        }

        // result available
        if (m_max.nextMax()) {
            ready = true;
        }
    }
    else if (m_settings.m_averagingMode == SpectrumSettings::AvgModeMin)
    {
        Real min;

        if (positiveOnly)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                if (m_min.storeAndGetMin(min, v, i))
                {
                    m_powerSpectrum[i * 2] = min;
                    m_powerSpectrum[i * 2 + 1] = min;
                }
            }
        }
        else if (reorder)
        {
            for (int i = 0; i < halfSize; i++)
            {
                c = fftOut[i + halfSize];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_min.storeAndGetMin(min, v, i+halfSize)) {
                    m_powerSpectrum[i] = min;
                }

                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                // result available
                if (m_min.storeAndGetMin(min, v, i)) {
                    m_powerSpectrum[i + halfSize] = min;
                }
            }
        }
        else
        {
            for (int i = 0; i < fftSize; i++)
            {
                c = fftOut[i];
                v = c.real() * c.real() + c.imag() * c.imag();
                v *= m_powFFTMul;

                if (m_min.storeAndGetMin(min, v, i)) {
                    m_powerSpectrum[i] = min;
                }
            }
        }

        // result available
        if (m_min.nextMin()) {
            ready = true;
        }
    }

    if (ready)
    {
        for (int i = fftSize; i < m_settings.m_fftSize; i++) {
            m_powerSpectrum[i] = 0.0f;
        }

        // Calculate maximum value in spectrum
        m_specMax = *std::max_element(&m_powerSpectrum[0], &m_powerSpectrum[m_settings.m_fftSize]);

        // Perform math operation on linear value
        if (m_settings.m_mathMode != SpectrumSettings::MathModeNone) {
            mathLinear(m_powerSpectrum);
        }

        // Convert to dB
        if (!m_settings.m_linear)
        {
            for (int i = 0; i < m_settings.m_fftSize; i++) {
                m_powerSpectrum[i] = m_mult * log2fapprox(m_powerSpectrum[i]);
            }
        }

        // Perform math operation on dB value
        if (m_settings.m_mathMode != SpectrumSettings::MathModeNone) {
            mathDB(m_powerSpectrum);
        }

        if (m_settings.mathAverageUsed()) {
            m_mathMovingAverage.nextAverage();
        }

        m_powerSpectrumUpdated = QDateTime::currentDateTimeUtc();

        // Stop profiling before newSpectrum, as we profile that separately
        PROFILER_STOP("processFFT");

        // send new data to visualisation
        if (m_glSpectrum)
        {
            m_glSpectrum->newSpectrum(
                &m_powerSpectrum.data()[0],
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
    else
    {
        PROFILER_STOP("processFFT");
    }

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

	while ((message = m_inputMessageQueue.pop()))
	{
		if (!handleMessage(*message)) {
			qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
		}
		delete message;
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
        m_powFFTMul = 1.0f / (fftSize * fftSize);

        if (fftSize > m_settings.m_fftSize)
        {
            m_fftBuffer.resize(fftSize);
            m_powerSpectrum.resize(fftSize);
            m_mathMemory.resize(fftSize);
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
			settings.m_fftOverlap < fftSize ? settings.m_fftOverlap : (fftSize - 1);
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
        m_min.resize(fftSize, averagingValue);
    }

    if ((fftSize != m_settings.m_fftSize)
        || (settings.m_mathAvgCount != m_settings.m_mathAvgCount) || force)
    {
        m_mathMovingAverage.resize(fftSize, settings.m_mathAvgCount);
    }

    if (settings.m_mathMode != m_settings.m_mathMode) {
        m_mathMovingAverage.clear();
    }

    if ((settings.m_wsSpectrumAddress != m_settings.m_wsSpectrumAddress)
     || (settings.m_wsSpectrumPort != m_settings.m_wsSpectrumPort) || force) {
         handleConfigureWSSpectrum(settings.m_wsSpectrumAddress, settings.m_wsSpectrumPort);
    }

    m_settings = settings;
    m_settings.m_fftSize = fftSize;

    if (m_guiMessageQueue)
    {
        MsgConfigureSpectrumVis *msg = MsgConfigureSpectrumVis::create(m_settings, false);
        m_guiMessageQueue->push(msg);
    }
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

// The history reduced to a matrix of rows by bins, in dB, over one frequency range and one
// device setting: rows taken while the device was tuned or sampling differently are left out,
// since their bins mean other frequencies
struct SpectrumVis::ReducedHistory
{
    int m_bins = 0;
    int m_rows = 0;
    qint64 m_startFrequency = 0;    //!< Centre of the first bin
    double m_binBandwidth = 0.0;
    qint64 m_centerFrequency = 0;
    int m_bandwidth = 0;
    int m_skippedRows = 0;
    std::vector<float> m_values;    //!< rows * bins, oldest row first
    QList<QDateTime> m_times;
};

// Collects the rows of the display's scroll buffer into a reduced matrix. Returns an HTTP status
int SpectrumVis::reduceHistory(double seconds, int bins, qint64 startFrequency, qint64 stopFrequency, int maxRows,
    ReducedHistory& out, QString& errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    const int fftSize = m_settings.m_fftSize;
    const qint64 centerFrequency = m_centerFrequency;
    const int bandwidth = m_sampleRate;
    const bool linear = m_settings.m_linear;
    const bool scrolling = m_settings.m_scrollBar;
    GLSpectrumInterface *display = m_glSpectrum;

    if (!display)
    {
        errorMessage = "There is no spectrum display for this device set, so there is no history";
        return 404;
    }

    if (!scrolling)
    {
        errorMessage = "The spectrum history is kept only while scrolling is enabled: "
            "set scrollBar to 1 (and scrollLength to the rows to keep) in the spectrum settings";
        return 404;
    }

    if ((fftSize <= 0) || (bandwidth <= 0))
    {
        errorMessage = "The spectrum has no sample rate yet";
        return 500;
    }

    bins = std::max(1, std::min(bins, m_maxDataBins));
    const double hzPerBin = bandwidth / (double) fftSize;
    const qint64 spectrumStart = centerFrequency - bandwidth / 2;
    const qint64 wantedStart = (startFrequency == 0) ? spectrumStart : startFrequency;
    const qint64 wantedStop = (stopFrequency == 0) ? spectrumStart + bandwidth : stopFrequency;

    if (wantedStop <= wantedStart)
    {
        errorMessage = "stopFrequency must be above startFrequency";
        return 400;
    }

    int firstBin = (int) std::floor((wantedStart - spectrumStart) / hzPerBin);
    int lastBin = (int) std::ceil((wantedStop - spectrumStart) / hzPerBin) - 1;
    firstBin = std::max(0, std::min(firstBin, fftSize - 1));
    lastBin = std::max(firstBin, std::min(lastBin, fftSize - 1));
    const int available = lastBin - firstBin + 1;
    const int returned = std::min(bins, available);
    const double binsPerGroup = available / (double) returned;

    out.m_bins = returned;
    out.m_centerFrequency = centerFrequency;
    out.m_bandwidth = bandwidth;
    out.m_binBandwidth = hzPerBin * binsPerGroup;
    out.m_startFrequency = spectrumStart + (qint64) (firstBin * hzPerBin + out.m_binBandwidth / 2);
    out.m_rows = 0;
    out.m_skippedRows = 0;
    out.m_values.clear();
    out.m_times.clear();

    const QDateTime since = QDateTime::currentDateTimeUtc().addMSecs(-(qint64) (seconds * 1000.0));

    const bool have = display->getSpectrumHistory(since, maxRows,
        [&](const Real *spectrum, int rowFftSize, quint32 rowSampleRate, qint64 rowCentre, const QDateTime& dateTime)
        {
            if ((rowFftSize != fftSize) || ((int) rowSampleRate != bandwidth) || (rowCentre != centerFrequency))
            {
                out.m_skippedRows++;
                return;
            }

            const size_t base = out.m_values.size();
            out.m_values.resize(base + returned);

            for (int i = 0; i < returned; i++)
            {
                const int from = firstBin + (int) std::floor(i * binsPerGroup);
                int to = firstBin + (int) std::floor((i + 1) * binsPerGroup) - 1;
                to = std::max(from, std::min(to, lastBin));
                float value = spectrum[from];

                for (int bin = from + 1; bin <= to; bin++) {
                    value = std::max(value, spectrum[bin]);
                }

                if (linear) {
                    value = (float) (10.0 * std::log10(std::max((double) value, 1e-20)));
                }

                out.m_values[base + i] = value;
            }

            out.m_times.append(dateTime);
            out.m_rows++;
        });

    if (!have)
    {
        errorMessage = "The spectrum history is kept only while scrolling is enabled in the spectrum display";
        return 404;
    }

    if (out.m_rows == 0)
    {
        errorMessage = out.m_skippedRows > 0
            ? QString("The %1 rows of history in that time were taken with the device tuned or sampling differently, so they do not describe the current band").arg(out.m_skippedRows)
            : "There is no spectrum history yet for that time: the spectrum is only computed while it is displayed, and rows arrive at the display's refresh rate";
        return 404;
    }

    return 200;
}

int SpectrumVis::webapiSpectrumHistoryGet(double seconds, int bins, qint64 startFrequency, qint64 stopFrequency, double thresholdDb,
    SWGSDRangel::SWGGLSpectrumHistory& response, QString& errorMessage) const
{
    ReducedHistory history;
    const int status = reduceHistory(seconds, bins, startFrequency, stopFrequency, m_maxHistoryRows, history, errorMessage);

    if (status != 200) {
        return status;
    }

    const int rows = history.m_rows;
    const int n = history.m_bins;
    std::vector<float> maxDb(n, -1e9f);
    std::vector<double> meanDb(n, 0.0);

    for (int r = 0; r < rows; r++)
    {
        const float *row = &history.m_values[(size_t) r * n];

        for (int i = 0; i < n; i++)
        {
            maxDb[i] = std::max(maxDb[i], row[i]);
            meanDb[i] += row[i];
        }
    }

    for (int i = 0; i < n; i++) {
        meanDb[i] /= rows;
    }

    // The floor is the median of the per bin means: most bins of most bands hold no signal,
    // and a mean survives the odd burst that a minimum would not
    std::vector<double> sorted(meanDb);
    std::sort(sorted.begin(), sorted.end());
    const double floorDb = sorted[n / 2];
    const double threshold = floorDb + thresholdDb;
    std::vector<int> above(n, 0);

    for (int r = 0; r < rows; r++)
    {
        const float *row = &history.m_values[(size_t) r * n];

        for (int i = 0; i < n; i++)
        {
            if (row[i] > threshold) {
                above[i]++;
            }
        }
    }

    response.init();
    response.setCenterFrequency(history.m_centerFrequency);
    response.setBandwidth(history.m_bandwidth);
    response.setStartFrequency(history.m_startFrequency);
    response.setStopFrequency(history.m_startFrequency + (qint64) ((n - 1) * history.m_binBandwidth));
    response.setBinBandwidth((int) history.m_binBandwidth);
    response.setBins(n);
    response.setRows(rows);
    response.setSkippedRows(history.m_skippedRows);
    response.setFirstTime(new QString(history.m_times.first().toString(Qt::ISODateWithMs)));
    response.setLastTime(new QString(history.m_times.last().toString(Qt::ISODateWithMs)));
    response.setSeconds((float) (history.m_times.first().msecsTo(history.m_times.last()) / 1000.0));
    response.setFloorDb((float) floorDb);
    response.setThresholdDb((float) threshold);

    for (int i = 0; i < n; i++)
    {
        response.getMaxDb()->append(maxDb[i]);
        response.getMeanDb()->append((float) meanDb[i]);
        response.getOccupancy()->append((float) (above[i] / (double) rows));
    }

    // Signals: runs of bins whose maximum rose above the threshold. Each is judged over the
    // rows as a whole, so a signal hopping within its run still counts as one transmission
    // per row, and its duty cycle is the share of rows in which any of its bins was up
    struct Found { int m_first; int m_last; };
    std::vector<Found> runs;

    for (int i = 0; i < n; i++)
    {
        if (maxDb[i] <= threshold) {
            continue;
        }

        if (!runs.empty() && (runs.back().m_last == i - 1)) {
            runs.back().m_last = i;
        } else {
            runs.push_back({i, i});
        }
    }

    // Over thousands of rows, noise alone rises past a threshold a few dB above the floor now
    // and then, so a run that was up in only a row or two of many is noise, not a signal,
    // unless it stood well clear when it did
    const int minActive = std::max(2, rows / 200);

    for (const Found& run : runs)
    {
        int peakBin = run.m_first;
        int active = 0;
        int firstRow = -1;
        int lastRow = -1;

        for (int i = run.m_first + 1; i <= run.m_last; i++)
        {
            if (maxDb[i] > maxDb[peakBin]) {
                peakBin = i;
            }
        }

        for (int r = 0; r < rows; r++)
        {
            const float *row = &history.m_values[(size_t) r * n];
            bool up = false;

            for (int i = run.m_first; (i <= run.m_last) && !up; i++) {
                up = row[i] > threshold;
            }

            if (up)
            {
                active++;
                lastRow = r;

                if (firstRow < 0) {
                    firstRow = r;
                }
            }
        }

        if ((active < minActive) && (maxDb[peakBin] < threshold + 6.0)) {
            continue;
        }

        SWGSDRangel::SWGSpectrumHistorySignal *found = new SWGSDRangel::SWGSpectrumHistorySignal();
        found->init();
        found->setFrequency(history.m_startFrequency + (qint64) (peakBin * history.m_binBandwidth));
        found->setStartFrequency(history.m_startFrequency + (qint64) (run.m_first * history.m_binBandwidth - history.m_binBandwidth / 2));
        found->setStopFrequency(history.m_startFrequency + (qint64) (run.m_last * history.m_binBandwidth + history.m_binBandwidth / 2));
        found->setBandwidth((int) ((run.m_last - run.m_first + 1) * history.m_binBandwidth));
        found->setPeakDb(maxDb[peakBin]);
        found->setDutyCycle((float) (active / (double) rows));
        found->setFirstSeen(new QString(history.m_times[firstRow].toString(Qt::ISODateWithMs)));
        found->setLastSeen(new QString(history.m_times[lastRow].toString(Qt::ISODateWithMs)));
        response.getSignalList()->append(found);
    }

    response.setSignalCount(response.getSignalList()->size());
    return 200;
}

int SpectrumVis::webapiSpectrumHistoryImageGet(double seconds, int bins, qint64 startFrequency, qint64 stopFrequency, int maxRows,
    QByteArray& png, QJsonObject& description, QString& errorMessage) const
{
    ReducedHistory history;
    maxRows = std::max(1, std::min(maxRows, m_maxHistoryRows));
    const int status = reduceHistory(seconds, bins, startFrequency, stopFrequency, maxRows, history, errorMessage);

    if (status != 200) {
        return status;
    }

    const int n = history.m_bins;
    const int rows = history.m_rows;

    // Greyscale from the floor to the peak: the floor as the median of everything, black a
    // little below it so noise is not solid black, white at the hottest bin
    std::vector<float> sorted(history.m_values);
    std::sort(sorted.begin(), sorted.end());
    const float floorDb = sorted[sorted.size() / 2];
    const float peakDb = sorted.back();
    const float low = floorDb - 5.0f;
    const float high = std::max(peakDb, low + 10.0f);

    QImage image(n, rows, QImage::Format_Grayscale8);

    for (int r = 0; r < rows; r++)
    {
        uchar *line = image.scanLine(r);
        const float *row = &history.m_values[(size_t) r * n];

        for (int i = 0; i < n; i++)
        {
            float v = (row[i] - low) / (high - low);
            v = std::max(0.0f, std::min(1.0f, v));
            line[i] = (uchar) (v * 255.0f);
        }
    }

    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);

    if (!image.save(&buffer, "PNG"))
    {
        errorMessage = "The image could not be encoded";
        return 500;
    }

    description["width"] = n;
    description["height"] = rows;
    description["startFrequency"] = (double) history.m_startFrequency;
    description["stopFrequency"] = (double) (history.m_startFrequency + (qint64) ((n - 1) * history.m_binBandwidth));
    description["binBandwidth"] = history.m_binBandwidth;
    description["centerFrequency"] = (double) history.m_centerFrequency;
    description["firstTime"] = history.m_times.first().toString(Qt::ISODateWithMs);
    description["lastTime"] = history.m_times.last().toString(Qt::ISODateWithMs);
    description["seconds"] = history.m_times.first().msecsTo(history.m_times.last()) / 1000.0;
    description["blackDb"] = low;
    description["whiteDb"] = high;
    description["skippedRows"] = history.m_skippedRows;
    return 200;
}

int SpectrumVis::webapiActionsPost(const QStringList& spectrumActionsKeys, SWGSDRangel::SWGSpectrumActions& query, QString& errorMessage)
{
    static const QStringList known = {"autoscale", "clearSpectrum", "resetMeasurements", "freeze", "gotoMarker"};
    QStringList unknown;

    for (const QString& key : spectrumActionsKeys)
    {
        if (!known.contains(key)) {
            unknown.append(key);
        }
    }

    // An action that does not exist is a mistake worth reporting: accepting it would answer
    // "submitted successfully" to a request that does nothing at all
    if (!unknown.isEmpty())
    {
        errorMessage = QString("Unknown action %1. This spectrum takes: %2")
            .arg(unknown.join(", ")).arg(known.join(", "));
        return 400;
    }

    if (spectrumActionsKeys.isEmpty())
    {
        errorMessage = QString("No action given. This spectrum takes: %1").arg(known.join(", "));
        return 400;
    }

    // Everything except freeze acts on what is displayed. Checked before anything is applied, so
    // that a request carrying both freeze and a display action is refused whole rather than
    // leaving the spectrum stopped behind a 400
    if (!m_glSpectrum)
    {
        for (const QString& key : {QString("autoscale"), QString("clearSpectrum"), QString("resetMeasurements"), QString("gotoMarker")})
        {
            if (spectrumActionsKeys.contains(key))
            {
                errorMessage = QString("%1 acts on the spectrum display, which this instance does not have").arg(key);
                return 400;
            }
        }
    }

    // freeze is ours: it stops the spectrum rather than its display, so it needs no GUI
    if (spectrumActionsKeys.contains("freeze"))
    {
        MsgStartStop *msg = MsgStartStop::create(query.getFreeze() == 0);
        getInputMessageQueue()->push(msg);
    }

    if (!m_glSpectrum) {
        return 202;
    }

    if (spectrumActionsKeys.contains("autoscale") && (query.getAutoscale() != 0)) {
        m_glSpectrum->spectrumAutoscale();
    }

    if (spectrumActionsKeys.contains("clearSpectrum") && (query.getClearSpectrum() != 0)) {
        m_glSpectrum->spectrumClear();
    }

    if (spectrumActionsKeys.contains("resetMeasurements") && (query.getResetMeasurements() != 0)) {
        m_glSpectrum->spectrumResetMeasurements();
    }

    if (spectrumActionsKeys.contains("gotoMarker")) {
        m_glSpectrum->spectrumGotoMarker(query.getGotoMarker());
    }

    return 202;
}


int SpectrumVis::webapiSpectrumDataGet(
    int bins,
    qint64 startFrequency,
    qint64 stopFrequency,
    const QString& reduce,
    SWGSDRangel::SWGGLSpectrumData& response,
    QString& errorMessage) const
{
    if ((reduce != "max") && (reduce != "mean"))
    {
        errorMessage = QString("reduce must be max or mean, not %1").arg(reduce);
        return 400;
    }

    if ((bins < 1) || (bins > m_maxDataBins))
    {
        errorMessage = QString("bins must be between 1 and %1").arg(m_maxDataBins);
        return 400;
    }

    std::vector<Real> spectrum;
    int fftSize;
    qint64 centerFrequency;
    int bandwidth;
    bool linear;
    QDateTime updated;

    {
        // feed() takes this with tryLock and gives up rather than waiting, so the worst this can
        // do to the sample path is cost it one FFT
        QMutexLocker locker(&m_mutex);
        fftSize = m_settings.m_fftSize;
        centerFrequency = m_centerFrequency;
        bandwidth = m_sampleRate;
        linear = m_settings.m_linear;
        updated = m_powerSpectrumUpdated;

        if ((int) m_powerSpectrum.size() < fftSize)
        {
            errorMessage = "The spectrum has not been computed yet";
            return 500;
        }

        // Both divide the bin arithmetic below, and a device that has notified a zero sample rate
        // leaves the spectrum sized but unscaled
        if ((fftSize <= 0) || (bandwidth <= 0))
        {
            errorMessage = "The spectrum has no sample rate yet";
            return 500;
        }

        spectrum.assign(m_powerSpectrum.begin(), m_powerSpectrum.begin() + fftSize);
    }

    double hzPerBin = bandwidth / (double) fftSize;
    qint64 spectrumStart = centerFrequency - bandwidth / 2;

    // The range asked for, clamped to what there is
    qint64 wantedStart = (startFrequency == 0) ? spectrumStart : startFrequency;
    qint64 wantedStop = (stopFrequency == 0) ? spectrumStart + bandwidth : stopFrequency;

    if (wantedStop <= wantedStart)
    {
        errorMessage = "stopFrequency must be above startFrequency";
        return 400;
    }

    int firstBin = (int) std::floor((wantedStart - spectrumStart) / hzPerBin);
    int lastBin = (int) std::ceil((wantedStop - spectrumStart) / hzPerBin) - 1;
    firstBin = std::max(0, std::min(firstBin, fftSize - 1));
    lastBin = std::max(firstBin, std::min(lastBin, fftSize - 1));

    int available = lastBin - firstBin + 1;
    int returned = std::min(bins, available);
    double binsPerGroup = available / (double) returned;

    response.setPower(new QList<float>());

    for (int i = 0; i < returned; i++)
    {
        int from = firstBin + (int) std::floor(i * binsPerGroup);
        int to = firstBin + (int) std::floor((i + 1) * binsPerGroup) - 1;
        to = std::max(from, std::min(to, lastBin));

        float value = spectrum[from];

        if (reduce == "max")
        {
            for (int bin = from + 1; bin <= to; bin++) {
                value = std::max(value, spectrum[bin]);
            }
        }
        else
        {
            double sum = 0.0;

            for (int bin = from; bin <= to; bin++) {
                sum += spectrum[bin];
            }

            value = (float) (sum / (to - from + 1));
        }

        response.getPower()->append(value);
    }

    response.setCenterFrequency(centerFrequency);
    response.setBandwidth(bandwidth);
    response.setFftSize(fftSize);
    response.setLinear(linear ? 1 : 0);
    response.setBins(returned);
    response.setReduce(new QString(reduce));
    response.setBinBandwidth((float) (available * hzPerBin / returned));
    // The centre of the first and last groups, so a caller can place every value without
    // knowing how the grouping fell out
    response.setStartFrequency((qint64) (spectrumStart + (firstBin + binsPerGroup / 2.0) * hzPerBin));
    response.setStopFrequency((qint64) (spectrumStart + (firstBin + (returned - 0.5) * binsPerGroup) * hzPerBin));

    if (updated.isValid()) {
        response.setUpdated(new QString(updated.toString(Qt::ISODateWithMs)));
    }

    return 200;
}

int SpectrumVis::webapiSpectrumReportGet(SWGSDRangel::SWGGLSpectrumReport& response, QString& errorMessage) const
{
    (void) errorMessage;
    SpectrumMeasurementResults results;
    getMeasurementResults(results);
    response.setMeasurement((int) results.m_measurement);

    if (results.m_updated.isValid()) {
        response.setUpdated(new QString(results.m_updated.toString(Qt::ISODateWithMs)));
    }

    // Only the fields the selected measurement actually produces are set, so that a caller cannot
    // read a zero from a measurement that was never taken
    switch (results.m_measurement)
    {
    case SpectrumSettings::MeasurementPeaks:
        response.setPeaks(new QList<SWGSDRangel::SWGSpectrumPeak *>);

        for (const auto& peak : results.m_peaks)
        {
            SWGSDRangel::SWGSpectrumPeak *swgPeak = new SWGSDRangel::SWGSpectrumPeak();
            swgPeak->setFrequency(peak.m_frequency);
            swgPeak->setPower(peak.m_power);
            response.getPeaks()->append(swgPeak);
        }

        break;

    case SpectrumSettings::MeasurementChannelPower:
        response.setChannelPower(results.m_channelPower);
        break;

    case SpectrumSettings::MeasurementAdjacentChannelPower:
        response.setAdjChannelPowerLeft(results.m_adjChannelPowerLeft);
        response.setAdjChannelPowerLeftRatio(results.m_adjChannelPowerLeftACPR);
        response.setAdjChannelPowerCentre(results.m_adjChannelPowerCentre);
        response.setAdjChannelPowerRight(results.m_adjChannelPowerRight);
        response.setAdjChannelPowerRightRatio(results.m_adjChannelPowerRightACPR);
        break;

    case SpectrumSettings::MeasurementOccupiedBandwidth:
        response.setOccupiedBandwidth(results.m_occupiedBandwidth);
        break;

    case SpectrumSettings::Measurement3dBBandwidth:
        response.setBandwidth3dB(results.m_bandwidth3dB);
        break;

    case SpectrumSettings::MeasurementSNR:
        response.setSnr(results.m_snr);
        response.setSnfr(results.m_snfr);
        response.setThd(results.m_thd);
        response.setThdPlusNoise(results.m_thdPlusNoise);
        response.setSinad(results.m_sinad);
        response.setSfdr(results.m_sfdr);
        break;

    default:
        break;
    }

    return 200;
}

void SpectrumVis::setMeasurementResults(const SpectrumMeasurementResults& results)
{
    QMutexLocker locker(&m_measurementResultsMutex);
    m_measurementResults = results;
}

void SpectrumVis::getMeasurementResults(SpectrumMeasurementResults& results) const
{
    QMutexLocker locker(&m_measurementResultsMutex);
    results = m_measurementResults;
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

// To calculate power, the usual equation:
//    10*log10(v=V1/V2), where V2=fftSize^2
// is calculated using log2 instead, with:
//   mult = 10.0f / log2(10.0f)
//   dB = m_mult * log2f(v)
// However, while the gcc version of log2f is twice as fast as log10f,
// MSVC version is 6x slower.
// Also, we don't need full accuracy of log2f for calculating the power for the spectrum,
// so we can use the following approximation to get a good speed-up for both compilers:
// https://www.vplesko.com/posts/replacing_log2f.html
// https://www.vplesko.com/assets/replacing_log2f/main.c.txt
float SpectrumVis::log2fapprox(float x) const
{
    // IEEE 754 representation constants.
    const int32_t mantissaLen = 23;
    const int32_t mantissaMask = (1 << mantissaLen) - 1;
    const int32_t baseExponent = -127;

    // Reinterpret x as int in a standard compliant way.
    int32_t xi;
    memcpy(&xi, &x, sizeof(xi));

    // Calculate exponent of x.
    float e = (float)((xi >> mantissaLen) + baseExponent);

    // Calculate mantissa of x. It will be in range [1, 2).
    float m;
    int32_t mxi = (xi & mantissaMask) | ((-baseExponent) << mantissaLen);
    memcpy(&m, &mxi, sizeof(m));

    // Use Remez algorithm-generated approximation polynomial
    // for log2(a) where a is in range [1, 2].
    float l = 0.15824871f;
    l = l * m + -1.051875f;
    l = l * m + 3.0478842f;
    l = l * m + -2.1536207f;

    // Add exponent to the calculation.
    // Final log is log2(m*2^e)=log2(m)+e.
    l += e;

    return l;
}

void SpectrumVis::setMathMemory(const QList<Real> &values)
{
    QMutexLocker mutexLocker(&m_mutex);

    int s = std::min((int) values.size(), m_settings.m_fftSize);

    for (int i = 0; i < s; i++) {
        m_mathMemory[i] = values[i];
    }
}

void SpectrumVis::getMathMovingAverageCopy(QList<Real>& copy)
{
    QMutexLocker mutexLocker(&m_mutex);

    std::vector<Real> averages;
    m_mathMovingAverage.getAverages<Real>(averages);
    copy = QList<Real>(averages.begin(), averages.end());
}
