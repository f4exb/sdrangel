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

#include <algorithm>
#include <numeric>

#include <QDebug>

#include "fftnr.h"

FFTNoiseReduction::FFTNoiseReduction(int len) :
    m_flen(len)
{
    m_scheme = SchemeAverage;
    m_mags = new float[m_flen];
    m_tmp = new float[m_flen];
    m_aboveAvgFactor = 1.0;
    m_sigmaFactor = 1.0;
    m_nbPeaks = m_flen;
}

FFTNoiseReduction::~FFTNoiseReduction()
{
    delete[] m_mags;
    delete[] m_tmp;
}

void FFTNoiseReduction::init()
{
    std::fill(m_mags, m_mags + m_flen, 0);
    std::fill(m_tmp, m_tmp + m_flen, 0);
    m_magAvg = 0;
}

void FFTNoiseReduction::push(cmplx data, int index)
{
    m_mags[index] = std::abs(data);

    if ((m_scheme == SchemeAverage) || (m_scheme == SchemeAvgStdDev)) {
        m_magAvg += m_mags[index];
    }
}

void FFTNoiseReduction::calc()
{
    if (m_scheme == SchemeAverage)
    {
        m_magAvg /= m_flen;
        m_magAvg = m_expFilter.push(m_magAvg);
    }
    if (m_scheme == SchemeAvgStdDev)
    {
        m_magAvg /= m_flen;

        auto variance_func = [this](float accumulator, const float& val) {
            return accumulator + ((val - m_magAvg)*(val - m_magAvg) / (m_flen - 1));
        };

        float var = std::accumulate(m_mags, m_mags + m_flen, 0.0, variance_func);
        m_magThr = (m_sigmaFactor/2.0)*std::sqrt(var) + m_magAvg;
        m_magThr = m_expFilter.push(m_magThr);
    }
    else if (m_scheme == SchemePeaks)
    {
        std::copy(m_mags, m_mags + m_flen, m_tmp);
        std::sort(m_tmp, m_tmp + m_flen);
        m_magThr = m_tmp[m_flen - m_nbPeaks];
    }
}

bool FFTNoiseReduction::cut(int index)
{
    if (m_scheme == SchemeAverage)
    {
        return m_mags[index] < m_aboveAvgFactor * m_magAvg;
    }
    else if ((m_scheme == SchemePeaks) || (m_scheme == SchemeAvgStdDev))
    {
        return m_mags[index] < m_magThr;
    }

    return false;
}

void FFTNoiseReduction::setScheme(Scheme scheme)
{
    if (m_scheme != scheme) {
        m_expFilter.reset();
    }

    m_scheme = scheme;
}

FFTNoiseReduction::ExponentialFilter::ExponentialFilter()
{
    m_alpha = 1.0;
    m_init = true;
}

float FFTNoiseReduction::ExponentialFilter::push(float newValue)
{
    if (m_init)
    {
        m_prev = newValue;
        m_init = false;
    }

    if (m_alpha == 1.0)
    {
        m_prev = newValue;
        return newValue;
    }
    else
    {
        float result = m_alpha*m_prev + (1.0 - m_alpha)*newValue;
        m_prev = result;
        return result;
    }
}

void FFTNoiseReduction::ExponentialFilter::reset()
{
    m_init = true;
}

void FFTNoiseReduction::ExponentialFilter::setAlpha(float alpha)
{
    m_alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
    qDebug("FFTNoiseReduction::ExponentialFilter::setAlpha: %f", m_alpha);
    m_init = true;
}

