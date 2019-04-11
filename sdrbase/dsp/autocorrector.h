///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DSP_AUTOCORRECTOR_H_
#define SDRBASE_DSP_AUTOCORRECTOR_H_

#include "util/movingaverage.h"

template<typename T>
class AutoCorrector
{
public:
    AutoCorrector(qint32 intBits);
    void process(const T& inreal, const T& inimag, qint32& outreal, qint32& outimag);
    void process(qint32& xreal, qint32& ximag);
    void setIQCorrection(bool iqCorrection) { m_iqCorrection = iqCorrection; }
private:
    bool m_iqCorrection;
    float m_scalef;
    MovingAverageUtil<int32_t, int64_t, 1024> m_iBeta;
    MovingAverageUtil<int32_t, int64_t, 1024> m_qBeta;
    MovingAverageUtil<float, double, 128> m_avgII;
    MovingAverageUtil<float, double, 128> m_avgIQ;
    MovingAverageUtil<float, double, 128> m_avgII2;
    MovingAverageUtil<float, double, 128> m_avgQQ2;
    MovingAverageUtil<double, double, 128> m_avgPhi;
    MovingAverageUtil<double, double, 128> m_avgAmp;
};

template<typename T>
AutoCorrector<T>::AutoCorrector(qint32 intBits) :
    m_iqCorrection(false),
    m_scalef((float) (1<<(intBits-1)))
{
}

template<typename T>
void AutoCorrector<T>::process(const T& inreal, const T& inimag, qint32& outreal, qint32& outimag)
{
    outreal = inreal;
    outimag = inimag;
    process(outreal, outimag);
}

template<typename T>
void AutoCorrector<T>::process(qint32& xreal, qint32& ximag)
{
    m_iBeta(xreal);
    m_qBeta(ximag);

    if (m_iqCorrection)
    {
        // DC correction and conversion
        float xi = (xreal - (int32_t) m_iBeta) / m_scalef;
        float xq = (ximag - (int32_t) m_qBeta) / m_scalef;

        // phase imbalance
        m_avgII(xi*xi); // <I", I">
        m_avgIQ(xi*xq); // <I", Q">


        if (m_avgII.asDouble() != 0) {
            m_avgPhi(m_avgIQ.asDouble()/m_avgII.asDouble());
        }

        float yi = xi - m_avgPhi.asDouble()*xq;
        float yq = xq;

        // amplitude I/Q imbalance
        m_avgII2(yi*yi); // <I, I>
        m_avgQQ2(yq*yq); // <Q, Q>

        if (m_avgQQ2.asDouble() != 0) {
            m_avgAmp(sqrt(m_avgII2.asDouble() / m_avgQQ2.asDouble()));
        }

        // final correction
        float zi = yi;
        float zq = m_avgAmp.asDouble() * yq;

        // convert and store
        xreal = zi * m_scalef;
        ximag = zq * m_scalef;
    }
    else
    {
        xreal -= (int32_t) m_iBeta;
        ximag -= (int32_t) m_qBeta;
    }
}

#endif /* SDRBASE_DSP_AUTOCORRECTOR_H_ */
