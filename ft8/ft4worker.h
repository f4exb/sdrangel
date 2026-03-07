///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// reformatted and adapted to Qt and SDRangel context                            //
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
#ifndef INCLUDE_FT4WORKER_H
#define INCLUDE_FT4WORKER_H

#include <QObject>
#include <vector>

#include "ft4.h"

namespace FT8 {

class CallbackInterface;

class FT4Worker : public QObject
{
    Q_OBJECT
public:
    FT4Worker(
        const std::vector<float>& samples,
        float minHz,
        float maxHz,
        int start,
        int rate,
        CallbackInterface *cb,
        const FT4ParamsLight& params
    );

    void start_work();

private:
    static float binPowerInterpolated(const FFTEngine::ffts_t& bins, int symbolIndex, float toneBin);
    void collectSyncMetrics(const FFTEngine::ffts_t& bins, float toneBin, float& sync, float& noise) const;
    float refineToneBin(const FFTEngine::ffts_t& bins, int toneBin) const;
    void buildBitMetrics(const FFTEngine::ffts_t& bins, float toneBin, std::array<float, 174>& bitMetrics) const;
    FFTEngine::ffts_t makeFrameBins(int start);
    void decode();

    std::vector<float> m_samples;
    float m_minHz;
    float m_maxHz;
    int m_start;
    int m_rate;
    CallbackInterface *m_cb;
    FT4ParamsLight m_params;
    FFTEngine m_fftEngine;
};

} // namespace FT8

#endif // INCLUDE_FT4WORKER_H
