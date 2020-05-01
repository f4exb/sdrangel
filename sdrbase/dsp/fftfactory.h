///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef _SDRBASE_FFTWFACTORY_H
#define _SDRBASE_FFTWFACTORY_H

#include <map>
#include <vector>

#include <QMutex>
#include <QString>

#include "export.h"
#include "dsp/dsptypes.h"
#include "fftengine.h"

class SDRBASE_API FFTFactory {
public:
	FFTFactory(const QString& fftwWisdomFileName);
	~FFTFactory();

    void preallocate(unsigned int minLog2Size, unsigned int maxLog2Size, unsigned int numberFFT, unsigned int numberInvFFT);
    unsigned int getEngine(unsigned int fftSize, bool inverse, FFTEngine **engine); //!< returns an engine sequence
    void releaseEngine(unsigned int fftSize, bool inverse, unsigned int engineSequence);

private:
    struct AllocatedEngine
    {
        FFTEngine *m_engine;
        bool m_inUse;

        AllocatedEngine() :
            m_engine(nullptr),
            m_inUse(false)
        {}
    };

    QString m_fftwWisdomFileName;
    std::map<unsigned int, std::vector<AllocatedEngine>> m_fftEngineBySize;
    std::map<unsigned int, std::vector<AllocatedEngine>> m_invFFTEngineBySize;
    QMutex m_mutex;
};

#endif // _SDRBASE_FFTWFACTORY_H
