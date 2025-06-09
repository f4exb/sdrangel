///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "whittakereilers.h"

WhittakerEilers::WhittakerEilers() :
    v1a(nullptr),
    v2a(nullptr),
    da(nullptr),
    dtd(nullptr),
    ca(nullptr),
    za(nullptr),
    zb(nullptr),
    b(nullptr),
    m_length(0)
{
}

WhittakerEilers::~WhittakerEilers()
{
    dealloc();
}

void WhittakerEilers::alloc(int length)
{
    v1a = new double[length];
    v2a = new double[length];
    da = new double[length * 3];
    dtd = new double[length * 3];
    ca = new double[length * 3];
    za = new double[length];
    zb = new double[length];
    b = new double[length];
    m_length = length;
}

void WhittakerEilers::dealloc()
{
    delete v1a;
    delete v2a;
    delete da;
    delete dtd;
    delete ca;
    delete za;
    delete zb;
    delete b;
    m_length = 0;
}

void WhittakerEilers::filter(const double *xi, double *yi, const double *w, const int length, const double lambda)
{
    int i;
    int j;
    int k;
    const int m = length;

    if (m_length < length)
    {
        dealloc();
        alloc(length);
    }

    for (i = 0; i < m - 1; i++) {
        v1a[i] = 1.0 / (xi[i + 1] - xi[i]);
    }
    v1a[m - 1] = 0.0;
    for (i = 0; i < m - 2; i++) {
        v2a[i] = 1.0 / (xi[i + 2] - xi[i]);
    }
    v2a[m - 1] = 0.0;
    v2a[m - 2] = 0.0;

    //Wa = w;
    // D1 = V1 * diff(I)
    for (i = 0; i < m - 1; i++) {
        da[i*3] = -v1a[i];
        da[i*3+1] = v1a[i];
    }

    // D2 = V2 * diff(D1)
    for (i = 0; i < m - 2; i++) {
        da[i*3] = v2a[i] * -da[i*3];
        da[i*3+1] = v2a[i] * (da[(i+1)*3] - da[i*3+1]);
        da[i*3+2] = v2a[i] * da[(i+1)*3+1];
    }
    for (i = 1; i <= 6; i++) {
        da[m * 3 - i] = 0;
    }

    dtd[0 * 3] = lambda * da[0 * 3] * da[0 * 3];
    dtd[0 * 3 + 1] = lambda * da[0 * 3] * da[0 * 3 + 1];
    dtd[0 * 3 + 2] = lambda * da[0 * 3] * da[0 * 3 + 2];

    dtd[1 * 3] = lambda * (da[0 * 3 + 1] * da[0 * 3 + 1] + da[1 * 3] * da[1 * 3]);
    dtd[1 * 3 + 1] = lambda * (da[0 * 3 + 1] * da[0 * 3 + 2] + da[1 * 3] * da[1 * 3 + 1]);
    dtd[1 * 3 + 2] = lambda * (da[1 * 3] * da[1 * 3 + 2]);

    for (int row = 2; row < m; row++) {
        dtd[row * 3] = lambda * (da[(row - 2) * 3 + 2] * da[(row - 2) * 3 + 2] + da[(row - 1) * 3 + 1] * da[(row - 1) * 3 + 1] + da[row * 3] * da[row * 3]);
        dtd[row * 3 + 1] = lambda * (da[(row - 1) * 3 + 1] * da[(row - 1) * 3 + 2] + da[(row) * 3] * da[(row) * 3 + 1]);
        dtd[row * 3 + 2] = lambda * (da[(row) * 3] * da[(row) * 3 + 2]);
    }

    // Add in W
    for (i = 0; i < m; i++) {
        dtd[i * 3] = w[i] + dtd[i * 3];
    }

    // Cholesky Decomposition

    i = 1;
    j = i - 1;
    ca[j * 3] = sqrt(dtd[j * 3]);

    i = 2;
    j = i - 2;
    ca[j * 3 + 1] = 1.0 / ca[j * 3] * dtd[j * 3 + 1];

    i = 2;
    j = i - 1;
    k = i - 2;
    double sum = ca[k * 3 + 1] * ca[k * 3 + 1];
    ca[j * 3] = sqrt(dtd[j * 3] - sum);

    for (i = 3; i <= m; i++) {
        j = i - 3;
        ca[j * 3 + 2] = 1.0 / ca[j * 3] * dtd[j * 3 + 2];

        k = i - 3;
        sum = ca[k * 3 + 2] * ca[k * 3 + 1];
        j = i - 2;
        ca[j * 3 + 1] = 1.0 / ca[j * 3] * (dtd[j * 3 + 1] - sum);

        k = i - 3;
        sum = ca[k * 3 + 2] * ca[k * 3 + 2];
        k = i - 2;
        sum = sum + ca[k * 3 + 1] * ca[k * 3 + 1];
        j = i - 1;
        ca[j * 3] = sqrt(dtd[j * 3] - sum);
    }
    ca[m * 3 - 1] = 0;
    ca[m * 3 - 2] = 0;
    ca[m * 3 - 4] = 0;

    // % Forward substitution(C' \ (w .* y))
    for (i = 0; i < m; i++) {
        b[i] = w[i] * yi[i];
    }

    za[0] = b[0] / ca[0];

    sum = ca[0 * 3 + 1] * za[0];
    za[1] = (b[1] - sum) / ca[1 * 3];

    for (i = 3; i <= m; i++) {
        sum = ca[(i - 3) * 3 + 2] * za[i - 3] + ca[(i - 2) * 3 + 1] * za[i - 2];
        za[i - 1] = (b[i - 1] - sum) / ca[(i - 1) * 3];
    }

    // Backward substituion  C \ (C' \ (w .* y));
    b = za;

    i = m - 1;
    zb[i] = b[i] / ca[i * 3];

    i = m - 2;
    sum = ca[i * 3 + 1] * zb[i + 1];
    zb[i] = (b[i] - sum) / ca[i * 3];

    for (i = m - 2; i >= 1; i--) {
        sum = ca[(i - 1) * 3 + 2] * zb[i + 1] + ca[(i - 1) * 3 + 1] * zb[i];
        zb[i-1] = (b[i - 1] - sum) / ca[(i - 1) * 3];
    }

    if (std::isnan(zb[0])) {
        qDebug() << "lambda" << lambda;
        for (int i = 0; i < m; i++) {
            qDebug() << "xi[" << i << "]" << qSetRealNumberPrecision(12) << xi[i];
        }
        for (int i = 0; i < m; i++) {
            qDebug() << "yi[" << i << "]" << qSetRealNumberPrecision(12) << yi[i];
        }
        for (int i = 0; i < m; i++) {
            qDebug() << "w[" << i << "]" << qSetRealNumberPrecision(12) << w[i];
        }
        for (int i = 0; i < m; i++) {
            qDebug() << "v1a[" << i << "]" << qSetRealNumberPrecision(12) << v1a[i];
        }
        for (int i = 0; i < m; i++) {
            qDebug() << "v2a[" << i << "]" << qSetRealNumberPrecision(12) << v2a[i];
        }
        for (int i = 0; i < m * 3; i++) {
            qDebug() << "da[" << i << "]" << qSetRealNumberPrecision(12) << da[i];
        }
        for (int i = 0; i < m * 3; i++) {
            qDebug() << "dtd[" << i << "]" << qSetRealNumberPrecision(12) << dtd[i];
        }
        for (int i = 0; i < m * 3; i++) {
            qDebug() << "ca[" << i << "]" << qSetRealNumberPrecision(12) << ca[i];
        }
        for (int i = 0; i < m; i++) {
            qDebug() << "za[" << i << "]" << qSetRealNumberPrecision(12) << za[i];
        }
        for (int i = 0; i < m; i++) {
            qDebug() << "zb[" << i << "]" << qSetRealNumberPrecision(12) << zb[i];
        }
        // Don't put NaNs in output
        return;
    }

    // Copy result back to input
    for (i = 0; i < m; i++) {
        yi[i] = zb[i];
    }

    /*for (i = 0; i < m; i++) {
        qDebug() << "zb[" << i << "]=" << zb[i];
    }*/
}
