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

#ifndef INCLUDE_VULKANVKFFTENGINE_H
#define INCLUDE_VULKANVKFFTENGINE_H

#include "vkfftengine.h"

#include "vulkan/vulkan.h"

class SDRBASE_API VulkanvkFFTEngine : public vkFFTEngine {
public:
	VulkanvkFFTEngine();
	virtual ~VulkanvkFFTEngine();

	void transform() override;
    QString getName() const override;
    static const QString m_name;

protected:

    struct VulkanPlan : Plan {
        VkBuffer m_inBuffer;        // CPU input memory
        VkDeviceMemory m_inMemory;
        VkBuffer m_outBuffer;       // CPU output memory
        VkDeviceMemory m_outMemory;
        VkBuffer m_buffer;          // GPU memory
        VkDeviceMemory m_bufferDeviceMemory;
        VkCommandBuffer m_commandBuffer;
    };

    VkFFTResult gpuInit() override;
    VkFFTResult gpuAllocateBuffers() override;
    VkFFTResult gpuConfigure() override;

    Plan *gpuAllocatePlan() override;
    void gpuDeallocatePlan(Plan *) override;

    VkFFTResult vulkanAllocateOut(VulkanPlan *plan);
    VkFFTResult vulkanAllocateIn(VulkanPlan *plan);
    void vulkanDeallocateOut(VulkanPlan *plan);
    void vulkanDeallocateIn(VulkanPlan *plan);
    VkFFTResult vulkanAllocateFFTCommand(VulkanPlan *plan);

};

#endif // INCLUDE_VULKANVKFFTENGINE_H
