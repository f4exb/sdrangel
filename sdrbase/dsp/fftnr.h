///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Helper class for noise reduction                                              //
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

#ifndef	_FFTNR_H
#define	_FFTNR_H

#include <complex>

#include "export.h"

class SDRBASE_API FFTNoiseReduction {
public:
    typedef std::complex<float> cmplx;
    enum Scheme {
        SchemeAverage,
        SchemeAvgStdDev,
        SchemePeaks
    };

    FFTNoiseReduction(int len);
    ~FFTNoiseReduction();

    void init(); //!< call befor start of initial FFT scan
    void push(cmplx data, int index); //!< Push FFT bin during initial FFT scan
    void calc(); //!< calculate after initial FFT scan
    bool cut(int index); //!< true if bin is to be zeroed else false (during second FFT scan)
    void setAlpha(float alpha) { m_expFilter.setAlpha(alpha); }
    void setScheme(Scheme scheme);

    float m_aboveAvgFactor; //!< above average factor
    float m_sigmaFactor; //!< sigma multiplicator for average + std deviation
    int m_nbPeaks; //!< number of peaks (peaks scheme)

private:
    class ExponentialFilter {
        public:
            ExponentialFilter();
            float push(float newValue);
            void reset();
            void setAlpha(float alpha);
        private:
            bool m_init;
            float m_alpha;
            float m_prev;
    };

    Scheme m_scheme;
    int m_flen; //!< FFT length
    float *m_mags; //!< magnitudes (PSD)
    float *m_tmp; //!< temporary buffer
    float m_magAvg; //!< average of magnitudes
    float m_magThr; //!< magnitude threshold (peaks scheme)
    ExponentialFilter m_expFilter; //!< exponential filter for parameter smoothing
};

#endif
