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

#ifndef INCLUDE_CUDAVKFFTENGINE_H
#define INCLUDE_CUDAVKFFTENGINE_H

#include "vkfftengine.h"

#include <cuda.h>
#include <cuda_runtime.h>
#include <nvrtc.h>
#include <cuda_runtime_api.h>
#include <cuComplex.h>

class SDRBASE_API CUDAvkFFTEngine : public vkFFTEngine {
public:
	CUDAvkFFTEngine();
	virtual ~CUDAvkFFTEngine();

	void transform() override;
    QString getName() const override;
    static const QString m_name;

protected:

    struct CUDAPlan : Plan {
		cuFloatComplex* m_buffer;    // GPU memory
    };

    VkFFTResult gpuInit() override;
    VkFFTResult gpuAllocateBuffers() override;
    VkFFTResult gpuConfigure() override;
    Plan *gpuAllocatePlan() override;
    void gpuDeallocatePlan(Plan *) override;

};

#endif // INCLUDE_CUDAVKFFTENGINE_H
