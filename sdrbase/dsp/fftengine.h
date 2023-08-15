///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_FFTENGINE_H
#define INCLUDE_FFTENGINE_H

#include <QString>

#include "dsp/dsptypes.h"
#include "util/profiler.h"
#include "export.h"

class SDRBASE_API FFTEngine {
public:
	virtual ~FFTEngine();

	virtual void configure(int n, bool inverse) = 0;
	virtual void transform() = 0;

	virtual Complex* in() = 0;
	virtual Complex* out() = 0;

    virtual void setReuse(bool reuse) = 0;

	static FFTEngine* create(const QString& fftWisdomFileName, const QString& preferredEngine="");

    virtual bool isAvailable() { return true; } // Is this FFT engine available to be used?
    virtual QString getName() const = 0;        // Get the name of this FFT engine

    static QStringList getAllNames();           // Get names of all available FFT engines

private:
    static QStringList m_allAvailableEngines;

};

#endif // INCLUDE_FFTENGINE_H
