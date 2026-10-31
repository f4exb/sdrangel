///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_ACARSIRIDIUMFFTENGINE_H
#define INCLUDE_ACARSIRIDIUMFFTENGINE_H

#include <cmath>
#include <complex>
#include <vector>

#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/fftengine.h"
#include "dsp/fftfactory.h"

#include "acarsiridiumchannelizer.h"
#include "acarsiridiumfft.h"

// Puts SDRangel's FFT engine behind the Iridium channelizer.
//
// The bank runs one transform per output block, which at the widest setting is a hundred
// and twenty five thousand 256 point transforms a second, and that was over a third of
// everything the scanner cost. The radix 2 in acarsiridiumfft.h is deliberately plain so
// that the offline harnesses in test/ can build against Qt alone; here, where sdrbase is
// available, FFTW does the same transform several times faster.
//
// Single precision, because that is what SDRangel's engines work in. The polyphase
// filtering either side of it stays double, and a 256 point transform in float carries
// far more precision than a burst detector and a DQPSK slicer can use.

class AcarsIridiumFftEngine : public AcarsIridiumTransform
{
public:
    AcarsIridiumFftEngine() :
        m_engine(nullptr),
        m_sequence(0),
        m_size(0)
    {
    }

    ~AcarsIridiumFftEngine()
    {
        release();
    }

    void configure(int size) override
    {
        release();

        FFTFactory *factory = DSPEngine::instance()->getFFTFactory();

        // Inverse, which the factory configures the engine for. Both this and the radix 2
        // leave it unscaled, which is what a filter bank wants
        m_sequence = factory->getEngine(size, true, &m_engine);
        m_size = size;
        m_builtIn.configure(size);

        if (!m_engine)
        {
            qWarning() << "AcarsIridiumFftEngine: no engine for size" << size
                       << "- falling back to the built-in transform";
            return;
        }

        qInfo() << "AcarsIridiumFftEngine: using" << m_engine->getName()
                << "for the" << size << "point channelizer transform";

        checkAgainstBuiltIn(size);
    }

    void transform(std::vector<std::complex<double>>& data) override
    {
        if (!m_engine)
        {
            m_builtIn.transform(data, true);
            return;
        }

        Complex *in = m_engine->in();

        for (int i = 0; i < m_size; i++) {
            in[i] = Complex((float) data[i].real(), (float) data[i].imag());
        }

        m_engine->transform();

        const Complex *out = m_engine->out();

        for (int i = 0; i < m_size; i++) {
            data[i] = std::complex<double>(out[i].real(), out[i].imag());
        }
    }

private:

    //! One transform of a known vector through both, compared, when the engine is taken.
    //!
    //! Worth the once-per-configure cost because the ways this can be wrong are not
    //! subtle but are silent: a library that scales its inverse by 1/N, or orders its
    //! output differently, would leave the bank producing plausible rubbish and the only
    //! visible symptom would be that nothing ever decodes. The tests in test/ cannot
    //! cover this path, because they build without sdrbase and so never see this engine
    void checkAgainstBuiltIn(int size)
    {
        AcarsIridiumFft reference;
        reference.configure(size);

        std::vector<std::complex<double>> a(size);

        for (int i = 0; i < size; i++)
        {
            const double phase = 0.7 * i;
            a[i] = std::complex<double>(std::cos(phase) + (0.1 * i), std::sin(phase) - (0.05 * i));
        }

        std::vector<std::complex<double>> b = a;

        reference.transform(a, true);
        transform(b);

        double worst = 0.0;
        double scale = 1e-12;

        for (int i = 0; i < size; i++)
        {
            worst = std::max(worst, std::abs(b[i] - a[i]));
            scale = std::max(scale, std::abs(a[i]));
        }

        // Generous: single precision over a 256 point transform of values this large is
        // good to about a part in a million, and anything structurally wrong is out by
        // orders of magnitude rather than by rounding
        if ((worst / scale) > 1e-4)
        {
            qWarning() << "AcarsIridiumFftEngine:" << m_engine->getName()
                       << "disagrees with the built-in transform by" << (worst / scale)
                       << "- falling back to the built-in one";
            release();
        }
    }

    void release()
    {
        if (m_engine)
        {
            DSPEngine::instance()->getFFTFactory()->releaseEngine(m_size, true, m_sequence);
            m_engine = nullptr;
        }
    }

    FFTEngine *m_engine;
    unsigned int m_sequence;
    int m_size;
    AcarsIridiumFft m_builtIn;
};

#endif // INCLUDE_ACARSIRIDIUMFFTENGINE_H
