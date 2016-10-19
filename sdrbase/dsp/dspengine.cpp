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
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"


DSPEngine::DSPEngine() :
    m_deviceSourceEnginesUIDSequence(0),
    m_deviceSinkEnginesUIDSequence(0),
	m_audioSampleRate(48000) // Use default output device at 48 kHz
{
	m_dvSerialSupport = false;
}

DSPEngine::~DSPEngine()
{
    m_audioOutput.setOnExit(true);

    std::vector<DSPDeviceSourceEngine*>::iterator it = m_deviceSourceEngines.begin();

    while (it != m_deviceSourceEngines.end())
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

DSPDeviceSourceEngine *DSPEngine::addDeviceSourceEngine()
{
    m_deviceSourceEngines.push_back(new DSPDeviceSourceEngine(m_deviceSourceEnginesUIDSequence));
    m_deviceSourceEnginesUIDSequence++;
    return m_deviceSourceEngines.back();
}

void DSPEngine::removeLastDeviceSourceEngine()
{
    if (m_deviceSourceEngines.size() > 0)
    {
        DSPDeviceSourceEngine *lastDeviceEngine = m_deviceSourceEngines.back();
        delete lastDeviceEngine;
        m_deviceSourceEngines.pop_back();
        m_deviceSourceEnginesUIDSequence--;
    }
}

DSPDeviceSinkEngine *DSPEngine::addDeviceSinkEngine()
{
    m_deviceSinkEngines.push_back(new DSPDeviceSinkEngine(m_deviceSinkEnginesUIDSequence));
    m_deviceSinkEnginesUIDSequence++;
    return m_deviceSinkEngines.back();
}

void DSPEngine::removeLastDeviceSinkEngine()
{
    if (m_deviceSinkEngines.size() > 0)
    {
        DSPDeviceSinkEngine *lastDeviceEngine = m_deviceSinkEngines.back();
        delete lastDeviceEngine;
        m_deviceSinkEngines.pop_back();
        m_deviceSinkEnginesUIDSequence--;
    }
}

void DSPEngine::startAudio()
{
    m_audioOutput.start(-1, m_audioSampleRate);
    m_audioSampleRate = m_audioOutput.getRate(); // update with actual rate
}

void DSPEngine::stopAudio()
{
    m_audioOutput.stop();
}

void DSPEngine::startAudioImmediate()
{
    m_audioOutput.start(-1, m_audioSampleRate);
    m_audioSampleRate = m_audioOutput.getRate(); // update with actual rate
}

void DSPEngine::stopAudioImmediate()
{
    m_audioOutput.stop();
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

DSPDeviceSourceEngine *DSPEngine::getDeviceSourceEngineByUID(uint uid)
{
    std::vector<DSPDeviceSourceEngine*>::iterator it = m_deviceSourceEngines.begin();

    while (it != m_deviceSourceEngines.end())
    {
        if ((*it)->getUID() == uid)
        {
            return *it;
        }

        ++it;
    }

    return 0;
}

DSPDeviceSinkEngine *DSPEngine::getDeviceSinkEngineByUID(uint uid)
{
    std::vector<DSPDeviceSinkEngine*>::iterator it = m_deviceSinkEngines.begin();

    while (it != m_deviceSinkEngines.end())
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
