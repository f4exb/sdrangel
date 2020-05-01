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

#include <QMutexLocker>
#include "fftfactory.h"

FFTFactory::FFTFactory(const QString& fftwWisdomFileName) :
    m_fftwWisdomFileName(fftwWisdomFileName),
    m_mutex(QMutex::Recursive)
{}

FFTFactory::~FFTFactory()
{
    qDebug("FFTFactory::~FFTFactory: deleting FFTs");

    for (auto mIt = m_fftEngineBySize.begin(); mIt != m_fftEngineBySize.end(); ++mIt)
    {
        for (auto eIt = mIt->second.begin(); eIt != mIt->second.end(); ++eIt) {
            delete eIt->m_engine;
        }
    }
}

void FFTFactory::preallocate(
    unsigned int minLog2Size,
    unsigned int maxLog2Size,
    unsigned int numberFFT,
    unsigned int numberInvFFT)
{
    if (minLog2Size <= maxLog2Size)
    {
        for (unsigned int log2Size = minLog2Size; log2Size <= maxLog2Size; log2Size++)
        {
            unsigned int fftSize = 1<<log2Size;
            m_fftEngineBySize.insert(std::pair<unsigned int, std::vector<AllocatedEngine>>(fftSize, std::vector<AllocatedEngine>()));
            m_invFFTEngineBySize.insert(std::pair<unsigned int, std::vector<AllocatedEngine>>(fftSize, std::vector<AllocatedEngine>()));
            std::vector<AllocatedEngine>& fftEngines = m_fftEngineBySize[fftSize];
            std::vector<AllocatedEngine>& invFFTEngines = m_invFFTEngineBySize[fftSize];

            for (unsigned int i = 0; i < numberFFT; i++)
            {
                fftEngines.push_back(AllocatedEngine());
                fftEngines.back().m_engine = FFTEngine::create(m_fftwWisdomFileName);
                fftEngines.back().m_engine->setReuse(false);
                fftEngines.back().m_engine->configure(fftSize, false);
            }

            for (unsigned int i = 0; i < numberInvFFT; i++)
            {
                invFFTEngines.push_back(AllocatedEngine());
                invFFTEngines.back().m_engine = FFTEngine::create(m_fftwWisdomFileName);
                fftEngines.back().m_engine->setReuse(false);
                invFFTEngines.back().m_engine->configure(fftSize, true);
            }
        }
    }
}

unsigned int FFTFactory::getEngine(unsigned int fftSize, bool inverse, FFTEngine **engine)
{
    QMutexLocker mutexLocker(&m_mutex);
    std::map<unsigned int, std::vector<AllocatedEngine>>& enginesBySize = inverse ?
        m_invFFTEngineBySize : m_fftEngineBySize;

    if (enginesBySize.find(fftSize) == enginesBySize.end())
    {
        qDebug("FFTFactory::getEngine: new FFT %s size: %u", (inverse ? "inv" : "fwd"), fftSize);
        enginesBySize.insert(std::pair<unsigned int, std::vector<AllocatedEngine>>(fftSize, std::vector<AllocatedEngine>()));
        std::vector<AllocatedEngine>& engines = enginesBySize[fftSize];
        engines.push_back(AllocatedEngine());
        engines.back().m_inUse = true;
        engines.back().m_engine = FFTEngine::create(m_fftwWisdomFileName);
        engines.back().m_engine->setReuse(false);
        engines.back().m_engine->configure(fftSize, inverse);
        *engine = engines.back().m_engine;
        return 0;
    }
    else
    {
        unsigned int i = 0;

        for (; i < enginesBySize[fftSize].size(); i++)
        {
            if (!enginesBySize[fftSize][i].m_inUse) {
                break;
            }
        }

        if (i < enginesBySize[fftSize].size())
        {
            qDebug("FFTFactory::getEngine: reuse engine: %u FFT %s size: %u", i, (inverse ? "inv" : "fwd"), fftSize);
            enginesBySize[fftSize][i].m_inUse = true;
            *engine = enginesBySize[fftSize][i].m_engine;
            return i;
        }
        else
        {
            std::vector<AllocatedEngine>& engines = enginesBySize[fftSize];
            qDebug("FFTFactory::getEngine: create engine: %lu FFT %s size: %u", engines.size(), (inverse ? "inv" : "fwd"), fftSize);
            engines.push_back(AllocatedEngine());
            engines.back().m_inUse = true;
            engines.back().m_engine = FFTEngine::create(m_fftwWisdomFileName);
            engines.back().m_engine->setReuse(false);
            engines.back().m_engine->configure(fftSize, inverse);
            *engine = engines.back().m_engine;
            return engines.size() - 1;
        }
    }
}

void FFTFactory::releaseEngine(unsigned int fftSize, bool inverse, unsigned int engineSequence)
{
    QMutexLocker mutexLocker(&m_mutex);
    std::map<unsigned int, std::vector<AllocatedEngine>>& enginesBySize = inverse ?
        m_invFFTEngineBySize : m_fftEngineBySize;

    if (enginesBySize.find(fftSize) != enginesBySize.end())
    {
        std::vector<AllocatedEngine>& engines = enginesBySize[fftSize];

        if (engineSequence < engines.size())
        {
            qDebug("FFTFactory::releaseEngine: engineSequence: %u FFT %s size: %u",
                engineSequence, (inverse ? "inv" : "fwd"), fftSize);
            engines[engineSequence].m_inUse = false;
        }
    }
}