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
    m_deviceEnginesUIDSequence(0),
	m_audioSampleRate(48000), // Use default output device at 48 kHz
	m_audioUsageCount(0)
{
    //m_deviceEngines.push_back(new DSPDeviceEngine(0)); // TODO: multi device support
	m_dvSerialSupport = false;
}

DSPEngine::~DSPEngine()
{
    std::vector<DSPDeviceEngine*>::iterator it = m_deviceEngines.begin();

    while (it != m_deviceEngines.end())
    {
        delete *it;
        ++it;
    }
}

Q_GLOBAL_STATIC(DSPEngine, dspEngine)
DSPEngine *DSPEngine::instance()
{
	return dspEngine;
}

DSPDeviceEngine *DSPEngine::addDeviceEngine()
{
    m_deviceEngines.push_back(new DSPDeviceEngine(m_deviceEnginesUIDSequence));
    m_deviceEnginesUIDSequence++;
    return m_deviceEngines.back();
}

void DSPEngine::removeLastDeviceEngine()
{
    if (m_deviceEngines.size() > 0)
    {
        DSPDeviceEngine *lastDeviceEngine = m_deviceEngines.back();
        delete lastDeviceEngine;
        m_deviceEngines.pop_back();
    }
}

void DSPEngine::stopAllAcquisitions()
{
    std::vector<DSPDeviceEngine*>::iterator it = m_deviceEngines.begin();

    while (it != m_deviceEngines.end())
    {
        (*it)->stopAcquistion();
        stopAudio();
        ++it;
    }
}

void DSPEngine::stopAllDeviceEngines()
{
    std::vector<DSPDeviceEngine*>::iterator it = m_deviceEngines.begin();

    while (it != m_deviceEngines.end())
    {
        (*it)->stop();
        ++it;
    }
}

void DSPEngine::startAudio()
{
    if (m_audioUsageCount == 0)
    {
        m_audioOutput.start(-1, m_audioSampleRate);
        m_audioSampleRate = m_audioOutput.getRate(); // update with actual rate
    }

    m_audioUsageCount++;
}

void DSPEngine::stopAudio()
{
    if (m_audioUsageCount > 0)
    {
        m_audioUsageCount--;

        if (m_audioUsageCount == 0)
        {
            m_audioOutput.stop();
        }
    }
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

DSPDeviceEngine *DSPEngine::getDeviceEngineByUID(uint uid)
{
    std::vector<DSPDeviceEngine*>::iterator it = m_deviceEngines.begin();

    while (it != m_deviceEngines.end())
    {
        if ((*it)->getUID() == uid)
        {
            return *it;
        }

        ++it;
    }

    return 0;
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
