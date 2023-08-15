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

#include <QDebug>

#include "dsp/fftengine.h"
#ifdef USE_KISSFFT
#include "dsp/kissengine.h"
#endif
#ifdef USE_FFTW
#include "dsp/fftwengine.h"
#endif
#ifdef VKFFT_BACKEND
#if VKFFT_BACKEND==0
#include "dsp/vulkanvkfftengine.h"
#elif VKFFT_BACKEND==1
#include "dsp/cudavkfftengine.h"
#endif
#endif

QStringList FFTEngine::m_allAvailableEngines;

FFTEngine::~FFTEngine()
{
}

FFTEngine* FFTEngine::create(const QString& fftWisdomFileName, const QString& preferredEngine)
{
    QStringList allNames = getAllNames();
    QString engine;

    if (allNames.size() == 0)
    {
        // No engines available
	   qCritical("FFTEngine::create: no engine built");
	   return nullptr;
    }
    else if (!preferredEngine.isEmpty() && allNames.contains(preferredEngine))
    {
        // Use the preferred engine
        engine = preferredEngine;
    }
    else
    {
        // Use first available
        engine = allNames[0];
    }

	qDebug("FFTEngine::create: using %s engine", qPrintable(engine));

#ifdef VKFFT_BACKEND
#if VKFFT_BACKEND==0
    if (engine == VulkanvkFFTEngine::m_name) {
	    return new VulkanvkFFTEngine();
    }
#endif
#if VKFFT_BACKEND==1
    if (engine == CUDAvkFFTEngine::m_name) {
	    return new CUDAvkFFTEngine();
    }
#endif
#endif
#ifdef USE_FFTW
	if (engine == FFTWEngine::m_name) {
	    return new FFTWEngine(fftWisdomFileName);
    }
#endif
#ifdef USE_KISSFFT
    if (engine == KissEngine::m_name) {
	    return new KissEngine;
    }
#endif
    return nullptr;
}

QStringList FFTEngine::getAllNames()
{
    if (m_allAvailableEngines.size() == 0)
    {
#ifdef USE_FFTW
        m_allAvailableEngines.append(FFTWEngine::m_name);
#endif
#ifdef USE_KISSFFT
        m_allAvailableEngines.append(KissEngine::m_name);
#endif
#ifdef VKFFT_BACKEND
#if VKFFT_BACKEND==0
        VulkanvkFFTEngine vulkanvkFFT;
        if (vulkanvkFFT.isAvailable()) {
            m_allAvailableEngines.append(vulkanvkFFT.getName());
        }
#elif VKFFT_BACKEND==1
        CUDAvkFFTEngine cudavkFFT;
        if (cudavkFFT.isAvailable()) {
            m_allAvailableEngines.append(cudavkFFT.getName());
        }
#endif
#endif
    }
    return m_allAvailableEngines;
}
