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

typedef float Real;
typedef std::complex<Real> Complex;

typedef qint16 FixReal;

#pragma pack(push, 1)
struct Sample {
	Sample() {}
	Sample(FixReal real) : m_real(real), m_imag(0) {}
	Sample(FixReal real, FixReal imag) : m_real(real), m_imag(imag) {}
	Sample(const Sample& other) : m_real(other.m_real), m_imag(other.m_imag) {}
	Sample& operator=(const Sample& other) { m_real = other.m_real; m_imag = other.m_imag; return *this; }

	Sample& operator+=(const Sample& other) { m_real += other.m_real; m_imag += other.m_imag; return *this; }
	Sample& operator-=(const Sample& other) { m_real -= other.m_real; m_imag -= other.m_imag; return *this; }

	void setReal(FixReal v) { m_real = v; }
	void setImag(FixReal v) { m_imag = v; }

	FixReal real() const { return m_real; }
	FixReal imag() const { return m_imag; }

	FixReal m_real;
	FixReal m_imag;
};
#pragma pack(pop)

typedef std::vector<Sample> SampleVector;

#endif // INCLUDE_DSPTYPES_H
