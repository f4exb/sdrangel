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

#ifndef INCLUDE_VKFFTENGINE_H
#define INCLUDE_VKFFTENGINE_H

#include <QMutex>

#include <vkFFT.h>

#include "dsp/fftengine.h"
#include "dsp/vkfftutils.h"
#include "export.h"

class SDRBASE_API vkFFTEngine : public FFTEngine {
public:
	vkFFTEngine();
	virtual ~vkFFTEngine();

	virtual void configure(int n, bool inverse);

	virtual Complex* in();
	virtual Complex* out();

    virtual void setReuse(bool reuse) { m_reuse = reuse; }
    bool isAvailable() override;

protected:
	static QMutex m_globalPlanMutex;

	struct Plan {
        Plan();
        virtual ~Plan();
		int n;
        uint64_t m_bufferSize;
        bool m_inverse;
		VkFFTConfiguration* m_configuration;
        VkFFTApplication* m_app;
        Complex* m_in;              // CPU memory
        Complex* m_out;
	};
	QList<Plan *> m_plans;
	Plan* m_currentPlan;
    bool m_reuse;

    VkGPU *vkGPU;

    virtual VkFFTResult gpuInit() = 0;
    virtual VkFFTResult gpuAllocateBuffers() = 0;
    virtual VkFFTResult gpuConfigure() = 0;
    virtual Plan *gpuAllocatePlan() = 0;
    virtual void gpuDeallocatePlan(Plan *plan) = 0;

	void freeAll();
};

#endif // INCLUDE_VKFFTENGINE_H
