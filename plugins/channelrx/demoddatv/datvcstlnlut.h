///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef DATV_CSTLN_LUT_H
#define DATV_CSTLN_LUT_H

#include "leansdr/dvb.h"
#include "leansdr/framework.h"

namespace leansdr {

static cstln_lut<eucl_ss, 256> * make_dvbs_constellation(
    cstln_lut<eucl_ss, 256>::predef c,
    code_rate r
)
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

static cstln_lut<llr_ss, 256> * make_dvbs2_constellation(
    cstln_lut<llr_ss, 256>::predef c,
    code_rate r
)
{
    float gamma1 = 1, gamma2 = 1, gamma3 = 1;

    switch (c)
    {
    case cstln_lut<llr_ss, 256>::APSK16:
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
            fail("cstln_lut<256>::make_dvbs2_constellation: Code rate not supported with APSK16");
            return 0;
        }
        break;
    case cstln_lut<llr_ss, 256>::APSK32:
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
            fail("cstln_lut<llr_ss, 256>::make_dvbs2_constellation: Code rate not supported with APSK32");
            return 0;
        }
        break;
    case cstln_lut<llr_ss, 256>::APSK64E:
        // EN 302 307-2, section 5.4.5, Table 13f
        gamma1 = 2.4;
        gamma2 = 4.3;
        gamma3 = 7;
        break;
    default:
        break;
    }

    cstln_lut<llr_ss, 256> *newCstln = new cstln_lut<llr_ss, 256>(c, 10, gamma1, gamma2, gamma3);
    newCstln->m_rateCode = r < code_rate::FEC_COUNT ? r : -1;
    newCstln->m_typeCode = c < cstln_lut<llr_ss, 256>::predef::COUNT ? c : -1;
    newCstln->m_setByModcod = false;
    return newCstln;
}

}

#endif // DATV_CSTLN_LUT_H