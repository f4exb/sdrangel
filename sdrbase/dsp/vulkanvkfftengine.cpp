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

#include "glslang_c_interface.h"

#include "dsp/vulkanvkfftengine.h"

class GLSInitialiser {
public:
    GLSInitialiser() {
        glslang_initialize_process();
    };

    ~GLSInitialiser() {
        glslang_finalize_process();
    }
};

static GLSInitialiser glsInitialiser;

VulkanvkFFTEngine::VulkanvkFFTEngine()
{
    VkFFTResult resFFT;
    resFFT = gpuInit();
    if (resFFT != VKFFT_SUCCESS)
    {
        qDebug() << "VulkanvkFFTEngine::VulkanvkFFTEngine: Failed to initialise GPU:" << getVkFFTErrorString(resFFT);
        delete vkGPU;
        vkGPU = nullptr;
    }
}

VulkanvkFFTEngine::~VulkanvkFFTEngine()
{
    if (vkGPU)
    {
        freeAll();
        vkDestroyFence(vkGPU->device, vkGPU->fence, nullptr);
        vkDestroyCommandPool(vkGPU->device, vkGPU->commandPool, nullptr);
        vkDestroyDevice(vkGPU->device, nullptr);
        DestroyDebugUtilsMessengerEXT(vkGPU, nullptr);
        vkDestroyInstance(vkGPU->instance, nullptr);
    }
}

const QString VulkanvkFFTEngine::m_name = "vkFFT (Vulkan)";

QString VulkanvkFFTEngine::getName() const
{
    return m_name;
}

VkFFTResult VulkanvkFFTEngine::gpuInit()
{
    VkResult res = VK_SUCCESS;

    // To enable validation on Windows:
    // set VK_LAYER_PATH=%VULKAN_SDK%\Bin
    // set VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump;VK_LAYER_KHRONOS_validation
    // https://vulkan.lunarg.com/doc/view/1.3.204.1/windows/layer_configuration.html
    // Create vk_layer_settings.txt in working dir
    // Or run vkconfig to do so

    // Create instance - a connection between the application and the Vulkan library
    res = createInstance(vkGPU, 0);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_CREATE_INSTANCE;
    }
    // Set up the debugging messenger
    res = setupDebugMessenger(vkGPU);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_SETUP_DEBUG_MESSENGER;
    }
    // Check if there are GPUs that support Vulkan and select one
    res = findPhysicalDevice(vkGPU);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_FIND_PHYSICAL_DEVICE;
    }
    // Create logical device representation
    res = createDevice(vkGPU, 0);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_CREATE_DEVICE;
    }
    // Create fence for synchronization
    res = createFence(vkGPU);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_CREATE_FENCE;
    }
    // Create a place, command buffer memory is allocated from
    res = createCommandPool(vkGPU);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_CREATE_COMMAND_POOL;
    }
    vkGetPhysicalDeviceProperties(vkGPU->physicalDevice, &vkGPU->physicalDeviceProperties);
    vkGetPhysicalDeviceMemoryProperties(vkGPU->physicalDevice, &vkGPU->physicalDeviceMemoryProperties);

    return VKFFT_SUCCESS;
}

VkFFTResult VulkanvkFFTEngine::gpuAllocateBuffers()
{
    VkFFTResult resFFT;
    VulkanPlan *plan = reinterpret_cast<VulkanPlan *>(m_currentPlan);

    // Allocate GPU memory
    resFFT = allocateBuffer(vkGPU,
                            &plan->m_buffer,
                            &plan->m_bufferDeviceMemory,
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
                            plan->m_bufferSize);
    if (resFFT != VKFFT_SUCCESS) {
        return resFFT;
    }

    // Allocate CPU/GPU memory (Requires m_currentPlan->m_buffer to have been created)
    resFFT = vulkanAllocateIn(plan);
    if (resFFT != VKFFT_SUCCESS) {
        return resFFT;
    }
    resFFT = vulkanAllocateOut(plan);
    if (resFFT != VKFFT_SUCCESS) {
        return resFFT;
    }

    plan->m_configuration->buffer = &plan->m_buffer;

    return VKFFT_SUCCESS;
}

VkFFTResult VulkanvkFFTEngine::gpuConfigure()
{
    VkFFTResult resFFT;
    VulkanPlan *plan = reinterpret_cast<VulkanPlan *>(m_currentPlan);

    // Allocate command buffer with command to perform FFT
    resFFT = vulkanAllocateFFTCommand(plan);
    if (resFFT != VKFFT_SUCCESS) {
        return resFFT;
    }

    return VKFFT_SUCCESS;
}

// Allocate CPU to GPU memory buffer
VkFFTResult VulkanvkFFTEngine::vulkanAllocateIn(VulkanPlan *plan)
{
    VkFFTResult resFFT;
    VkResult res = VK_SUCCESS;
    VkBuffer* buffer = (VkBuffer*)&plan->m_buffer;

    resFFT = allocateBuffer(vkGPU, &plan->m_inBuffer, &plan->m_inMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, m_currentPlan->m_bufferSize);
    if (resFFT != VKFFT_SUCCESS) {
        return resFFT;
    }

    void* data;
    res = vkMapMemory(vkGPU->device, plan->m_inMemory, 0, plan->m_bufferSize, 0, &data);
    if (res != VK_SUCCESS) {
        return VKFFT_ERROR_FAILED_TO_MAP_MEMORY;
    }
    plan->m_in = (Complex*) data;

    return VKFFT_SUCCESS;
}

// Allocate GPU to CPU memory buffer
VkFFTResult VulkanvkFFTEngine::vulkanAllocateOut(VulkanPlan *plan)
{
    VkFFTResult resFFT;
    VkResult res;
    VkBuffer* buffer = (VkBuffer*)&plan->m_buffer;

    resFFT = allocateBuffer(vkGPU, &plan->m_outBuffer, &plan->m_outMemory, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, m_currentPlan->m_bufferSize);
    if (resFFT != VKFFT_SUCCESS) {
        return resFFT;
    }

    void* data;
    res = vkMapMemory(vkGPU->device, plan->m_outMemory, 0, plan->m_bufferSize, 0, &data);
    if (res != VK_SUCCESS) {
        return VKFFT_ERROR_FAILED_TO_MAP_MEMORY;
    }
    plan->m_out = (Complex*) data;

    return VKFFT_SUCCESS;
}

void VulkanvkFFTEngine::vulkanDeallocateIn(VulkanPlan *plan)
{
    vkUnmapMemory(vkGPU->device, plan->m_inMemory);
    vkDestroyBuffer(vkGPU->device, plan->m_inBuffer, nullptr);
    vkFreeMemory(vkGPU->device, plan->m_inMemory, nullptr);
    plan->m_in = nullptr;
}

void VulkanvkFFTEngine::vulkanDeallocateOut(VulkanPlan *plan)
{
    vkUnmapMemory(vkGPU->device, plan->m_outMemory);
    vkDestroyBuffer(vkGPU->device, plan->m_outBuffer, nullptr);
    vkFreeMemory(vkGPU->device, plan->m_outMemory, nullptr);
    plan->m_out = nullptr;
}

VkFFTResult VulkanvkFFTEngine::vulkanAllocateFFTCommand(VulkanPlan *plan)
{
    VkFFTResult resFFT;
    VkResult res = VK_SUCCESS;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    commandBufferAllocateInfo.commandPool = vkGPU->commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    res = vkAllocateCommandBuffers(vkGPU->device, &commandBufferAllocateInfo, &plan->m_commandBuffer);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_ALLOCATE_COMMAND_BUFFERS;
    }
    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    commandBufferBeginInfo.flags = 0;
    res = vkBeginCommandBuffer(plan->m_commandBuffer, &commandBufferBeginInfo);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_BEGIN_COMMAND_BUFFER;
    }

    VkBuffer* buffer = (VkBuffer*)&plan->m_buffer;

    // Copy from CPU to GPU
    VkBufferCopy copyRegionIn = { 0 };
    copyRegionIn.srcOffset = 0;
    copyRegionIn.dstOffset = 0;
    copyRegionIn.size = plan->m_bufferSize;
    vkCmdCopyBuffer(plan->m_commandBuffer, plan->m_inBuffer, buffer[0], 1, &copyRegionIn);

    // Wait for copy to complete
    VkMemoryBarrier memoryBarrierIn = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            0,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
    };
    vkCmdPipelineBarrier(
        plan->m_commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        1,
        &memoryBarrierIn,
        0, 0, 0, 0);

    // Perform FFT
    VkFFTLaunchParams launchParams = {};
    launchParams.commandBuffer = &plan->m_commandBuffer;
    resFFT = VkFFTAppend(plan->m_app, plan->m_inverse, &launchParams);
    if (resFFT != VKFFT_SUCCESS) {
        return resFFT;
    }

    // Wait for FFT to complete
    VkMemoryBarrier memoryBarrierOut = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            0,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_HOST_READ_BIT,
    };
    vkCmdPipelineBarrier(
        plan->m_commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        1,
        &memoryBarrierIn,
        0, 0, 0, 0);

    // Copy from GPU to CPU
    VkBufferCopy copyRegionOut = { 0 };
    copyRegionOut.srcOffset = 0;
    copyRegionOut.dstOffset = 0;
    copyRegionOut.size = plan->m_bufferSize;
    vkCmdCopyBuffer(plan->m_commandBuffer, buffer[0], plan->m_outBuffer, 1, &copyRegionOut);

    res = vkEndCommandBuffer(plan->m_commandBuffer);
    if (res != 0) {
        return VKFFT_ERROR_FAILED_TO_END_COMMAND_BUFFER;
    }
    return VKFFT_SUCCESS;
}

void VulkanvkFFTEngine::transform()
{
    PROFILER_START()

    VkResult res = VK_SUCCESS;
    VulkanPlan *plan = reinterpret_cast<VulkanPlan *>(m_currentPlan);

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &plan->m_commandBuffer;
    res = vkQueueSubmit(vkGPU->queue, 1, &submitInfo, vkGPU->fence);
    if (res != 0) {
        qDebug() << "VulkanvkFFTEngine::transform: Failed to submit to queue";
    }
    res = vkWaitForFences(vkGPU->device, 1, &vkGPU->fence, VK_TRUE, 100000000000);
    if (res != 0) {
        qDebug() << "VulkanvkFFTEngine::transform: Failed to wait for fences";
    }
    res = vkResetFences(vkGPU->device, 1, &vkGPU->fence);
    if (res != 0) {
        qDebug() << "VulkanvkFFTEngine::transform: Failed to reset fences";
    }

    PROFILER_STOP(QString("%1 FFT %2").arg(getName()).arg(m_currentPlan->n))
}

vkFFTEngine::Plan *VulkanvkFFTEngine::gpuAllocatePlan()
{
    return new VulkanPlan();
}

void VulkanvkFFTEngine::gpuDeallocatePlan(Plan *p)
{
    VulkanPlan *plan = reinterpret_cast<VulkanPlan *>(p);

    vulkanDeallocateOut(plan);
    vulkanDeallocateIn(plan);

    vkFreeCommandBuffers(vkGPU->device, vkGPU->commandPool, 1, &plan->m_commandBuffer);
    vkDestroyBuffer(vkGPU->device, plan->m_buffer, nullptr);
    vkFreeMemory(vkGPU->device, plan->m_bufferDeviceMemory, nullptr);
}
