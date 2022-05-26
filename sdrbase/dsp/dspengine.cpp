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
    QList<DSPDeviceSourceEngine*>::iterator it = m_deviceSourceEngines.begin();

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
    m_deviceEngineReferences.push_back(DeviceEngineReference{0, m_deviceSourceEngines.back(), nullptr, nullptr});
    return m_deviceSourceEngines.back();
}

void DSPEngine::removeLastDeviceSourceEngine()
{
    if (m_deviceSourceEngines.size() > 0)
    {
        DSPDeviceSourceEngine *lastDeviceEngine = m_deviceSourceEngines.back();
        delete lastDeviceEngine;
        m_deviceSourceEngines.pop_back();

        for (int i = 0; i < m_deviceEngineReferences.size(); i++)
        {
            if (m_deviceEngineReferences[i].m_deviceSourceEngine == lastDeviceEngine)
            {
                m_deviceEngineReferences.removeAt(i);
                break;
            }
        }
    }
}

DSPDeviceSinkEngine *DSPEngine::addDeviceSinkEngine()
{
    m_deviceSinkEngines.push_back(new DSPDeviceSinkEngine(m_deviceSinkEnginesUIDSequence));
    m_deviceSinkEnginesUIDSequence++;
    m_deviceEngineReferences.push_back(DeviceEngineReference{1, nullptr, m_deviceSinkEngines.back(), nullptr});
    return m_deviceSinkEngines.back();
}

void DSPEngine::removeLastDeviceSinkEngine()
{
    if (m_deviceSinkEngines.size() > 0)
    {
        DSPDeviceSinkEngine *lastDeviceEngine = m_deviceSinkEngines.back();
        delete lastDeviceEngine;
        m_deviceSinkEngines.pop_back();

        for (int i = 0; i < m_deviceEngineReferences.size(); i++)
        {
            if (m_deviceEngineReferences[i].m_deviceSinkEngine == lastDeviceEngine)
            {
                m_deviceEngineReferences.removeAt(i);
                break;
            }
        }
    }
}

DSPDeviceMIMOEngine *DSPEngine::addDeviceMIMOEngine()
{
    m_deviceMIMOEngines.push_back(new DSPDeviceMIMOEngine(m_deviceMIMOEnginesUIDSequence));
    m_deviceMIMOEnginesUIDSequence++;
    m_deviceEngineReferences.push_back(DeviceEngineReference{2, nullptr, nullptr, m_deviceMIMOEngines.back()});
    return m_deviceMIMOEngines.back();
}

void DSPEngine::removeLastDeviceMIMOEngine()
{
    if (m_deviceMIMOEngines.size() > 0)
    {
        DSPDeviceMIMOEngine *lastDeviceEngine = m_deviceMIMOEngines.back();
        delete lastDeviceEngine;
        m_deviceMIMOEngines.pop_back();

        for (int i = 0; i < m_deviceEngineReferences.size(); i++)
        {
            if (m_deviceEngineReferences[i].m_deviceMIMOEngine == lastDeviceEngine)
            {
                m_deviceEngineReferences.removeAt(i);
                break;
            }
        }
    }
}

void DSPEngine::removeDeviceEngineAt(int deviceIndex)
{
    if (deviceIndex >= m_deviceEngineReferences.size()) {
        return;
    }

    if (m_deviceEngineReferences[deviceIndex].m_deviceEngineType == 0) // source
    {
        DSPDeviceSourceEngine *deviceEngine = m_deviceEngineReferences[deviceIndex].m_deviceSourceEngine;
        delete deviceEngine;
        m_deviceSourceEngines.removeAll(deviceEngine);
    }
    else if (m_deviceEngineReferences[deviceIndex].m_deviceEngineType == 1) // sink
    {
        DSPDeviceSinkEngine *deviceEngine = m_deviceEngineReferences[deviceIndex].m_deviceSinkEngine;
        delete deviceEngine;
        m_deviceSinkEngines.removeAll(deviceEngine);
    }
    else if (m_deviceEngineReferences[deviceIndex].m_deviceEngineType == 2) // MIMO
    {
        DSPDeviceMIMOEngine *deviceEngine = m_deviceEngineReferences[deviceIndex].m_deviceMIMOEngine;
        delete deviceEngine;
        m_deviceMIMOEngines.removeAll(deviceEngine);
    }

    m_deviceEngineReferences.removeAt(deviceIndex);
}

void DSPEngine::createFFTFactory(const QString& fftWisdomFileName)
{
    m_fftFactory = new FFTFactory(fftWisdomFileName);
}

void DSPEngine::preAllocateFFTs()
{
    m_fftFactory->preallocate(7, 10, 1, 0); // pre-acllocate forward FFT only 1 per size from 128 to 1024
}
