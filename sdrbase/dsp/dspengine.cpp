///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QGlobalStatic>
#include <QThread>
#include "dsp/dspengine.h"

DSPEngine::DSPEngine() :
	m_audioSampleRate(48000) // Use default output device at 48 kHz
{
	m_deviceEngine = new DSPDeviceEngine();
}

DSPEngine::~DSPEngine()
{
	delete m_deviceEngine;
}

Q_GLOBAL_STATIC(DSPEngine, dspEngine)
DSPEngine *DSPEngine::instance()
{
	return dspEngine;
}

MessageQueue* DSPEngine::getInputMessageQueue()
{
	return m_deviceEngine->getInputMessageQueue();
}

MessageQueue* DSPEngine::getOutputMessageQueue()
{
	return m_deviceEngine->getOutputMessageQueue();
}

void DSPEngine::start()
{
	qDebug("DSPEngine::start");
	m_deviceEngine->start();
}

void DSPEngine::stop()
{
	qDebug("DSPEngine::stop");
	m_audioOutput.stop();
	m_deviceEngine->stop();
}

bool DSPEngine::initAcquisition()
{
	qDebug("DSPEngine::initAcquisition");
	return m_deviceEngine->initAcquisition();
}

bool DSPEngine::startAcquisition()
{
	qDebug("DSPEngine::startAcquisition");
	bool started = m_deviceEngine->startAcquisition();

	if (started)
	{
		m_audioOutput.start(-1, m_audioSampleRate);
		m_audioSampleRate = m_audioOutput.getRate(); // update with actual rate
	}

	return started;
}

void DSPEngine::stopAcquistion()
{
	qDebug("DSPEngine::stopAcquistion");
	m_audioOutput.stop();
	m_deviceEngine->stopAcquistion();
}

void DSPEngine::setSource(SampleSource* source)
{
	qDebug("DSPEngine::setSource");
	m_deviceEngine->setSource(source);
}

void DSPEngine::setSourceSequence(int sequence)
{
	qDebug("DSPEngine::setSource");
	m_deviceEngine->setSourceSequence(sequence);
}

void DSPEngine::addSink(SampleSink* sink)
{
	qDebug("DSPEngine::setSource");
	m_deviceEngine->addSink(sink);
}

void DSPEngine::removeSink(SampleSink* sink)
{
	qDebug("DSPEngine::removeSink");
	m_deviceEngine->removeSink(sink);
}

void DSPEngine::addThreadedSink(ThreadedSampleSink* sink)
{
	qDebug("DSPEngine::addThreadedSink");
	m_deviceEngine->addThreadedSink(sink);
}

void DSPEngine::removeThreadedSink(ThreadedSampleSink* sink)
{
	qDebug("DSPEngine::addThreadedSink");
	m_deviceEngine->removeThreadedSink(sink);
}

void DSPEngine::addAudioSink(AudioFifo* audioFifo)
{
	qDebug("DSPEngine::addAudioSink");
	m_audioOutput.addFifo(audioFifo);
}

void DSPEngine::removeAudioSink(AudioFifo* audioFifo)
{
	qDebug("DSPEngine::removeAudioSink");
	m_audioOutput.removeFifo(audioFifo);
}

void DSPEngine::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
	qDebug("DSPEngine::configureCorrections");
	m_deviceEngine->configureCorrections(dcOffsetCorrection, iqImbalanceCorrection);
}

DSPDeviceEngine::State DSPEngine::state() const
{
	return m_deviceEngine->state();
}

QString DSPEngine::errorMessage()
{
	return m_deviceEngine->errorMessage();
}

QString DSPEngine::sourceDeviceDescription()
{
	return m_deviceEngine->sourceDeviceDescription();
}

void DSPEngine::setDVSerialSupport(bool support)
{
#ifdef DSD_USE_SERIALDV
    if (support)
    {
        m_dvSerialSupport = m_dvSerialEngine.scan();
    }
    else
    {
        m_dvSerialEngine.release();
        m_dvSerialSupport = false;
    }
#endif
}
