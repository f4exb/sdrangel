///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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
    auto it = m_deviceSourceEngines.begin();

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
    auto *deviceSourceEngine = new DSPDeviceSourceEngine(m_deviceSourceEnginesUIDSequence);
    auto *deviceThread = new QThread();
    m_deviceSourceEnginesUIDSequence++;
    m_deviceSourceEngines.push_back(deviceSourceEngine);
    m_deviceEngineReferences.push_back(DeviceEngineReference{0, m_deviceSourceEngines.back(), nullptr, nullptr, deviceThread});
    deviceSourceEngine->moveToThread(deviceThread);

    QObject::connect(
        deviceThread,
        &QThread::finished,
        deviceThread,
        &QThread::deleteLater
    );

    deviceThread->start();

    return deviceSourceEngine;
}

void DSPEngine::removeLastDeviceSourceEngine()
{
    if (!m_deviceSourceEngines.empty())
    {
        const DSPDeviceSourceEngine *lastDeviceEngine = m_deviceSourceEngines.back();
        m_deviceSourceEngines.pop_back();

        for (int i = 0; i < m_deviceEngineReferences.size(); i++)
        {
            if (m_deviceEngineReferences[i].m_deviceSourceEngine == lastDeviceEngine)
            {
                QThread* deviceThread = m_deviceEngineReferences[i].m_thread;
                deviceThread->exit();
                deviceThread->wait();
                m_deviceEngineReferences.removeAt(i);
                break;
            }
        }
    }
}

DSPDeviceSinkEngine *DSPEngine::addDeviceSinkEngine()
{
    auto *deviceSinkEngine = new DSPDeviceSinkEngine(m_deviceSinkEnginesUIDSequence);
    auto *deviceThread = new QThread();
    m_deviceSinkEnginesUIDSequence++;
    m_deviceSinkEngines.push_back(deviceSinkEngine);
    m_deviceEngineReferences.push_back(DeviceEngineReference{1, nullptr, m_deviceSinkEngines.back(), nullptr, deviceThread});
    deviceSinkEngine->moveToThread(deviceThread);

    QObject::connect(
        deviceThread,
        &QThread::finished,
        deviceThread,
        &QThread::deleteLater
    );

    deviceThread->start();

    return deviceSinkEngine;
}

void DSPEngine::removeLastDeviceSinkEngine()
{
    if (!m_deviceSinkEngines.empty())
    {
        const DSPDeviceSinkEngine *lastDeviceEngine = m_deviceSinkEngines.back();
        m_deviceSinkEngines.pop_back();

        for (int i = 0; i < m_deviceEngineReferences.size(); i++)
        {
            if (m_deviceEngineReferences[i].m_deviceSinkEngine == lastDeviceEngine)
            {
                QThread* deviceThread = m_deviceEngineReferences[i].m_thread;
                deviceThread->exit();
                deviceThread->wait();
                m_deviceEngineReferences.removeAt(i);
                break;
            }
        }
    }
}

DSPDeviceMIMOEngine *DSPEngine::addDeviceMIMOEngine()
{
    auto *deviceMIMOEngine = new DSPDeviceMIMOEngine(m_deviceMIMOEnginesUIDSequence);
    auto *deviceThread = new QThread();
    m_deviceMIMOEnginesUIDSequence++;
    m_deviceMIMOEngines.push_back(deviceMIMOEngine);
    m_deviceEngineReferences.push_back(DeviceEngineReference{2, nullptr, nullptr, m_deviceMIMOEngines.back(), deviceThread});
    deviceMIMOEngine->moveToThread(deviceThread);

    QObject::connect(
        deviceThread,
        &QThread::finished,
        deviceThread,
        &QThread::deleteLater
    );

    deviceThread->start();

    return deviceMIMOEngine;
}

void DSPEngine::removeLastDeviceMIMOEngine()
{
    if (!m_deviceMIMOEngines.empty())
    {
        const DSPDeviceMIMOEngine *lastDeviceEngine = m_deviceMIMOEngines.back();
        m_deviceMIMOEngines.pop_back();

        for (int i = 0; i < m_deviceEngineReferences.size(); i++)
        {
            if (m_deviceEngineReferences[i].m_deviceMIMOEngine == lastDeviceEngine)
            {
                QThread* deviceThread = m_deviceEngineReferences[i].m_thread;
                deviceThread->exit();
                deviceThread->wait();
                m_deviceEngineReferences.removeAt(i);
                break;
            }
        }
    }
}

QThread * DSPEngine::removeDeviceEngineAt(int deviceIndex)
{
    if (deviceIndex >= m_deviceEngineReferences.size()) {
        return nullptr;
    }

    QThread *deviceThread = nullptr;

    if (m_deviceEngineReferences[deviceIndex].m_deviceEngineType == 0) // source
    {
        DSPDeviceSourceEngine *deviceEngine = m_deviceEngineReferences[deviceIndex].m_deviceSourceEngine;
        deviceThread = m_deviceEngineReferences[deviceIndex].m_thread;
        deviceThread->exit();
        m_deviceSourceEngines.removeAll(deviceEngine);
    }
    else if (m_deviceEngineReferences[deviceIndex].m_deviceEngineType == 1) // sink
    {
        DSPDeviceSinkEngine *deviceEngine = m_deviceEngineReferences[deviceIndex].m_deviceSinkEngine;
        deviceThread = m_deviceEngineReferences[deviceIndex].m_thread;
        deviceThread->exit();
        m_deviceSinkEngines.removeAll(deviceEngine);
    }
    else if (m_deviceEngineReferences[deviceIndex].m_deviceEngineType == 2) // MIMO
    {
        DSPDeviceMIMOEngine *deviceEngine = m_deviceEngineReferences[deviceIndex].m_deviceMIMOEngine;
        deviceThread = m_deviceEngineReferences[deviceIndex].m_thread;
        deviceThread->exit();
        m_deviceMIMOEngines.removeAll(deviceEngine);
    }

    m_deviceEngineReferences.removeAt(deviceIndex);

    return deviceThread;
}

void DSPEngine::createFFTFactory(const QString& fftWisdomFileName)
{
    m_fftFactory = new FFTFactory(fftWisdomFileName);
}

void DSPEngine::preAllocateFFTs()
{
    m_fftFactory->preallocate(7, 10, 1, 0); // pre-acllocate forward FFT only 1 per size from 128 to 1024
}
