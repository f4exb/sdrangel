///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "dsp/vkfftengine.h"

QMutex vkFFTEngine::m_globalPlanMutex;

vkFFTEngine::vkFFTEngine() :
	m_currentPlan(nullptr),
    m_reuse(true)
{
    vkGPU = new VkGPU();
    memset(vkGPU, sizeof(VkGPU), 0);
    vkGPU->device_id = 0; // Could be set in GUI to support multiple GPUs
}

vkFFTEngine::~vkFFTEngine()
{
}

bool vkFFTEngine::isAvailable()
{
    return vkGPU != nullptr;
}

void vkFFTEngine::configure(int n, bool inverse)
{
    if (m_reuse)
    {
        for (const auto plan : m_plans)
        {
            if ((plan->n == n) && (plan->m_inverse == inverse))
            {
                m_currentPlan = plan;
                return;
            }
        }
    }

    m_currentPlan = gpuAllocatePlan();
	m_currentPlan->n = n;

	QElapsedTimer t;
	t.start();
    m_globalPlanMutex.lock();

    VkFFTResult resFFT;

    // Allocate and intialise plan
    m_currentPlan->m_configuration = new VkFFTConfiguration();
    memset(m_currentPlan->m_configuration, sizeof(VkFFTConfiguration), 0);
    m_currentPlan->m_app = new VkFFTApplication();
    memset(m_currentPlan->m_app, sizeof(VkFFTApplication), 0);
    m_currentPlan->m_configuration->FFTdim = 1;
    m_currentPlan->m_configuration->size[0] = n;
    m_currentPlan->m_configuration->size[1] = 1;
    m_currentPlan->m_configuration->size[2] = 1;
    m_currentPlan->m_configuration->numberBatches = 1;
    m_currentPlan->m_configuration->performR2C = false;
    m_currentPlan->m_configuration->performDCT = false;
    m_currentPlan->m_configuration->doublePrecision = false;
    m_currentPlan->m_configuration->halfPrecision = false;
    m_currentPlan->m_bufferSize = sizeof(float) * 2 * n;
    m_currentPlan->m_inverse = inverse;

    m_currentPlan->m_configuration->device = &vkGPU->device;
#if(VKFFT_BACKEND==0)
	m_currentPlan->m_configuration->queue = &vkGPU->queue;
	m_currentPlan->m_configuration->fence = &vkGPU->fence;
	m_currentPlan->m_configuration->commandPool = &vkGPU->commandPool;
	m_currentPlan->m_configuration->physicalDevice = &vkGPU->physicalDevice;
	m_currentPlan->m_configuration->isCompilerInitialized = true;
#endif
    m_currentPlan->m_configuration->bufferSize = &m_currentPlan->m_bufferSize;

    resFFT = gpuAllocateBuffers();
    if (resFFT != VKFFT_SUCCESS)
    {
        qDebug() << "vkFFTEngine::configure: gpuAllocateBuffers failed:" << getVkFFTErrorString(resFFT);
        m_globalPlanMutex.unlock();
        delete m_currentPlan;
        m_currentPlan = nullptr;
        return;
    }

	m_currentPlan->m_configuration->bufferSize = &m_currentPlan->m_bufferSize;

    resFFT = initializeVkFFT(m_currentPlan->m_app, *m_currentPlan->m_configuration);
    if (resFFT != VKFFT_SUCCESS)
    {
        qDebug() << "vkFFTEngine::configure: initializeVkFFT failed:" << getVkFFTErrorString(resFFT);
        m_globalPlanMutex.unlock();
        delete m_currentPlan;
        m_currentPlan = nullptr;
        return;
    }

    resFFT = gpuConfigure();
    if (resFFT != VKFFT_SUCCESS)
    {
        qDebug() << "vkFFTEngine::configure: gpuConfigure failed:" << getVkFFTErrorString(resFFT);
        m_globalPlanMutex.unlock();
        delete m_currentPlan;
        m_currentPlan = nullptr;
        return;
    }

    m_globalPlanMutex.unlock();

    qDebug("FFT: creating vkFFT plan (n=%d,%s) took %lld ms", n, inverse ? "inverse" : "forward", t.elapsed());
	m_plans.push_back(m_currentPlan);
}

Complex* vkFFTEngine::in()
{
	if (m_currentPlan != nullptr) {
		return m_currentPlan->m_in;
	} else {
        return nullptr;
    }
}

Complex* vkFFTEngine::out()
{
	if (m_currentPlan != nullptr) {
		return m_currentPlan->m_out;
	} else {
        return nullptr;
    }
}

void vkFFTEngine::freeAll()
{
	for (auto plan : m_plans)
    {
        gpuDeallocatePlan(plan);
		delete plan;
	}
	m_plans.clear();
}

vkFFTEngine::Plan::Plan() :
    m_configuration(nullptr),
    m_app(nullptr)
{
}

vkFFTEngine::Plan::~Plan()
{
    if (m_app) {
        deleteVkFFT(m_app);
    }
    delete m_configuration;
}
