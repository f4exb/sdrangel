///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_SDRBASE_DSP_INTERPOLATORSIFNORMALIZED_H_
#define INCLUDE_SDRBASE_DSP_INTERPOLATORSIFNORMALIZED_H_

#include <stdint.h>
#include "dsp/dsptypes.h"
#include "export.h"

#ifdef USE_SSE4_1
#include "dsp/inthalfbandfiltereo1.h"
#else
#include "dsp/inthalfbandfilterdb.h"
#endif

#define INTERPOLATORS_HB_FILTER_ORDER_FIRST  64
#define INTERPOLATORS_HB_FILTER_ORDER_SECOND 32
#define INTERPOLATORS_HB_FILTER_ORDER_NEXT   16

class SDRBASE_API InterpolatorsIFNormalized
{
public:
    void interpolate1(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);

    void interpolate2_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate2_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate2_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);

    void interpolate4_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate4_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate4_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);

    void interpolate8_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
    void interpolate8_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
    void interpolate8_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);

	void interpolate16_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate16_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate16_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);

	void interpolate32_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate32_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate32_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);

	void interpolate64_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate64_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);
	void interpolate64_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm = 1.0f, bool invertIQ = false);

private:
#ifdef USE_SSE4_1
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_FIRST> m_interpolator2;  // 1st stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_SECOND> m_interpolator4;  // 2nd stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator8;  // 3rd stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator16; // 4th stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator32; // 5th stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator64; // 6th stages
#else
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_FIRST> m_interpolator2;  // 1st stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_SECOND> m_interpolator4;  // 2nd stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator8;  // 3rd stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator16; // 4th stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator32; // 5th stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator64; // 6th stages
#endif
};

#endif /* INCLUDE_SDRBASE_DSP_INTERPOLATORSIFNORMALIZED_H_ */
