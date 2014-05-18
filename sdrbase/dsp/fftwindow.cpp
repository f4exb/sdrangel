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

#include "dsp/fftwindow.h"

void FFTWindow::create(Function function, int n)
{
	Real (*wFunc)(Real n, Real i);

	m_window.clear();

	switch(function) {
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

		case Rectangle:
		default:
			wFunc = rectangle;
			break;
	}

	for(int i = 0; i < n; i++)
		m_window.push_back(wFunc(n, i));
}

void FFTWindow::apply(const std::vector<Real>& in, std::vector<Real>* out)
{
	for(size_t i = 0; i < m_window.size(); i++)
		(*out)[i] = in[i] * m_window[i];
}

void FFTWindow::apply(const std::vector<Complex>& in, std::vector<Complex>* out)
{
	for(size_t i = 0; i < m_window.size(); i++)
		(*out)[i] = in[i] * m_window[i];
}

void FFTWindow::apply(const Complex* in, Complex* out)
{
	for(size_t i = 0; i < m_window.size(); i++)
		out[i] = in[i] * m_window[i];
}
