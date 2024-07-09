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

BLDR* BLDR::create_builder(int points, int ints)
{
    // for the create function, 'points' and 'ints' are the MAXIMUM values that will be encountered
    BLDR *a = new BLDR;
    a->catxy = new float[2 * points]; // (float*)malloc0(2 * points * sizeof(float));
    a->sx    = new float[points];     // (float*)malloc0(    points * sizeof(float));
    a->sy    = new float[points];     // (float*)malloc0(    points * sizeof(float));
    a->h     = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->p     = new int[ints];          // (int*)   malloc0(    ints   * sizeof(int));
    a->np    = new int[ints];          // (int*)   malloc0(    ints   * sizeof(int));
    a->taa   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tab   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tag   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tad   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tbb   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tbg   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tbd   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tgg   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tgd   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    a->tdd   = new float[ints];       // (float*)malloc0(    ints   * sizeof(float));
    int nsize = 3 * ints + 1;
    int intp1 = ints + 1;
    int intm1 = ints - 1;
    a->A     = new float[intp1 * intp1]; // (float*)malloc0(intp1 * intp1 * sizeof(float));
    a->B     = new float[intp1 * intp1]; // (float*)malloc0(intp1 * intp1 * sizeof(float));
    a->C     = new float[intp1 * intp1]; // (float*)malloc0(intm1 * intp1 * sizeof(float));
    a->D     = new float[intp1];         // (float*)malloc0(intp1         * sizeof(float));
    a->E     = new float[intp1 * intp1]; // (float*)malloc0(intp1 * intp1 * sizeof(float));
    a->F     = new float[intm1 * intp1]; // (float*)malloc0(intm1 * intp1 * sizeof(float));
    a->G     = new float[intp1];         // (float*)malloc0(intp1         * sizeof(float));
    a->MAT   = new float[nsize * nsize]; // (float*)malloc0(nsize * nsize * sizeof(float));
    a->RHS   = new float[nsize];         // (float*)malloc0(nsize         * sizeof(float));
    a->SLN   = new float[nsize];         // (float*)malloc0(nsize         * sizeof(float));
    a->z     = new float[intp1];         // (float*)malloc0(intp1         * sizeof(float));
    a->zp    = new float[intp1];         // (float*)malloc0(intp1         * sizeof(float));
    a->wrk   = new float[nsize];         // (float*)malloc0(nsize         * sizeof(float));
    a->ipiv  = new int[nsize];            // (int*)   malloc0(nsize         * sizeof(int));
    return a;
}

void BLDR::destroy_builder(BLDR *a)
{
    delete[](a->ipiv);
    delete[](a->wrk);
    delete[](a->catxy);
    delete[](a->sx);
    delete[](a->sy);
    delete[](a->h);
    delete[](a->p);
    delete[](a->np);

    delete[](a->taa);
    delete[](a->tab);
    delete[](a->tag);
    delete[](a->tad);
    delete[](a->tbb);
    delete[](a->tbg);
    delete[](a->tbd);
    delete[](a->tgg);
    delete[](a->tgd);
    delete[](a->tdd);

    delete[](a->A);
    delete[](a->B);
    delete[](a->C);
    delete[](a->D);
    delete[](a->E);
    delete[](a->F);
    delete[](a->G);

    delete[](a->MAT);
    delete[](a->RHS);
    delete[](a->SLN);

    delete[](a->z);
    delete[](a->zp);

    delete(a);
}

void BLDR::flush_builder(BLDR *a, int points, int ints)
{
    memset(a->catxy, 0, 2 * points * sizeof(float));
    memset(a->sx,    0, points * sizeof(float));
    memset(a->sy,    0, points * sizeof(float));
    memset(a->h,     0, ints * sizeof(float));
    memset(a->p,     0, ints * sizeof(int));
    memset(a->np,    0, ints * sizeof(int));
    memset(a->taa,   0, ints * sizeof(float));
    memset(a->tab,   0, ints * sizeof(float));
    memset(a->tag,   0, ints * sizeof(float));
    memset(a->tad,   0, ints * sizeof(float));
    memset(a->tbb,   0, ints * sizeof(float));
    memset(a->tbg,   0, ints * sizeof(float));
    memset(a->tbd,   0, ints * sizeof(float));
    memset(a->tgg,   0, ints * sizeof(float));
    memset(a->tgd,   0, ints * sizeof(float));
    memset(a->tdd,   0, ints * sizeof(float));
    int nsize = 3 * ints + 1;
    int intp1 = ints + 1;
    int intm1 = ints - 1;
    memset(a->A,     0, intp1 * intp1 * sizeof(float));
    memset(a->B,     0, intp1 * intp1 * sizeof(float));
    memset(a->C,     0, intm1 * intp1 * sizeof(float));
    memset(a->D,     0, intp1         * sizeof(float));
    memset(a->E,     0, intp1 * intp1 * sizeof(float));
    memset(a->F,     0, intm1 * intp1 * sizeof(float));
    memset(a->G,     0, intp1         * sizeof(float));
    memset(a->MAT,   0, nsize * nsize * sizeof(float));
    memset(a->RHS,   0, nsize * sizeof(float));
    memset(a->SLN,   0, nsize * sizeof(float));
    memset(a->z,     0, intp1 * sizeof(float));
    memset(a->zp,    0, intp1 * sizeof(float));
    memset(a->wrk,   0, nsize * sizeof(float));
    memset(a->ipiv,  0, nsize * sizeof(int));
}

int BLDR::fcompare(const void* a, const void* b)
{
    if (*(float*)a < *(float*)b)
        return -1;
    else if (*(float*)a == *(float*)b)
        return 0;
    else
        return 1;
}

void BLDR::decomp(int n, float* a, int* piv, int* info, float* wrk)
{
    int i, j, k;
    int t_piv;
    float m_row, mt_row, m_col, mt_col;
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
    for (k = 0; k < n - 1; k++)
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

void BLDR::dsolve(int n, float* a, int* piv, float* b, float* x)
{
    int j, k;
    float sum;

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

void BLDR::cull(int* n, int ints, float* x, float* t, float ptol)
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

void BLDR::xbuilder(BLDR *a, int points, float* x, float* y, int ints, float* t, int* info, float* c, float ptol)
{
    float u, v, alpha, beta, gamma, delta;
    int nsize = 3 * ints + 1;
    int intp1 = ints + 1;
    int intm1 = ints - 1;
    int i, j, k, m;
    int dinfo;
    flush_builder(a, points, ints);
    for (i = 0; i < points; i++)
    {
        a->catxy[2 * i + 0] = x[i];
        a->catxy[2 * i + 1] = y[i];
    }
    qsort(a->catxy, points, 2 * sizeof(float), fcompare);
    for (i = 0; i < points; i++)
    {
        a->sx[i] = a->catxy[2 * i + 0];
        a->sy[i] = a->catxy[2 * i + 1];
    }
    cull(&points, ints, a->sx, t, ptol);
    if (points <= 0 || a->sx[points - 1] > t[ints])
    {
        *info = -1000;
        goto cleanup;
    }
    else *info = 0;

    for (j = 0; j < ints; j++)
        a->h[j] = t[j + 1] - t[j];
    a->p[0] = 0;
    j = 0;
    for (i = 0; i < points; i++)
    {
        if (a->sx[i] <= t[j + 1])
            a->np[j]++;
        else
        {
            a->p[++j] = i;
            while (a->sx[i] > t[j + 1])
                a->p[++j] = i;
            a->np[j] = 1;
        }
    }
    for (i = 0; i < ints; i++)
        for (j = a->p[i]; j < a->p[i] + a->np[i]; j++)
        {
            u = (a->sx[j] - t[i]) / a->h[i];
            v = u - 1.0;
            alpha = (2.0 * u + 1.0) * v * v;
            beta = u * u * (1.0 - 2.0 * v);
            gamma = a->h[i] * u * v * v;
            delta = a->h[i] * u * u * v;
            a->taa[i] += alpha * alpha;
            a->tab[i] += alpha * beta;
            a->tag[i] += alpha * gamma;
            a->tad[i] += alpha * delta;
            a->tbb[i] += beta * beta;
            a->tbg[i] += beta * gamma;
            a->tbd[i] += beta * delta;
            a->tgg[i] += gamma * gamma;
            a->tgd[i] += gamma * delta;
            a->tdd[i] += delta * delta;
            a->D[i + 0] += 2.0 * a->sy[j] * alpha;
            a->D[i + 1] += 2.0 * a->sy[j] * beta;
            a->G[i + 0] += 2.0 * a->sy[j] * gamma;
            a->G[i + 1] += 2.0 * a->sy[j] * delta;
        }
    for (i = 0; i < ints; i++)
    {
        a->A[(i + 0) * intp1 + (i + 0)] += 2.0 * a->taa[i];
        a->A[(i + 1) * intp1 + (i + 1)] = 2.0 * a->tbb[i];
        a->A[(i + 0) * intp1 + (i + 1)] = 2.0 * a->tab[i];
        a->A[(i + 1) * intp1 + (i + 0)] = 2.0 * a->tab[i];
        a->B[(i + 0) * intp1 + (i + 0)] += 2.0 * a->tag[i];
        a->B[(i + 1) * intp1 + (i + 1)] = 2.0 * a->tbd[i];
        a->B[(i + 0) * intp1 + (i + 1)] = 2.0 * a->tbg[i];
        a->B[(i + 1) * intp1 + (i + 0)] = 2.0 * a->tad[i];
        a->E[(i + 0) * intp1 + (i + 0)] += 2.0 * a->tgg[i];
        a->E[(i + 1) * intp1 + (i + 1)] = 2.0 * a->tdd[i];
        a->E[(i + 0) * intp1 + (i + 1)] = 2.0 * a->tgd[i];
        a->E[(i + 1) * intp1 + (i + 0)] = 2.0 * a->tgd[i];
    }
    for (i = 0; i < intm1; i++)
    {
        a->C[i * intp1 + (i + 0)] = +3.0 * a->h[i + 1] / a->h[i];
        a->C[i * intp1 + (i + 2)] = -3.0 * a->h[i] / a->h[i + 1];
        a->C[i * intp1 + (i + 1)] = -a->C[i * intp1 + (i + 0)] - a->C[i * intp1 + (i + 2)];
        a->F[i * intp1 + (i + 0)] = a->h[i + 1];
        a->F[i * intp1 + (i + 1)] = 2.0 * (a->h[i] + a->h[i + 1]);
        a->F[i * intp1 + (i + 2)] = a->h[i];
    }
    for (i = 0, k = 0; i < intp1; i++, k++)
    {
        for (j = 0, m = 0; j < intp1; j++, m++)
            a->MAT[k * nsize + m] = a->A[i * intp1 + j];
        for (j = 0, m = intp1; j < intp1; j++, m++)
            a->MAT[k * nsize + m] = a->B[j * intp1 + i];
        for (j = 0, m = 2 * intp1; j < intm1; j++, m++)
            a->MAT[k * nsize + m] = a->C[j * intp1 + i];
        a->RHS[k] = a->D[i];
    }
    for (i = 0, k = intp1; i < intp1; i++, k++)
    {
        for (j = 0, m = 0; j < intp1; j++, m++)
            a->MAT[k * nsize + m] = a->B[i * intp1 + j];
        for (j = 0, m = intp1; j < intp1; j++, m++)
            a->MAT[k * nsize + m] = a->E[i * intp1 + j];
        for (j = 0, m = 2 * intp1; j < intm1; j++, m++)
            a->MAT[k * nsize + m] = a->F[j * intp1 + i];
        a->RHS[k] = a->G[i];
    }
    for (i = 0, k = 2 * intp1; i < intm1; i++, k++)
    {
        for (j = 0, m = 0; j < intp1; j++, m++)
            a->MAT[k * nsize + m] = a->C[i * intp1 + j];
        for (j = 0, m = intp1; j < intp1; j++, m++)
            a->MAT[k * nsize + m] = a->F[i * intp1 + j];
        for (j = 0, m = 2 * intp1; j < intm1; j++, m++)
            a->MAT[k * nsize + m] = 0.0;
        a->RHS[k] = 0.0;
    }
    decomp(nsize, a->MAT, a->ipiv, &dinfo, a->wrk);
    dsolve(nsize, a->MAT, a->ipiv, a->RHS, a->SLN);
    if (dinfo != 0)
    {
        *info = dinfo;
        goto cleanup;
    }

    for (i = 0; i <= ints; i++)
    {
        a->z[i] = a->SLN[i];
        a->zp[i] = a->SLN[i + ints + 1];
    }
    for (i = 0; i < ints; i++)
    {
        c[4 * i + 0] = a->z[i];
        c[4 * i + 1] = a->zp[i];
        c[4 * i + 2] = -3.0 / (a->h[i] * a->h[i]) * (a->z[i] - a->z[i + 1]) - 1.0 / a->h[i] * (2.0 * a->zp[i] + a->zp[i + 1]);
        c[4 * i + 3] = 2.0 / (a->h[i] * a->h[i] * a->h[i]) * (a->z[i] - a->z[i + 1]) + 1.0 / (a->h[i] * a->h[i]) * (a->zp[i] + a->zp[i + 1]);
    }
cleanup:
    return;
}

} // namespace WDSP
