///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef DATVCONSTELLATION_H
#define DATVCONSTELLATION_H

#include <vector>

#include "leansdr/framework.h"
#include "gui/tvscreen.h"

namespace leansdr {

static const int DEFAULT_GUI_DECIMATION = 64;

static inline cstln_lut<eucl_ss, 256> * make_dvbs_constellation(cstln_lut<eucl_ss, 256>::predef c,
        code_rate r)
{
    float gamma1 = 1, gamma2 = 1, gamma3 = 1;
    switch (c)
    {
    case cstln_lut<eucl_ss, 256>::APSK16:
        // EN 302 307, section 5.4.3, Table 9
        switch (r)
        {
        case FEC23:
        case FEC46:
            gamma1 = 3.15;
            break;
        case FEC34:
            gamma1 = 2.85;
            break;
        case FEC45:
            gamma1 = 2.75;
            break;
        case FEC56:
            gamma1 = 2.70;
            break;
        case FEC89:
            gamma1 = 2.60;
            break;
        case FEC910:
            gamma1 = 2.57;
            break;
        default:
            fail("cstln_lut<256>::make_dvbs_constellation: Code rate not supported with APSK16");
            return 0;
        }
        break;
    case cstln_lut<eucl_ss, 256>::APSK32:
        // EN 302 307, section 5.4.4, Table 10
        switch (r)
        {
        case FEC34:
            gamma1 = 2.84;
            gamma2 = 5.27;
            break;
        case FEC45:
            gamma1 = 2.72;
            gamma2 = 4.87;
            break;
        case FEC56:
            gamma1 = 2.64;
            gamma2 = 4.64;
            break;
        case FEC89:
            gamma1 = 2.54;
            gamma2 = 4.33;
            break;
        case FEC910:
            gamma1 = 2.53;
            gamma2 = 4.30;
            break;
        default:
            fail("cstln_lut<eucl_ss, 256>::make_dvbs_constellation: Code rate not supported with APSK32");
            return 0;
        }
        break;
    case cstln_lut<eucl_ss, 256>::APSK64E:
        // EN 302 307-2, section 5.4.5, Table 13f
        gamma1 = 2.4;
        gamma2 = 4.3;
        gamma3 = 7;
        break;
    default:
        break;
    }
    cstln_lut<eucl_ss, 256> *newCstln =  new cstln_lut<eucl_ss, 256>(c, 10, gamma1, gamma2, gamma3);
    newCstln->m_rateCode = (int) r;
    newCstln->m_typeCode = (int) c;
    newCstln->m_setByModcod = false;
    return newCstln;
}

template<typename T> struct datvconstellation: runnable
{
    T xymin, xymax;
    unsigned long decimation;
    long pixels_per_frame;
    cstln_lut<eucl_ss, 256> **cstln;  // Optional ptr to optional constellation
    TVScreen *m_tvScreen;
    pipereader<std::complex<T> > in;
    unsigned long phase;
    std::vector<int> cstln_rows;
    std::vector<int> cstln_cols;

    datvconstellation(
            scheduler *sch,
            pipebuf<std::complex<T> > &_in,
            T _xymin,
            T _xymax,
            const char *_name = nullptr,
            TVScreen *tvScreen = nullptr) :
        runnable(sch, _name ? _name : _in.name),
        xymin(_xymin),
        xymax(_xymax),
        decimation(DEFAULT_GUI_DECIMATION),
        pixels_per_frame(1024),
        cstln(0),
        m_tvScreen(tvScreen),
        in(_in),
        phase(0)
    {
    }

    void run()
    {
        //Symbols
        while (in.readable() >= pixels_per_frame)
        {
            if ((!phase) && m_tvScreen)
            {
                m_tvScreen->resetImage();

                std::complex<T> *p = in.rd(), *pend = p + pixels_per_frame;

                for (; p < pend; ++p)
                {
                    m_tvScreen->selectRow(256 * (p->real() - xymin) / (xymax - xymin));
                    m_tvScreen->setDataColor(
                        256 - 256 * ((p->imag() - xymin) / (xymax - xymin)),
                        255, 0, 255);
                }

                if (cstln && (*cstln))
                {
                    // Plot constellation points
                    std::vector<int>::const_iterator row_it = cstln_rows.begin();
                    std::vector<int>::const_iterator col_it = cstln_cols.begin();

                    for (;(row_it != cstln_rows.end()) && (col_it != cstln_cols.end()); ++row_it, ++col_it)
                    {
                        m_tvScreen->selectRow(*row_it);
                        m_tvScreen->setDataColor(*col_it, 250, 250, 5);
                    }
                }

                m_tvScreen->renderImage(0);
            }

            in.read(pixels_per_frame);

            if (++phase >= decimation) {
                phase = 0;
            }
        }
    }

    void draw_begin()
    {
    }

    void calculate_cstln_points()
    {
        if (!(*cstln)) {
            return;
        }

        cstln_rows.clear();
        cstln_cols.clear();

        for (int i = 0; i < (*cstln)->nsymbols; ++i)
        {
            std::complex<signed char> *p = &(*cstln)->symbols[i];
            int x = 256 * (p->real() - xymin) / (xymax - xymin);
            int y = 256 - 256 * (p->imag() - xymin) / (xymax - xymin);

            for (int d = -4; d <= 4; ++d)
            {
                cstln_rows.push_back(x + d);
                cstln_cols.push_back(y);
                cstln_rows.push_back(x);
                cstln_cols.push_back(y + d);
            }
        }
    }
};

} // leansdr

#endif // DATVCONSTELLATION_H
