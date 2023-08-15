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

#include "dsp/cudavkfftengine.h"

CUDAvkFFTEngine::CUDAvkFFTEngine()
{
    VkFFTResult resFFT;
    resFFT = gpuInit();
    if (resFFT != VKFFT_SUCCESS)
    {
        qDebug() << "CUDAvkFFTEngine::CUDAvkFFTEngine: Failed to initialise GPU" << getVkFFTErrorString(resFFT);
        delete vkGPU;
        vkGPU = nullptr;
    }
}

CUDAvkFFTEngine::~CUDAvkFFTEngine()
{
    if (vkGPU)
    {
        freeAll();
        cuCtxDestroy(vkGPU->context);
    }
}

const QString CUDAvkFFTEngine::m_name = "vkFFT (CUDA)";

QString CUDAvkFFTEngine::getName() const
{
    return m_name;
}

VkFFTResult CUDAvkFFTEngine::gpuInit()
{
    CUresult res = CUDA_SUCCESS;
    cudaError_t res2 = cudaSuccess;
    res = cuInit(0);
    if (res != CUDA_SUCCESS) {
        return VKFFT_ERROR_FAILED_TO_INITIALIZE;
    }
    res2 = cudaSetDevice((int)vkGPU->device_id);
    if (res2 != cudaSuccess) {
        return VKFFT_ERROR_FAILED_TO_SET_DEVICE_ID;
    }
    res = cuDeviceGet(&vkGPU->device, (int)vkGPU->device_id);
    if (res != CUDA_SUCCESS) {
        return VKFFT_ERROR_FAILED_TO_GET_DEVICE;
    }
    res = cuDevicePrimaryCtxRetain(&vkGPU->context, (int)vkGPU->device);
    if (res != CUDA_SUCCESS) {
        return VKFFT_ERROR_FAILED_TO_CREATE_CONTEXT;
    }
    return VKFFT_SUCCESS;
}

VkFFTResult CUDAvkFFTEngine::gpuAllocateBuffers()
{
    cudaError_t res;
    CUDAPlan *plan = reinterpret_cast<CUDAPlan *>(m_currentPlan);

    // Allocate DMA accessible pinned memory, which may be faster than malloc'ed memory
    res = cudaHostAlloc(&plan->m_in, sizeof(Complex) * plan->n, cudaHostAllocMapped);
    if (res != cudaSuccess) {
        return VKFFT_ERROR_FAILED_TO_ALLOCATE;
    }
    res = cudaHostAlloc(&plan->m_out, sizeof(Complex) * plan->n, cudaHostAllocMapped);
    if (res != cudaSuccess) {
        return VKFFT_ERROR_FAILED_TO_ALLOCATE;
    }

    // Allocate GPU memory
    res = cudaMalloc((void**)&plan->m_buffer, sizeof(cuFloatComplex) * plan->n * 2);
    if (res != cudaSuccess) {
        return VKFFT_ERROR_FAILED_TO_ALLOCATE;
    }

    plan->m_configuration->buffer = (void**)&plan->m_buffer;

    return VKFFT_SUCCESS;
}

VkFFTResult CUDAvkFFTEngine::gpuConfigure()
{
    return VKFFT_SUCCESS;
}

void CUDAvkFFTEngine::transform()
{
    if (m_currentPlan)
    {
        CUDAPlan *plan = reinterpret_cast<CUDAPlan *>(m_currentPlan);
        cudaError_t res = cudaSuccess;
        void* buffer = ((void**)&plan->m_buffer)[0];

        // Transfer input from CPU to GPU memory
        PROFILER_START()
        res = cudaMemcpy(buffer, plan->m_in, plan->m_bufferSize, cudaMemcpyHostToDevice);
        PROFILER_STOP(QString("%1 TX %2").arg(getName()).arg(m_currentPlan->n))
        if (res != cudaSuccess) {
            qDebug() << "CUDAvkFFTEngine::transform: cudaMemcpy host to device failed";
        }

        // Perform FFT
        PROFILER_RESTART()
        VkFFTLaunchParams launchParams = {};
        VkFFTResult resFFT = VkFFTAppend(plan->m_app, plan->m_inverse ? 1 : -1, &launchParams);
        PROFILER_STOP(QString("%1 FFT %2").arg(getName()).arg(m_currentPlan->n))
        if (resFFT != VKFFT_SUCCESS) {
            qDebug() << "CUDAvkFFTEngine::transform: VkFFTAppend failed:" << getVkFFTErrorString(resFFT);
        }

        // Transfer result from GPU to CPU memory
        PROFILER_RESTART()
        res = cudaMemcpy(plan->m_out, buffer, plan->m_bufferSize, cudaMemcpyDeviceToHost);
        PROFILER_STOP(QString("%1 RX %2").arg(getName()).arg(m_currentPlan->n))
        if (res != cudaSuccess) {
            qDebug() << "CUDAvkFFTEngine::transform: cudaMemcpy device to host failed";
        }
    }

}

vkFFTEngine::Plan *CUDAvkFFTEngine::gpuAllocatePlan()
{
    return new CUDAPlan();
}

void CUDAvkFFTEngine::gpuDeallocatePlan(Plan *p)
{
    CUDAPlan *plan = reinterpret_cast<CUDAPlan *>(p);

    cudaFree(plan->m_in);
    plan->m_in = nullptr;
    cudaFree(plan->m_out);
    plan->m_out = nullptr;
    cudaFree(plan->m_buffer);
}
