// Selected code from https://github.com/DTolm/VkFFT/blob/master/benchmark_scripts/vkFFT_scripts/src/utils_VkFFT.cpp
// Formatting kept the same as source, to allow easier future merges

#include "vkfftutils.h"

#if(VKFFT_BACKEND==0)
#include "vulkan/vulkan.h"
#include "glslang_c_interface.h"
#endif


#if(VKFFT_BACKEND==0)

VkResult CreateDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	//pointer to the function, as it is not part of the core. Function creates debugging messenger
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkGPU->instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != NULL) {
		return func(vkGPU->instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}
void DestroyDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkAllocationCallbacks* pAllocator) {
	//pointer to the function, as it is not part of the core. Function destroys debugging messenger
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkGPU->instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != NULL) {
		func(vkGPU->instance, vkGPU->debugMessenger, pAllocator);
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	printf("validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

VkResult setupDebugMessenger(VkGPU* vkGPU) {
	//function that sets up the debugging messenger
	if (vkGPU->enableValidationLayers == 0) return VK_SUCCESS;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;

	if (CreateDebugUtilsMessengerEXT(vkGPU, &createInfo, NULL, &vkGPU->debugMessenger) != VK_SUCCESS) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	return VK_SUCCESS;
}

VkResult checkValidationLayerSupport() {
	//check if validation layers are supported when an instance is created
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);

	VkLayerProperties* availableLayers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layerCount);
	if (!availableLayers) return VK_INCOMPLETE;
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
	if (availableLayers) {
		for (uint64_t i = 0; i < layerCount; i++) {
			if (strcmp("VK_LAYER_KHRONOS_validation", availableLayers[i].layerName) == 0) {
				free(availableLayers);
				return VK_SUCCESS;
			}
		}
		free(availableLayers);
	}
	else {
		return VK_INCOMPLETE;
	}
	return VK_ERROR_LAYER_NOT_PRESENT;
}

std::vector<const char*> getRequiredExtensions(VkGPU* vkGPU, uint64_t sample_id) {
	std::vector<const char*> extensions;

	if (vkGPU->enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	switch (sample_id) {
#if (VK_API_VERSION>10)
	case 2: case 102:
		extensions.push_back("VK_KHR_get_physical_device_properties2");
		break;
#endif
	default:
		break;
	}


	return extensions;
}

VkResult createInstance(VkGPU* vkGPU, uint64_t sample_id) {
	//create instance - a connection between the application and the Vulkan library
	VkResult res = VK_SUCCESS;
	//check if validation layers are supported
	if (vkGPU->enableValidationLayers == 1) {
		res = checkValidationLayerSupport();
		if (res != VK_SUCCESS) return res;
	}

	VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.pApplicationName = "VkFFT";
	applicationInfo.applicationVersion = (uint32_t)VkFFTGetVersion();
	applicationInfo.pEngineName = "VkFFT";
	applicationInfo.engineVersion = 1;
#if (VK_API_VERSION>=12)
	applicationInfo.apiVersion = VK_API_VERSION_1_2;
#elif (VK_API_VERSION==11)
	applicationInfo.apiVersion = VK_API_VERSION_1_1;
#else
	applicationInfo.apiVersion = VK_API_VERSION_1_0;
#endif

	VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.flags = 0;
	createInfo.pApplicationInfo = &applicationInfo;

	auto extensions = getRequiredExtensions(vkGPU, sample_id);
	createInfo.enabledExtensionCount = (uint32_t)(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

	if (vkGPU->enableValidationLayers) {
		//query for the validation layer support in the instance
		createInfo.enabledLayerCount = 1;
		const char* validationLayers = "VK_LAYER_KHRONOS_validation";
		createInfo.ppEnabledLayerNames = &validationLayers;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	res = vkCreateInstance(&createInfo, NULL, &vkGPU->instance);
	if (res != VK_SUCCESS) {
        return res;

    }
	return res;
}

VkResult findPhysicalDevice(VkGPU* vkGPU) {
	//check if there are GPUs that support Vulkan and select one
	VkResult res = VK_SUCCESS;
	uint32_t deviceCount;
	res = vkEnumeratePhysicalDevices(vkGPU->instance, &deviceCount, NULL);
	if (res != VK_SUCCESS) return res;
	if (deviceCount == 0) {
		return VK_ERROR_DEVICE_LOST;
	}

	VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * deviceCount);
	if (!devices) return VK_INCOMPLETE;
	res = vkEnumeratePhysicalDevices(vkGPU->instance, &deviceCount, devices);
	if (res != VK_SUCCESS) return res;
	if (devices) {
		vkGPU->physicalDevice = devices[vkGPU->device_id];
		free(devices);
		return VK_SUCCESS;
	}
	else
		return VK_INCOMPLETE;
}
VkResult getComputeQueueFamilyIndex(VkGPU* vkGPU) {
	//find a queue family for a selected GPU, select the first available for use
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(vkGPU->physicalDevice, &queueFamilyCount, NULL);

	VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
	if (!queueFamilies) return VK_INCOMPLETE;
	if (queueFamilies) {
		vkGetPhysicalDeviceQueueFamilyProperties(vkGPU->physicalDevice, &queueFamilyCount, queueFamilies);
		uint64_t i = 0;
		for (; i < queueFamilyCount; i++) {
			VkQueueFamilyProperties props = queueFamilies[i];

			if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
				break;
			}
		}
		free(queueFamilies);
		if (i == queueFamilyCount) {
			return VK_ERROR_INITIALIZATION_FAILED;
		}
		vkGPU->queueFamilyIndex = i;
		return VK_SUCCESS;
	}
	else
		return VK_INCOMPLETE;
}

VkResult createDevice(VkGPU* vkGPU, uint64_t sample_id) {
	//create logical device representation
	VkResult res = VK_SUCCESS;
	VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	res = getComputeQueueFamilyIndex(vkGPU);
	if (res != VK_SUCCESS) return res;
	queueCreateInfo.queueFamilyIndex = (uint32_t)vkGPU->queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	float queuePriorities = 1.0;
	queueCreateInfo.pQueuePriorities = &queuePriorities;
	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	VkPhysicalDeviceFeatures deviceFeatures = {};
	switch (sample_id) {
	case 1: case 12: case 17: case 18: case 101: case 201: case 1001: {
		deviceFeatures.shaderFloat64 = true;
		deviceCreateInfo.enabledExtensionCount = (uint32_t)vkGPU->enabledDeviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = vkGPU->enabledDeviceExtensions.data();
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		res = vkCreateDevice(vkGPU->physicalDevice, &deviceCreateInfo, NULL, &vkGPU->device);
		if (res != VK_SUCCESS) return res;
		vkGetDeviceQueue(vkGPU->device, (uint32_t)vkGPU->queueFamilyIndex, 0, &vkGPU->queue);
		break;
	}
#if (VK_API_VERSION>10)
	case 2: case 102: {
		VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
		VkPhysicalDevice16BitStorageFeatures shaderFloat16 = {};
		shaderFloat16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
		shaderFloat16.storageBuffer16BitAccess = true;
		/*VkPhysicalDeviceShaderFloat16Int8Features shaderFloat16 = {};
		shaderFloat16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
		shaderFloat16.shaderFloat16 = true;
		shaderFloat16.shaderInt8 = true;*/
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &shaderFloat16;
		deviceFeatures2.features = deviceFeatures;
		vkGetPhysicalDeviceFeatures2(vkGPU->physicalDevice, &deviceFeatures2);
		deviceCreateInfo.pNext = &deviceFeatures2;
		vkGPU->enabledDeviceExtensions.push_back("VK_KHR_16bit_storage");
		deviceCreateInfo.enabledExtensionCount = (uint32_t)vkGPU->enabledDeviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = vkGPU->enabledDeviceExtensions.data();
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pEnabledFeatures = NULL;
		res = vkCreateDevice(vkGPU->physicalDevice, &deviceCreateInfo, NULL, &vkGPU->device);
		if (res != VK_SUCCESS) return res;
		vkGetDeviceQueue(vkGPU->device, (uint32_t)vkGPU->queueFamilyIndex, 0, &vkGPU->queue);
		break;
	}
#endif
	default: {
		deviceCreateInfo.enabledExtensionCount = (uint32_t)vkGPU->enabledDeviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = vkGPU->enabledDeviceExtensions.data();
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pEnabledFeatures = NULL;
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		res = vkCreateDevice(vkGPU->physicalDevice, &deviceCreateInfo, NULL, &vkGPU->device);
		if (res != VK_SUCCESS) return res;
		vkGetDeviceQueue(vkGPU->device, (uint32_t)vkGPU->queueFamilyIndex, 0, &vkGPU->queue);
		break;
	}
	}
	return res;
}
VkResult createFence(VkGPU* vkGPU) {
	//create fence for synchronization
	VkResult res = VK_SUCCESS;
	VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceCreateInfo.flags = 0;
	res = vkCreateFence(vkGPU->device, &fenceCreateInfo, NULL, &vkGPU->fence);
	return res;
}
VkResult createCommandPool(VkGPU* vkGPU) {
	//create a place, command buffer memory is allocated from
	VkResult res = VK_SUCCESS;
	VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = (uint32_t)vkGPU->queueFamilyIndex;
	res = vkCreateCommandPool(vkGPU->device, &commandPoolCreateInfo, NULL, &vkGPU->commandPool);
	return res;
}

VkFFTResult findMemoryType(VkGPU* vkGPU, uint64_t memoryTypeBits, uint64_t memorySize, VkMemoryPropertyFlags properties, uint32_t* memoryTypeIndex) {
	VkPhysicalDeviceMemoryProperties memoryProperties = { 0 };

	vkGetPhysicalDeviceMemoryProperties(vkGPU->physicalDevice, &memoryProperties);

	for (uint64_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
		if ((memoryTypeBits & ((uint64_t)1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) && (memoryProperties.memoryHeaps[memoryProperties.memoryTypes[i].heapIndex].size >= memorySize))
		{
			memoryTypeIndex[0] = (uint32_t)i;
			return VKFFT_SUCCESS;
		}
	}
	return VKFFT_ERROR_FAILED_TO_FIND_MEMORY;
}

VkFFTResult allocateBuffer(VkGPU* vkGPU, VkBuffer* buffer, VkDeviceMemory* deviceMemory, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, uint64_t size) {
	//allocate the buffer used by the GPU with specified properties
	VkFFTResult resFFT = VKFFT_SUCCESS;
	VkResult res = VK_SUCCESS;
	uint32_t queueFamilyIndices;
	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 1;
	bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndices;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	res = vkCreateBuffer(vkGPU->device, &bufferCreateInfo, NULL, buffer);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_CREATE_BUFFER;
	VkMemoryRequirements memoryRequirements = { 0 };
	vkGetBufferMemoryRequirements(vkGPU->device, buffer[0], &memoryRequirements);
	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	resFFT = findMemoryType(vkGPU, memoryRequirements.memoryTypeBits, memoryRequirements.size, propertyFlags, &memoryAllocateInfo.memoryTypeIndex);
	if (resFFT != VKFFT_SUCCESS) return resFFT;
	res = vkAllocateMemory(vkGPU->device, &memoryAllocateInfo, NULL, deviceMemory);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_ALLOCATE_MEMORY;
	res = vkBindBufferMemory(vkGPU->device, buffer[0], deviceMemory[0], 0);
	if (res != VK_SUCCESS) return VKFFT_ERROR_FAILED_TO_BIND_BUFFER_MEMORY;
	return resFFT;
}
#endif

