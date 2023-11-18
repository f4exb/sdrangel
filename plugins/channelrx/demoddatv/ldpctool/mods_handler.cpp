///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
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
/*
LDPC modulations handler

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#include <cstring>
#include <cmath>
#include <algorithm>
#include "psk.h"
#include "qam.h"
#include "modulation.h"
#include "testbench.h"

using namespace ldpctool;

template <typename TYPE, typename CODE>
constexpr typename TYPE::value_type PhaseShiftKeying<4, TYPE, CODE>::rcp_sqrt_2;
template <typename TYPE, typename CODE>
constexpr TYPE PhaseShiftKeying<8, TYPE, CODE>::rot_acw;
template <typename TYPE, typename CODE>
constexpr TYPE PhaseShiftKeying<8, TYPE, CODE>::rot_cw;
template <typename TYPE, typename CODE>
constexpr typename TYPE::value_type QuadratureAmplitudeModulation<16, TYPE, CODE>::AMP;
template <typename TYPE, typename CODE>
constexpr typename TYPE::value_type QuadratureAmplitudeModulation<64, TYPE, CODE>::AMP;
template <typename TYPE, typename CODE>
constexpr typename TYPE::value_type QuadratureAmplitudeModulation<256, TYPE, CODE>::AMP;
template <typename TYPE, typename CODE>
constexpr typename TYPE::value_type QuadratureAmplitudeModulation<1024, TYPE, CODE>::AMP;

template <int LEN>
ModulationInterface<complex_type, code_type> *create_modulation(char *name)
{
	if (!strcmp(name, "BPSK"))
		return new Modulation<PhaseShiftKeying<2, complex_type, code_type>, LEN>();
	if (!strcmp(name, "QPSK"))
		return new Modulation<PhaseShiftKeying<4, complex_type, code_type>, LEN / 2>();
	if (!strcmp(name, "8PSK"))
		return new Modulation<PhaseShiftKeying<8, complex_type, code_type>, LEN / 3>();
	if (!strcmp(name, "QAM16"))
		return new Modulation<QuadratureAmplitudeModulation<16, complex_type, code_type>, LEN / 4>();
	if (!strcmp(name, "QAM64"))
		return new Modulation<QuadratureAmplitudeModulation<64, complex_type, code_type>, LEN / 6>();
	if (!strcmp(name, "QAM256"))
		return new Modulation<QuadratureAmplitudeModulation<256, complex_type, code_type>, LEN / 8>();
	if (!strcmp(name, "QAM1024"))
		return new Modulation<QuadratureAmplitudeModulation<1024, complex_type, code_type>, LEN / 10>();
	return 0;
}

ModulationInterface<complex_type, code_type> *create_modulation(char *name, int len)
{
	switch (len) {
	case 16200:
		return create_modulation<16200>(name);
	case 32400:
		return create_modulation<32400>(name);
	case 64800:
		return create_modulation<64800>(name);
	}
	return 0;
}

