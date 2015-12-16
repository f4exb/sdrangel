///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_DSP_PHASEDISCRI_H_
#define INCLUDE_DSP_PHASEDISCRI_H_

/**
 * Standard discriminator using atan2. On modern processors this is as efficient as the non atan2 one.
 * This is better for high fidelity.
 */
Real phaseDiscriminator(const Complex& sample)
{
	Complex d(std::conj(m_m1Sample) * sample);
	m_m1Sample = sample;
	return (std::atan2(d.imag(), d.real()) / M_PI_2) * m_fmScaling;
}

/**
 * Alternative without atan at the expense of a slight distorsion on very wideband signals
 * http://www.embedded.com/design/configurable-systems/4212086/DSP-Tricks--Frequency-demodulation-algorithms-
 * in addition it needs scaling by instantaneous magnitude squared and volume (0..10) adjustment factor
 */
Real phaseDiscriminator2(const Complex& sample)
{
	Real ip = sample.real() - m_m2Sample.real();
	Real qp = sample.imag() - m_m2Sample.imag();
	Real h1 = m_m1Sample.real() * qp;
	Real h2 = m_m1Sample.imag() * ip;

	m_m2Sample = m_m1Sample;
	m_m1Sample = sample;

	return ((h1 - h2) / M_PI) * m_fmScaling;
}

#endif /* INCLUDE_DSP_PHASEDISCRI_H_ */
