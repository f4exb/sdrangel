///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DSPTYPES_H
#define INCLUDE_DSPTYPES_H

#include <complex>
#include <vector>
#include <QtGlobal>

#ifdef SDR_RX_SAMPLE_24BIT
#define SDR_RX_SAMP_SZ 24 // internal fixed arithmetic sample size
#define SDR_RX_SCALEF 8388608.0f
#define SDR_RX_SCALED 8388608.0
typedef qint32 FixReal;
#else
#define SDR_RX_SAMP_SZ 16 // internal fixed arithmetic sample size
#define SDR_RX_SCALEF 32768.0f
#define SDR_RX_SCALED 32768.0
typedef qint16 FixReal;
#endif

#define SDR_TX_SAMP_SZ 16
#define SDR_TX_SCALEF 32768.0f
#define SDR_TX_SCALED 32768.0

typedef float Real;
typedef std::complex<Real> Complex;

#pragma pack(push, 1)
struct Sample
{
	Sample() : m_real(0), m_imag(0) {}
	Sample(FixReal real) : m_real(real), m_imag(0) {}
	Sample(FixReal real, FixReal imag) : m_real(real), m_imag(imag) {}
	Sample(const Sample& other) : m_real(other.m_real), m_imag(other.m_imag) {}
	inline Sample& operator=(const Sample& other) { m_real = other.m_real; m_imag = other.m_imag; return *this; }

	inline Sample& operator+=(const Sample& other) { m_real += other.m_real; m_imag += other.m_imag; return *this; }
	inline Sample& operator-=(const Sample& other) { m_real -= other.m_real; m_imag -= other.m_imag; return *this; }
	inline Sample& operator/=(const unsigned int& divisor) { m_real /= divisor; m_imag /= divisor; return *this; }

	inline void setReal(FixReal v) { m_real = v; }
	inline void setImag(FixReal v) { m_imag = v; }

	inline FixReal real() const { return m_real; }
	inline FixReal imag() const { return m_imag; }

	FixReal m_real;
	FixReal m_imag;
};

struct AudioSample {
    qint16 l;
    qint16 r;
};
#pragma pack(pop)

typedef std::vector<Sample> SampleVector;
typedef std::vector<AudioSample> AudioVector;

#endif // INCLUDE_DSPTYPES_H
