///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QGlobalStatic>
#include <QThread>

#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/fftfactory.h"

DSPEngine::DSPEngine() :
    m_deviceSourceEnginesUIDSequence(0),
    m_deviceSinkEnginesUIDSequence(0),
    m_deviceMIMOEnginesUIDSequence(0),
    m_audioInputDeviceIndex(-1),    // default device
    m_audioOutputDeviceIndex(-1),   // default device
    m_fftFactory(nullptr)
{
	m_dvSerialSupport = false;
    m_mimoSupport = false;
    m_masterTimer.start(50);
}

DSPEngine::~DSPEngine()
{
    std::vector<DSPDeviceSourceEngine*>::iterator it = m_deviceSourceEngines.begin();

    while (it != m_deviceSourceEngines.end())
    {
        delete *it;
        ++it;
    }

    if (m_fftFactory) {
        delete m_fftFactory;
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

DSPDeviceMIMOEngine *DSPEngine::addDeviceMIMOEngine()
{
    m_deviceMIMOEngines.push_back(new DSPDeviceMIMOEngine(m_deviceMIMOEnginesUIDSequence));
    m_deviceMIMOEnginesUIDSequence++;
    return m_deviceMIMOEngines.back();
}

void DSPEngine::removeLastDeviceMIMOEngine()
{
    if (m_deviceMIMOEngines.size() > 0)
    {
        DSPDeviceMIMOEngine *lastDeviceEngine = m_deviceMIMOEngines.back();
        delete lastDeviceEngine;
        m_deviceMIMOEngines.pop_back();
        m_deviceMIMOEnginesUIDSequence--;
    }
}

DSPDeviceSourceEngine *DSPEngine::getDeviceSourceEngineByUID(uint uid)
{
    std::vector<DSPDeviceSourceEngine*>::iterator it = m_deviceSourceEngines.begin();

    while (it != m_deviceSourceEngines.end())
    {
        if ((*it)->getUID() == uid) {
            return *it;
        }

        ++it;
    }

    return nullptr;
}

DSPDeviceSinkEngine *DSPEngine::getDeviceSinkEngineByUID(uint uid)
{
    std::vector<DSPDeviceSinkEngine*>::iterator it = m_deviceSinkEngines.begin();

    while (it != m_deviceSinkEngines.end())
    {
        if ((*it)->getUID() == uid) {
            return *it;
        }

        ++it;
    }

    return nullptr;
}

DSPDeviceMIMOEngine *DSPEngine::getDeviceMIMOEngineByUID(uint uid)
{
    std::vector<DSPDeviceMIMOEngine*>::iterator it = m_deviceMIMOEngines.begin();

    while (it != m_deviceMIMOEngines.end())
    {
        if ((*it)->getUID() == uid) {
            return *it;
        }

        ++it;
    }

    return nullptr;
}

bool DSPEngine::hasDVSerialSupport()
{
    return m_ambeEngine.getNbDevices() > 0;
}

void DSPEngine::setDVSerialSupport(bool support)
{ (void) support; }

void DSPEngine::getDVSerialNames(std::vector<std::string>& deviceNames)
{
    std::vector<QString> qDeviceRefs;
    m_ambeEngine.getDeviceRefs(qDeviceRefs);
    deviceNames.clear();

    for (std::vector<QString>::const_iterator it = qDeviceRefs.begin(); it != qDeviceRefs.end(); ++it) {
        deviceNames.push_back(it->toStdString());
    }
}

void DSPEngine::pushMbeFrame(
        const unsigned char *mbeFrame,
        int mbeRateIndex,
        int mbeVolumeIndex,
        unsigned char channels,
        bool useHP,
        int upsampling,
        AudioFifo *audioFifo)
{
    m_ambeEngine.pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, useHP, upsampling, audioFifo);
}

void DSPEngine::createFFTFactory(const QString& fftWisdomFileName)
{
    m_fftFactory = new FFTFactory(fftWisdomFileName);
}

void DSPEngine::preAllocateFFTs()
{
    m_fftFactory->preallocate(7, 10, 1, 0); // pre-acllocate forward FFT only 1 per size from 128 to 1024
}