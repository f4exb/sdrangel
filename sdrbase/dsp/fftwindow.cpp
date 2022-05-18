///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include "dsp/fftwindow.h"

FFTWindow::FFTWindow() :
	m_kaiserAlpha(M_PI) // first sidelobe at < -70dB
{
	m_kaiserI0Alpha = zeroethOrderBessel(m_kaiserAlpha);
}

void FFTWindow::setKaiserAlpha(Real alpha)
{
	m_kaiserAlpha = alpha;
	m_kaiserI0Alpha = zeroethOrderBessel(m_kaiserAlpha);
}

void FFTWindow::setKaiserBeta(Real beta)
{
	m_kaiserAlpha = beta / M_PI;
	m_kaiserI0Alpha = zeroethOrderBessel(m_kaiserAlpha);
}

void FFTWindow::create(Function function, int n)
{
	Real (*wFunc)(Real n, Real i);

	m_window.clear();

	if (function == Kaiser) // Kaiser special case
	{
		for(int i = 0; i < n; i++) {
			m_window.push_back(kaiser(n, i));
		}

		return;
	}

	switch (function) {
		case Flattop:
			wFunc = flatTop;
			break;

		case Bartlett:
			wFunc = bartlett;
			break;

		case BlackmanHarris:
			wFunc = blackmanHarris;
			break;

		case Hamming:
			wFunc = hamming;
			break;

		case Hanning:
			wFunc = hanning;
			break;

        case Blackman:
            wFunc = blackman;
            break;

        case BlackmanHarris7:
            wFunc = blackmanHarris7;
            break;

		case Rectangle:
		default:
			wFunc = rectangle;
			break;
	}

	for(int i = 0; i < n; i++) {
		m_window.push_back(wFunc(n, i));
	}
}

void FFTWindow::apply(const std::vector<Real>& in, std::vector<Real>* out)
{
	for(size_t i = 0; i < m_window.size(); i++) {
		(*out)[i] = in[i] * m_window[i];
    }
}

void FFTWindow::apply(const std::vector<Complex>& in, std::vector<Complex>* out)
{
	for(size_t i = 0; i < m_window.size(); i++) {
		(*out)[i] = in[i] * m_window[i];
    }
}

void FFTWindow::apply(std::vector<Complex>& in)
{
	for(size_t i = 0; i < m_window.size(); i++) {
		in[i] *= m_window[i];
    }
}

void FFTWindow::apply(const Complex* in, Complex* out)
{
	for(size_t i = 0; i < m_window.size(); i++) {
		out[i] = in[i] * m_window[i];
    }
}

void FFTWindow::apply(Complex* in)
{
	for(size_t i = 0; i < m_window.size(); i++) {
		in[i] *= m_window[i];
    }
}

