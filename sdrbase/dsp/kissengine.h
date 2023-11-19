///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2016, 2018, 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>  //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                         //
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
#ifndef INCLUDE_KISSENGINE_H
#define INCLUDE_KISSENGINE_H

#include "dsp/fftengine.h"
#include "dsp/kissfft.h"
#include "export.h"

class SDRBASE_API KissEngine : public FFTEngine {
public:
	virtual void configure(int n, bool inverse);
	virtual void transform();

	virtual Complex* in();
	virtual Complex* out();

    virtual void setReuse(bool reuse);
    QString getName() const override;
    static const QString m_name;

protected:
	typedef kissfft<Real, Complex> KissFFT;
	KissFFT m_fft;

	std::vector<Complex> m_in;
	std::vector<Complex> m_out;
};

#endif // INCLUDE_KISSENGINE_H
