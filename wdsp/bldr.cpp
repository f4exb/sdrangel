/*  lmath.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015, 2016, 2023 Warren Pratt, NR0V
Copyright (C) 2024 Edouard Griffiths, F4EXB Adapted to SDRangel

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

#include "comm.hpp"
#include "bldr.hpp"

namespace WDSP {

BLDR::BLDR(int points, int ints)
{
    // for the create function, 'points' and 'ints' are the MAXIMUM values that will be encountered
    catxy = new double[2 * points];
    sx.resize(points);
    sy.resize(points);
    h .resize(ints);
    p.resize(ints);
    np.resize(ints);
    taa.resize(ints);
    tab.resize(ints);
    tag.resize(ints);
    tad.resize(ints);
    tbb.resize(ints);
    tbg.resize(ints);
    tbd.resize(ints);
    tgg.resize(ints);
    tgd.resize(ints);
    tdd.resize(ints);
    int nsize = 3 * ints + 1;
    int intp1 = ints + 1;
    int intm1 = ints - 1;
    A .resize(intp1 * intp1);
    B .resize(intp1 * intp1);
    C .resize(intp1 * intp1);
    D .resize(intp1);
    E .resize(intp1 * intp1);
    F .resize(intm1 * intp1);
    G .resize(intp1);
    MAT.resize(nsize * nsize);
    RHS.resize(nsize);
    SLN.resize(nsize);
    z .resize(intp1);
    zp.resize(intp1);
    wrk.resize(nsize);
    ipiv.resize(nsize);
}

BLDR::~BLDR()
{
    delete[]catxy;
}

void BLDR::flush(int points)
{
    memset(catxy, 0, 2 * points * sizeof(double));
    std::fill(sx.begin(),   sx.end(),   0);
    std::fill(sy.begin(),   sy.end(),   0);
    std::fill(h.begin(),    h.end(),    0);
    std::fill(p.begin(),    p.end(),    0);
    std::fill(np.begin(),   np.end(),   0);
    std::fill(taa.begin(),  taa.end(),  0);
    std::fill(tab.begin(),  tab.end(),  0);
    std::fill(tag.begin(),  tag.end(),  0);
    std::fill(tad.begin(),  tad.end(),  0);
    std::fill(tbb.begin(),  tbb.end(),  0);
    std::fill(tbg.begin(),  tbg.end(),  0);
    std::fill(tbd.begin(),  tbd.end(),  0);
    std::fill(tgg.begin(),  tgg.end(),  0);
    std::fill(tgd.begin(),  tgd.end(),  0);
    std::fill(tdd.begin(),  tdd.end(),  0);
    std::fill(A.begin(),    A.end(),    0);
    std::fill(B.begin(),    B.end(),    0);
    std::fill(C.begin(),    C.end(),    0);
    std::fill(D.begin(),    D.end(),    0);
    std::fill(E.begin(),    E.end(),    0);
    std::fill(F.begin(),    F.end(),    0);
    std::fill(G.begin(),    G.end(),    0);
    std::fill(MAT.begin(),  MAT.end(),  0);
    std::fill(RHS.begin(),  RHS.end(),  0);
    std::fill(SLN.begin(),  SLN.end(),  0);
    std::fill(z.begin(),    z.end(),    0);
    std::fill(zp.begin(),   zp.end(),   0);
    std::fill(wrk.begin(),  wrk.end(),  0);
    std::fill(ipiv.begin(), ipiv.end(), 0);
}

int BLDR::fcompare(const void* a, const void* b)
{
    if (*(double*)a < *(double*)b)
        return -1;
    else if (*(double*)a == *(double*)b)
        return 0;
    else
        return 1;
}

void BLDR::decomp(int n, std::vector<double>& a, std::vector<int>& piv, int* info, std::vector<double>& wrk)
{
    int i;
    int j;
    int t_piv;
    double m_row;
    double mt_row;
    double m_col;
    double mt_col;
    *info = 0;
    for (i = 0; i < n; i++)
    {
        piv[i] = i;
        m_row = 0.0;
        for (j = 0; j < n; j++)
        {
            mt_row = a[n * i + j];
            if (mt_row < 0.0)  mt_row = -mt_row;
            if (mt_row > m_row)  m_row = mt_row;
        }
        if (m_row == 0.0)
        {
            *info = i;
            goto cleanup;
        }
        wrk[i] = m_row;
    }
    for (int k = 0; k < n - 1; k++)
    {
        j = k;
        m_col = a[n * piv[k] + k] / wrk[piv[k]];
        if (m_col < 0)  m_col = -m_col;
        for (i = k + 1; i < n; i++)
        {
            mt_col = a[n * piv[i] + k] / wrk[piv[k]];
            if (mt_col < 0.0)  mt_col = -mt_col;
            if (mt_col > m_col)
            {
                m_col = mt_col;
                j = i;
            }
        }
        if (m_col == 0)
        {
            *info = -k;
            goto cleanup;
        }
        t_piv = piv[k];
        piv[k] = piv[j];
        piv[j] = t_piv;
        for (i = k + 1; i < n; i++)
        {
            a[n * piv[i] + k] /= a[n * piv[k] + k];
            for (j = k + 1; j < n; j++)
                a[n * piv[i] + j] -= a[n * piv[i] + k] * a[n * piv[k] + j];
        }
    }
    if (a[n * n - 1] == 0.0)
        *info = -n;
cleanup:
    return;
}

void BLDR::dsolve(int n, std::vector<double>& a, std::vector<int>& piv, std::vector<double>& b, std::vector<double>& x)
{
    int j;
    int k;
    double sum;

    for (k = 0; k < n; k++)
    {
        sum = 0.0;
        for (j = 0; j < k; j++)
            sum += a[n * piv[k] + j] * x[j];
        x[k] = b[piv[k]] - sum;
    }

    for (k = n - 1; k >= 0; k--)
    {
        sum = 0.0;
        for (j = k + 1; j < n; j++)
            sum += a[n * piv[k] + j] * x[j];
        x[k] = (x[k] - sum) / a[n * piv[k] + k];
    }
}

void BLDR::cull(int* n, int ints, std::vector<double>& x, const double* t, double ptol)
{
    int k = 0;
    int i = *n;
    int ntopint;
    int npx;

    while (x[i - 1] > t[ints - 1])
        i--;
    ntopint = *n - i;
    npx = (int)(ntopint * (1.0 - ptol));
    i = *n;
    while ((k < npx) && (x[--i] > t[ints]))
        k++;
    *n -= k;
}

void BLDR::execute(int points, const double* x, const double* y, int ints, const double* t, int* info, double* c, double ptol)
{
    double u;
    double v;
    double alpha;
    double beta;
    double gamma;
    double delta;
    int nsize = 3 * ints + 1;
    int intp1 = ints + 1;
    int intm1 = ints - 1;
    int i;
    int j;
    int k;
    int m;
    int dinfo;
    flush(points);
    for (i = 0; i < points; i++)
    {
        catxy[2 * i + 0] = x[i];
        catxy[2 * i + 1] = y[i];
    }
    qsort(catxy, points, 2 * sizeof(double), fcompare);
    for (i = 0; i < points; i++)
    {
        sx[i] = catxy[2 * i + 0];
        sy[i] = catxy[2 * i + 1];
    }
    cull(&points, ints, sx, t, ptol);
    if (points <= 0 || sx[points - 1] > t[ints])
    {
        *info = -1000;
        goto cleanup;
    }
    else *info = 0;

    for (j = 0; j < ints; j++)
        h[j] = t[j + 1] - t[j];
    p[0] = 0;
    j = 0;
    for (i = 0; i < points; i++)
    {
        if (sx[i] <= t[j + 1])
            np[j]++;
        else
        {
            p[++j] = i;
            while (sx[i] > t[j + 1])
                p[++j] = i;
            np[j] = 1;
        }
    }
    for (i = 0; i < ints; i++)
        for (j = p[i]; j < p[i] + np[i]; j++)
        {
            u = (sx[j] - t[i]) / h[i];
            v = u - 1.0;
            alpha = (2.0 * u + 1.0) * v * v;
            beta = u * u * (1.0 - 2.0 * v);
            gamma = h[i] * u * v * v;
            delta = h[i] * u * u * v;
            taa[i] += alpha * alpha;
            tab[i] += alpha * beta;
            tag[i] += alpha * gamma;
            tad[i] += alpha * delta;
            tbb[i] += beta * beta;
            tbg[i] += beta * gamma;
            tbd[i] += beta * delta;
            tgg[i] += gamma * gamma;
            tgd[i] += gamma * delta;
            tdd[i] += delta * delta;
            D[i + 0] += 2.0 * sy[j] * alpha;
            D[i + 1] += 2.0 * sy[j] * beta;
            G[i + 0] += 2.0 * sy[j] * gamma;
            G[i + 1] += 2.0 * sy[j] * delta;
        }
    for (i = 0; i < ints; i++)
    {
        A[(i + 0) * intp1 + (i + 0)] += 2.0 * taa[i];
        A[(i + 1) * intp1 + (i + 1)] = 2.0 * tbb[i];
        A[(i + 0) * intp1 + (i + 1)] = 2.0 * tab[i];
        A[(i + 1) * intp1 + (i + 0)] = 2.0 * tab[i];
        B[(i + 0) * intp1 + (i + 0)] += 2.0 * tag[i];
        B[(i + 1) * intp1 + (i + 1)] = 2.0 * tbd[i];
        B[(i + 0) * intp1 + (i + 1)] = 2.0 * tbg[i];
        B[(i + 1) * intp1 + (i + 0)] = 2.0 * tad[i];
        E[(i + 0) * intp1 + (i + 0)] += 2.0 * tgg[i];
        E[(i + 1) * intp1 + (i + 1)] = 2.0 * tdd[i];
        E[(i + 0) * intp1 + (i + 1)] = 2.0 * tgd[i];
        E[(i + 1) * intp1 + (i + 0)] = 2.0 * tgd[i];
    }
    for (i = 0; i < intm1; i++)
    {
        C[i * intp1 + (i + 0)] = +3.0 * h[i + 1] / h[i];
        C[i * intp1 + (i + 2)] = -3.0 * h[i] / h[i + 1];
        C[i * intp1 + (i + 1)] = -C[i * intp1 + (i + 0)] - C[i * intp1 + (i + 2)];
        F[i * intp1 + (i + 0)] = h[i + 1];
        F[i * intp1 + (i + 1)] = 2.0 * (h[i] + h[i + 1]);
        F[i * intp1 + (i + 2)] = h[i];
    }
    for (i = 0, k = 0; i < intp1; i++, k++)
    {
        for (j = 0, m = 0; j < intp1; j++, m++)
            MAT[k * nsize + m] = A[i * intp1 + j];
        for (j = 0, m = intp1; j < intp1; j++, m++)
            MAT[k * nsize + m] = B[j * intp1 + i];
        for (j = 0, m = 2 * intp1; j < intm1; j++, m++)
            MAT[k * nsize + m] = C[j * intp1 + i];
        RHS[k] = D[i];
    }
    for (i = 0, k = intp1; i < intp1; i++, k++)
    {
        for (j = 0, m = 0; j < intp1; j++, m++)
            MAT[k * nsize + m] = B[i * intp1 + j];
        for (j = 0, m = intp1; j < intp1; j++, m++)
            MAT[k * nsize + m] = E[i * intp1 + j];
        for (j = 0, m = 2 * intp1; j < intm1; j++, m++)
            MAT[k * nsize + m] = F[j * intp1 + i];
        RHS[k] = G[i];
    }
    for (i = 0, k = 2 * intp1; i < intm1; i++, k++)
    {
        for (j = 0, m = 0; j < intp1; j++, m++)
            MAT[k * nsize + m] = C[i * intp1 + j];
        for (j = 0, m = intp1; j < intp1; j++, m++)
            MAT[k * nsize + m] = F[i * intp1 + j];
        for (j = 0, m = 2 * intp1; j < intm1; j++, m++)
            MAT[k * nsize + m] = 0.0;
        RHS[k] = 0.0;
    }
    decomp(nsize, MAT, ipiv, &dinfo, wrk);
    dsolve(nsize, MAT, ipiv, RHS, SLN);
    if (dinfo != 0)
    {
        *info = dinfo;
        goto cleanup;
    }

    for (i = 0; i <= ints; i++)
    {
        z[i] = SLN[i];
        zp[i] = SLN[i + ints + 1];
    }
    for (i = 0; i < ints; i++)
    {
        c[4 * i + 0] = z[i];
        c[4 * i + 1] = zp[i];
        c[4 * i + 2] = -3.0 / (h[i] * h[i]) * (z[i] - z[i + 1]) - 1.0 / h[i] * (2.0 * zp[i] + zp[i + 1]);
        c[4 * i + 3] = 2.0 / (h[i] * h[i] * h[i]) * (z[i] - z[i + 1]) + 1.0 / (h[i] * h[i]) * (zp[i] + zp[i + 1]);
    }
cleanup:
    return;
}

} // namespace WDSP
