///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
// Selected code from https://github.com/DTolm/VkFFT/blob/master/benchmark_scripts/vkFFT_scripts/include/utils_VkFFT.h

#ifndef VKFFT_UTILS_H
#define VKFFT_UTILS_H

#include <vector>

#include <vkFFT.h>

typedef struct {
#if(VKFFT_BACKEND==0)
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties physicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	VkDevice device;
	VkDebugUtilsMessengerEXT debugMessenger;
	uint64_t queueFamilyIndex;
	VkQueue queue;
	VkCommandPool commandPool;
	VkFence fence;
	std::vector<const char*> enabledDeviceExtensions;
	uint64_t enableValidationLayers;
#elif(VKFFT_BACKEND==1)
	CUdevice device;
	CUcontext context;
#endif
	uint64_t device_id;
} VkGPU;

#if(VKFFT_BACKEND==0)
VkResult CreateDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void DestroyDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkAllocationCallbacks* pAllocator);
VkResult setupDebugMessenger(VkGPU* vkGPU);
VkResult checkValidationLayerSupport();
std::vector<const char*> getRequiredExtensions(VkGPU* vkGPU, uint64_t sample_id);
VkResult createInstance(VkGPU* vkGPU, uint64_t sample_id);
VkResult findPhysicalDevice(VkGPU* vkGPU);
VkResult getComputeQueueFamilyIndex(VkGPU* vkGPU);
VkResult createDevice(VkGPU* vkGPU, uint64_t sample_id);
VkResult createFence(VkGPU* vkGPU);
VkResult createCommandPool(VkGPU* vkGPU);
VkFFTResult findMemoryType(VkGPU* vkGPU, uint64_t memoryTypeBits, uint64_t memorySize, VkMemoryPropertyFlags properties, uint32_t* memoryTypeIndex);
VkFFTResult allocateBuffer(VkGPU* vkGPU, VkBuffer* buffer, VkDeviceMemory* deviceMemory, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, uint64_t size);
#endif

#endif // VKFFT_UTILS_H
