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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "dvbs2.h"
#include "dvbt2fec.h"

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#include <emmintrin.h>
#define DVBS2_SSE2
#endif

namespace dvbs2 {

struct ModcodEntry
{
    Modulation m_modulation;
    CodeRate m_rate;
    double m_esn0;      //!< Table 13
    const char *m_name;
};

// Table 12, from MODCOD 1
const ModcodEntry S2Tables::m_modcods[28] = {
    {Modulation::QPSK, CodeRate::R1_4, -2.35, "QPSK 1/4"},
    {Modulation::QPSK, CodeRate::R1_3, -1.24, "QPSK 1/3"},
    {Modulation::QPSK, CodeRate::R2_5, -0.30, "QPSK 2/5"},
    {Modulation::QPSK, CodeRate::R1_2, 1.00, "QPSK 1/2"},
    {Modulation::QPSK, CodeRate::R3_5, 2.23, "QPSK 3/5"},
    {Modulation::QPSK, CodeRate::R2_3, 3.10, "QPSK 2/3"},
    {Modulation::QPSK, CodeRate::R3_4, 4.03, "QPSK 3/4"},
    {Modulation::QPSK, CodeRate::R4_5, 4.68, "QPSK 4/5"},
    {Modulation::QPSK, CodeRate::R5_6, 5.18, "QPSK 5/6"},
    {Modulation::QPSK, CodeRate::R8_9, 6.20, "QPSK 8/9"},
    {Modulation::QPSK, CodeRate::R9_10, 6.42, "QPSK 9/10"},
    {Modulation::PSK8, CodeRate::R3_5, 5.50, "8PSK 3/5"},
    {Modulation::PSK8, CodeRate::R2_3, 6.62, "8PSK 2/3"},
    {Modulation::PSK8, CodeRate::R3_4, 7.91, "8PSK 3/4"},
    {Modulation::PSK8, CodeRate::R5_6, 9.35, "8PSK 5/6"},
    {Modulation::PSK8, CodeRate::R8_9, 10.69, "8PSK 8/9"},
    {Modulation::PSK8, CodeRate::R9_10, 10.98, "8PSK 9/10"},
    {Modulation::APSK16, CodeRate::R2_3, 8.97, "16APSK 2/3"},
    {Modulation::APSK16, CodeRate::R3_4, 10.21, "16APSK 3/4"},
    {Modulation::APSK16, CodeRate::R4_5, 11.03, "16APSK 4/5"},
    {Modulation::APSK16, CodeRate::R5_6, 11.61, "16APSK 5/6"},
    {Modulation::APSK16, CodeRate::R8_9, 12.89, "16APSK 8/9"},
    {Modulation::APSK16, CodeRate::R9_10, 13.13, "16APSK 9/10"},
    {Modulation::APSK32, CodeRate::R3_4, 12.73, "32APSK 3/4"},
    {Modulation::APSK32, CodeRate::R4_5, 13.64, "32APSK 4/5"},
    {Modulation::APSK32, CodeRate::R5_6, 14.28, "32APSK 5/6"},
    {Modulation::APSK32, CodeRate::R8_9, 15.69, "32APSK 8/9"},
    {Modulation::APSK32, CodeRate::R9_10, 16.05, "32APSK 9/10"},
};

struct S2XModcodEntry
{
    int m_pls;                  //!< The PLS code, pilots off
    Modulation m_modulation;
    CodeRate m_rate;            //!< The LDPC code's (the implementation MODCOD's)
    FECType m_type;
    double m_esn0;              //!< EN 302 307-2 tables 20a and 20b
    const char *m_name;         //!< Canonical
};

// EN 302 307-2 table 17a, but VL-SNR's (PLS codes 129 and 131, whose MODCODs their VL-SNR header says: vlsnrModcod)
const S2XModcodEntry S2Tables::m_s2xModcods[] = {
    {132, Modulation::QPSK, CodeRate::R13_45, FECType::Normal, -2.03, "QPSK 13/45"},
    {134, Modulation::QPSK, CodeRate::R9_20, FECType::Normal, 0.22, "QPSK 9/20"},
    {136, Modulation::QPSK, CodeRate::R11_20, FECType::Normal, 1.45, "QPSK 11/20"},
    {138, Modulation::APSK8, CodeRate::R100_180, FECType::Normal, 4.73, "8APSK 5/9-L"},
    {140, Modulation::APSK8, CodeRate::R104_180, FECType::Normal, 5.13, "8APSK 26/45-L"},
    {142, Modulation::PSK8, CodeRate::R23_36, FECType::Normal, 6.12, "8PSK 23/36"},
    {144, Modulation::PSK8, CodeRate::R25_36, FECType::Normal, 7.02, "8PSK 25/36"},
    {146, Modulation::PSK8, CodeRate::R13_18, FECType::Normal, 7.49, "8PSK 13/18"},
    {148, Modulation::APSK16, CodeRate::R90_180, FECType::Normal, 5.97, "16APSK 1/2-L"},
    {150, Modulation::APSK16, CodeRate::R96_180, FECType::Normal, 6.55, "16APSK 8/15-L"},
    {152, Modulation::APSK16, CodeRate::R100_180, FECType::Normal, 6.84, "16APSK 5/9-L"},
    {154, Modulation::APSK16, CodeRate::R26_45, FECType::Normal, 7.51, "16APSK 26/45"},
    {156, Modulation::APSK16, CodeRate::R3_5, FECType::Normal, 7.80, "16APSK 3/5"},
    {158, Modulation::APSK16, CodeRate::R18_30, FECType::Normal, 7.41, "16APSK 3/5-L"},
    {160, Modulation::APSK16, CodeRate::R28_45, FECType::Normal, 8.10, "16APSK 28/45"},
    {162, Modulation::APSK16, CodeRate::R23_36, FECType::Normal, 8.38, "16APSK 23/36"},
    {164, Modulation::APSK16, CodeRate::R20_30, FECType::Normal, 8.43, "16APSK 2/3-L"},
    {166, Modulation::APSK16, CodeRate::R25_36, FECType::Normal, 9.27, "16APSK 25/36"},
    {168, Modulation::APSK16, CodeRate::R13_18, FECType::Normal, 9.71, "16APSK 13/18"},
    {170, Modulation::APSK16, CodeRate::R140_180, FECType::Normal, 10.65, "16APSK 7/9"},
    {172, Modulation::APSK16, CodeRate::R154_180, FECType::Normal, 11.99, "16APSK 77/90"},
    {174, Modulation::APSK32, CodeRate::R2_3, FECType::Normal, 11.10, "32APSK 2/3-L"},
    {178, Modulation::APSK32, CodeRate::R128_180, FECType::Normal, 11.75, "32APSK 32/45"},
    {180, Modulation::APSK32, CodeRate::R132_180, FECType::Normal, 12.17, "32APSK 11/15"},
    {182, Modulation::APSK32, CodeRate::R140_180, FECType::Normal, 13.05, "32APSK 7/9"},
    {184, Modulation::APSK64, CodeRate::R128_180, FECType::Normal, 13.98, "64APSK 32/45-L"},
    {186, Modulation::APSK64, CodeRate::R132_180, FECType::Normal, 14.81, "64APSK 11/15"},
    {190, Modulation::APSK64, CodeRate::R7_9, FECType::Normal, 15.47, "64APSK 7/9"},
    {194, Modulation::APSK64, CodeRate::R4_5, FECType::Normal, 15.87, "64APSK 4/5"},
    {198, Modulation::APSK64, CodeRate::R5_6, FECType::Normal, 16.55, "64APSK 5/6"},
    {200, Modulation::APSK128, CodeRate::R135_180, FECType::Normal, 17.73, "128APSK 3/4"},
    {202, Modulation::APSK128, CodeRate::R140_180, FECType::Normal, 18.53, "128APSK 7/9"},
    {204, Modulation::APSK256, CodeRate::R116_180, FECType::Normal, 16.98, "256APSK 29/45-L"},
    {206, Modulation::APSK256, CodeRate::R20_30, FECType::Normal, 17.24, "256APSK 2/3-L"},
    {208, Modulation::APSK256, CodeRate::R124_180, FECType::Normal, 18.10, "256APSK 31/45-L"},
    {210, Modulation::APSK256, CodeRate::R128_180, FECType::Normal, 18.59, "256APSK 32/45"},
    {212, Modulation::APSK256, CodeRate::R22_30, FECType::Normal, 18.84, "256APSK 11/15-L"},
    {214, Modulation::APSK256, CodeRate::R135_180, FECType::Normal, 19.57, "256APSK 3/4"},
    {216, Modulation::QPSK, CodeRate::R11_45, FECType::Short, -2.50, "QPSK 11/45"},
    {218, Modulation::QPSK, CodeRate::R4_15, FECType::Short, -2.24, "QPSK 4/15"},
    {220, Modulation::QPSK, CodeRate::R14_45, FECType::Short, -1.46, "QPSK 14/45"},
    {222, Modulation::QPSK, CodeRate::R7_15, FECType::Short, 0.60, "QPSK 7/15"},
    {224, Modulation::QPSK, CodeRate::R8_15, FECType::Short, 1.45, "QPSK 8/15"},
    {226, Modulation::QPSK, CodeRate::R32_45, FECType::Short, 3.66, "QPSK 32/45"},
    {228, Modulation::PSK8, CodeRate::R7_15, FECType::Short, 3.83, "8PSK 7/15"},
    {230, Modulation::PSK8, CodeRate::R8_15, FECType::Short, 4.71, "8PSK 8/15"},
    {232, Modulation::PSK8, CodeRate::R26_45, FECType::Short, 5.52, "8PSK 26/45"},
    {234, Modulation::PSK8, CodeRate::R32_45, FECType::Short, 7.54, "8PSK 32/45"},
    {236, Modulation::APSK16, CodeRate::R7_15, FECType::Short, 5.99, "16APSK 7/15"},
    {238, Modulation::APSK16, CodeRate::R8_15, FECType::Short, 6.93, "16APSK 8/15"},
    {240, Modulation::APSK16, CodeRate::R26_45, FECType::Short, 7.66, "16APSK 26/45"},
    {242, Modulation::APSK16, CodeRate::R3_5, FECType::Short, 8.10, "16APSK 3/5"},
    {244, Modulation::APSK16, CodeRate::R32_45, FECType::Short, 9.81, "16APSK 32/45"},
    {246, Modulation::APSK32, CodeRate::R2_3, FECType::Short, 11.41, "32APSK 2/3"},
    {248, Modulation::APSK32, CodeRate::R32_45, FECType::Short, 12.18, "32APSK 32/45"},
};

const S2XModcodEntry *S2Tables::s2xEntry(int modcod)
{
    if (!isS2X(modcod) || (modcod >= nbModcods)) {
        return nullptr;
    }

    const int pls = 128 + 2 * (modcod - s2xModcodBase);

    for (const S2XModcodEntry& entry : m_s2xModcods)
    {
        if (entry.m_pls == pls) {
            return &entry;
        }
    }

    return nullptr;
}

// Table 11e, 8+8APSK 18/30
const float S2Tables::m_apsk16_18_30[16][2] = {
    {0.4718f, 0.2606f}, {0.2606f, 0.4718f}, {-0.4718f, 0.2606f}, {-0.2606f, 0.4718f},
    {0.4718f, -0.2606f}, {0.2606f, -0.4718f}, {-0.4718f, -0.2606f}, {-0.2606f, -0.4718f},
    {1.2088f, 0.4984f}, {0.4984f, 1.2088f}, {-1.2088f, 0.4984f}, {-0.4984f, 1.2088f},
    {1.2088f, -0.4984f}, {0.4984f, -1.2088f}, {-1.2088f, -0.4984f}, {-0.4984f, -1.2088f},
};

// Table 11e, 8+8APSK 20/30
const float S2Tables::m_apsk16_20_30[16][2] = {
    {0.5061f, 0.2474f}, {0.2474f, 0.5061f}, {-0.5061f, 0.2474f}, {-0.2474f, 0.5061f},
    {0.5061f, -0.2474f}, {0.2474f, -0.5061f}, {-0.5061f, -0.2474f}, {-0.2474f, -0.5061f},
    {1.2007f, 0.4909f}, {0.4909f, 1.2007f}, {-1.2007f, 0.4909f}, {-0.4909f, 1.2007f},
    {1.2007f, -0.4909f}, {0.4909f, -1.2007f}, {-1.2007f, -0.4909f}, {-0.4909f, -1.2007f},
};

// Table 15d, 256APSK 20/30
const float S2Tables::m_apsk256_20_30[256][2] = {
    {1.635f, 0.1593f}, {1.5776f, 0.4735f}, {0.943f, 0.11f}, {0.9069f, 0.2829f},
    {0.3237f, 0.0849f}, {0.3228f, 0.0867f}, {0.7502f, 0.1138f}, {0.7325f, 0.2088f},
    {0.1658f, 1.6747f}, {0.4907f, 1.6084f}, {0.1088f, 0.953f}, {0.2464f, 0.927f},
    {0.0872f, 0.139f}, {0.0871f, 0.1392f}, {0.1091f, 0.7656f}, {0.1699f, 0.7537f},
    {-1.635f, 0.1593f}, {-1.5776f, 0.4735f}, {-0.943f, 0.11f}, {-0.9069f, 0.2829f},
    {-0.3237f, 0.0849f}, {-0.3228f, 0.0867f}, {-0.7502f, 0.1138f}, {-0.7325f, 0.2088f},
    {-0.1658f, 1.6747f}, {-0.4907f, 1.6084f}, {-0.1088f, 0.953f}, {-0.2464f, 0.927f},
    {-0.0872f, 0.139f}, {-0.0871f, 0.1392f}, {-0.1091f, 0.7656f}, {-0.1699f, 0.7537f},
    {1.3225f, 0.132f}, {1.2742f, 0.3922f}, {1.0854f, 0.1139f}, {1.0441f, 0.3296f},
    {0.4582f, 0.1123f}, {0.4545f, 0.1251f}, {0.6473f, 0.1138f}, {0.6339f, 0.1702f},
    {0.1322f, 1.3631f}, {0.3929f, 1.3102f}, {0.1124f, 1.1327f}, {0.316f, 1.0913f},
    {0.0928f, 0.397f}, {0.0937f, 0.3973f}, {0.1054f, 0.5979f}, {0.123f, 0.5949f},
    {-1.3225f, 0.132f}, {-1.2742f, 0.3922f}, {-1.0854f, 0.1139f}, {-1.0441f, 0.3296f},
    {-0.4582f, 0.1123f}, {-0.4545f, 0.1251f}, {-0.6473f, 0.1138f}, {-0.6339f, 0.1702f},
    {-0.1322f, 1.3631f}, {-0.3929f, 1.3102f}, {-0.1124f, 1.1327f}, {-0.316f, 1.0913f},
    {-0.0928f, 0.397f}, {-0.0937f, 0.3973f}, {-0.1054f, 0.5979f}, {-0.123f, 0.5949f},
    {1.635f, -0.1593f}, {1.5776f, -0.4735f}, {0.943f, -0.11f}, {0.9069f, -0.2829f},
    {0.3237f, -0.0849f}, {0.3228f, -0.0867f}, {0.7502f, -0.1138f}, {0.7325f, -0.2088f},
    {0.1658f, -1.6747f}, {0.4907f, -1.6084f}, {0.1088f, -0.953f}, {0.2464f, -0.927f},
    {0.0872f, -0.139f}, {0.0871f, -0.1392f}, {0.1091f, -0.7656f}, {0.1699f, -0.7537f},
    {-1.635f, -0.1593f}, {-1.5776f, -0.4735f}, {-0.943f, -0.11f}, {-0.9069f, -0.2829f},
    {-0.3237f, -0.0849f}, {-0.3228f, -0.0867f}, {-0.7502f, -0.1138f}, {-0.7325f, -0.2088f},
    {-0.1658f, -1.6747f}, {-0.4907f, -1.6084f}, {-0.1088f, -0.953f}, {-0.2464f, -0.927f},
    {-0.0872f, -0.139f}, {-0.0871f, -0.1392f}, {-0.1091f, -0.7656f}, {-0.1699f, -0.7537f},
    {1.3225f, -0.132f}, {1.2742f, -0.3922f}, {1.0854f, -0.1139f}, {1.0441f, -0.3296f},
    {0.4582f, -0.1123f}, {0.4545f, -0.1251f}, {0.6473f, -0.1138f}, {0.6339f, -0.1702f},
    {0.1322f, -1.3631f}, {0.3929f, -1.3102f}, {0.1124f, -1.1327f}, {0.316f, -1.0913f},
    {0.0928f, -0.397f}, {0.0937f, -0.3973f}, {0.1054f, -0.5979f}, {0.123f, -0.5949f},
    {-1.3225f, -0.132f}, {-1.2742f, -0.3922f}, {-1.0854f, -0.1139f}, {-1.0441f, -0.3296f},
    {-0.4582f, -0.1123f}, {-0.4545f, -0.1251f}, {-0.6473f, -0.1138f}, {-0.6339f, -0.1702f},
    {-0.1322f, -1.3631f}, {-0.3929f, -1.3102f}, {-0.1124f, -1.1327f}, {-0.316f, -1.0913f},
    {-0.0928f, -0.397f}, {-0.0937f, -0.3973f}, {-0.1054f, -0.5979f}, {-0.123f, -0.5949f},
    {1.2901f, 1.0495f}, {1.4625f, 0.774f}, {0.7273f, 0.616f}, {0.8177f, 0.4841f},
    {0.2844f, 0.1296f}, {0.2853f, 0.1309f}, {0.5902f, 0.4857f}, {0.6355f, 0.4185f},
    {1.0646f, 1.2876f}, {0.7949f, 1.4772f}, {0.5707f, 0.7662f}, {0.449f, 0.8461f},
    {0.1053f, 0.1494f}, {0.1052f, 0.1495f}, {0.4294f, 0.6363f}, {0.3744f, 0.6744f},
    {-1.2901f, 1.0495f}, {-1.4625f, 0.774f}, {-0.7273f, 0.616f}, {-0.8177f, 0.4841f},
    {-0.2844f, 0.1296f}, {-0.2853f, 0.1309f}, {-0.5902f, 0.4857f}, {-0.6355f, 0.4185f},
    {-1.0646f, 1.2876f}, {-0.7949f, 1.4772f}, {-0.5707f, 0.7662f}, {-0.449f, 0.8461f},
    {-0.1053f, 0.1494f}, {-0.1052f, 0.1495f}, {-0.4294f, 0.6363f}, {-0.3744f, 0.6744f},
    {1.0382f, 0.8623f}, {1.1794f, 0.6376f}, {0.8504f, 0.7217f}, {0.9638f, 0.5407f},
    {0.3734f, 0.256f}, {0.3799f, 0.2517f}, {0.4968f, 0.3947f}, {0.5231f, 0.3644f},
    {0.8555f, 1.0542f}, {0.6363f, 1.2064f}, {0.6961f, 0.885f}, {0.5229f, 1.0037f},
    {0.1938f, 0.3621f}, {0.1909f, 0.3627f}, {0.3224f, 0.5236f}, {0.3016f, 0.5347f},
    {-1.0382f, 0.8623f}, {-1.1794f, 0.6376f}, {-0.8504f, 0.7217f}, {-0.9638f, 0.5407f},
    {-0.3734f, 0.256f}, {-0.3799f, 0.2517f}, {-0.4968f, 0.3947f}, {-0.5231f, 0.3644f},
    {-0.8555f, 1.0542f}, {-0.6363f, 1.2064f}, {-0.6961f, 0.885f}, {-0.5229f, 1.0037f},
    {-0.1938f, 0.3621f}, {-0.1909f, 0.3627f}, {-0.3224f, 0.5236f}, {-0.3016f, 0.5347f},
    {1.2901f, -1.0495f}, {1.4625f, -0.774f}, {0.7273f, -0.616f}, {0.8177f, -0.4841f},
    {0.2844f, -0.1296f}, {0.2853f, -0.1309f}, {0.5902f, -0.4857f}, {0.6355f, -0.4185f},
    {1.0646f, -1.2876f}, {0.7949f, -1.4772f}, {0.5707f, -0.7662f}, {0.449f, -0.8461f},
    {0.1053f, -0.1494f}, {0.1052f, -0.1495f}, {0.4294f, -0.6363f}, {0.3744f, -0.6744f},
    {-1.2901f, -1.0495f}, {-1.4625f, -0.774f}, {-0.7273f, -0.616f}, {-0.8177f, -0.4841f},
    {-0.2844f, -0.1296f}, {-0.2853f, -0.1309f}, {-0.5902f, -0.4857f}, {-0.6355f, -0.4185f},
    {-1.0646f, -1.2876f}, {-0.7949f, -1.4772f}, {-0.5707f, -0.7662f}, {-0.449f, -0.8461f},
    {-0.1053f, -0.1494f}, {-0.1052f, -0.1495f}, {-0.4294f, -0.6363f}, {-0.3744f, -0.6744f},
    {1.0382f, -0.8623f}, {1.1794f, -0.6376f}, {0.8504f, -0.7217f}, {0.9638f, -0.5407f},
    {0.3734f, -0.256f}, {0.3799f, -0.2517f}, {0.4968f, -0.3947f}, {0.5231f, -0.3644f},
    {0.8555f, -1.0542f}, {0.6363f, -1.2064f}, {0.6961f, -0.885f}, {0.5229f, -1.0037f},
    {0.1938f, -0.3621f}, {0.1909f, -0.3627f}, {0.3224f, -0.5236f}, {0.3016f, -0.5347f},
    {-1.0382f, -0.8623f}, {-1.1794f, -0.6376f}, {-0.8504f, -0.7217f}, {-0.9638f, -0.5407f},
    {-0.3734f, -0.256f}, {-0.3799f, -0.2517f}, {-0.4968f, -0.3947f}, {-0.5231f, -0.3644f},
    {-0.8555f, -1.0542f}, {-0.6363f, -1.2064f}, {-0.6961f, -0.885f}, {-0.5229f, -1.0037f},
    {-0.1938f, -0.3621f}, {-0.1909f, -0.3627f}, {-0.3224f, -0.5236f}, {-0.3016f, -0.5347f},
};

// Table 15d, 256APSK 22/30
const float S2Tables::m_apsk256_22_30[256][2] = {
    {1.5977f, 0.1526f}, {1.3187f, 0.1269f}, {-1.5977f, 0.1526f}, {-1.3187f, 0.1269f},
    {0.2574f, 0.0733f}, {0.4496f, 0.0807f}, {-0.2574f, 0.0733f}, {-0.4496f, 0.0807f},
    {1.5977f, -0.1526f}, {1.3187f, -0.1269f}, {-1.5977f, -0.1526f}, {-1.3187f, -0.1269f},
    {0.2574f, -0.0733f}, {0.4496f, -0.0807f}, {-0.2574f, -0.0733f}, {-0.4496f, -0.0807f},
    {0.9269f, 0.0943f}, {1.1024f, 0.1086f}, {-0.9269f, 0.0943f}, {-1.1024f, 0.1086f},
    {0.7663f, 0.0867f}, {0.6115f, 0.0871f}, {-0.7663f, 0.0867f}, {-0.6115f, 0.0871f},
    {0.9269f, -0.0943f}, {1.1024f, -0.1086f}, {-0.9269f, -0.0943f}, {-1.1024f, -0.1086f},
    {0.7663f, -0.0867f}, {0.6115f, -0.0871f}, {-0.7663f, -0.0867f}, {-0.6115f, -0.0871f},
    {1.2701f, 1.0139f}, {1.0525f, 0.8406f}, {-1.2701f, 1.0139f}, {-1.0525f, 0.8406f},
    {0.2487f, 0.1978f}, {0.3523f, 0.2915f}, {-0.2487f, 0.1978f}, {-0.3523f, 0.2915f},
    {1.2701f, -1.0139f}, {1.0525f, -0.8406f}, {-1.2701f, -1.0139f}, {-1.0525f, -0.8406f},
    {0.2487f, -0.1978f}, {0.3523f, -0.2915f}, {-0.2487f, -0.1978f}, {-0.3523f, -0.2915f},
    {0.7359f, 0.6043f}, {0.8807f, 0.7105f}, {-0.7359f, 0.6043f}, {-0.8807f, 0.7105f},
    {0.6017f, 0.5019f}, {0.4747f, 0.3996f}, {-0.6017f, 0.5019f}, {-0.4747f, 0.3996f},
    {0.7359f, -0.6043f}, {0.8807f, -0.7105f}, {-0.7359f, -0.6043f}, {-0.8807f, -0.7105f},
    {0.6017f, -0.5019f}, {0.4747f, -0.3996f}, {-0.6017f, -0.5019f}, {-0.4747f, -0.3996f},
    {1.5441f, 0.4545f}, {1.275f, 0.3775f}, {-1.5441f, 0.4545f}, {-1.275f, 0.3775f},
    {0.2586f, 0.0752f}, {0.4435f, 0.1065f}, {-0.2586f, 0.0752f}, {-0.4435f, 0.1065f},
    {1.5441f, -0.4545f}, {1.275f, -0.3775f}, {-1.5441f, -0.4545f}, {-1.275f, -0.3775f},
    {0.2586f, -0.0752f}, {0.4435f, -0.1065f}, {-0.2586f, -0.0752f}, {-0.4435f, -0.1065f},
    {0.8925f, 0.2771f}, {1.0649f, 0.3219f}, {-0.8925f, 0.2771f}, {-1.0649f, 0.3219f},
    {0.7362f, 0.2279f}, {0.5936f, 0.1699f}, {-0.7362f, 0.2279f}, {-0.5936f, 0.1699f},
    {0.8925f, -0.2771f}, {1.0649f, -0.3219f}, {-0.8925f, -0.2771f}, {-1.0649f, -0.3219f},
    {0.7362f, -0.2279f}, {0.5936f, -0.1699f}, {-0.7362f, -0.2279f}, {-0.5936f, -0.1699f},
    {1.4352f, 0.7452f}, {1.1866f, 0.6182f}, {-1.4352f, 0.7452f}, {-1.1866f, 0.6182f},
    {0.2523f, 0.1944f}, {0.3695f, 0.2695f}, {-0.2523f, 0.1944f}, {-0.3695f, 0.2695f},
    {1.4352f, -0.7452f}, {1.1866f, -0.6182f}, {-1.4352f, -0.7452f}, {-1.1866f, -0.6182f},
    {0.2523f, -0.1944f}, {0.3695f, -0.2695f}, {-0.2523f, -0.1944f}, {-0.3695f, -0.2695f},
    {0.8273f, 0.4493f}, {0.9911f, 0.5243f}, {-0.8273f, 0.4493f}, {-0.9911f, 0.5243f},
    {0.6708f, 0.3859f}, {0.5197f, 0.3331f}, {-0.6708f, 0.3859f}, {-0.5197f, 0.3331f},
    {0.8273f, -0.4493f}, {0.9911f, -0.5243f}, {-0.8273f, -0.4493f}, {-0.9911f, -0.5243f},
    {0.6708f, -0.3859f}, {0.5197f, -0.3331f}, {-0.6708f, -0.3859f}, {-0.5197f, -0.3331f},
    {0.1646f, 1.6329f}, {0.1379f, 1.3595f}, {-0.1646f, 1.6329f}, {-0.1379f, 1.3595f},
    {0.0736f, 0.0898f}, {0.0742f, 0.5054f}, {-0.0736f, 0.0898f}, {-0.0742f, 0.5054f},
    {0.1646f, -1.6329f}, {0.1379f, -1.3595f}, {-0.1646f, -1.6329f}, {-0.1379f, -1.3595f},
    {0.0736f, -0.0898f}, {0.0742f, -0.5054f}, {-0.0736f, -0.0898f}, {-0.0742f, -0.5054f},
    {0.0992f, 0.9847f}, {0.117f, 1.1517f}, {-0.0992f, 0.9847f}, {-0.117f, 1.1517f},
    {0.0894f, 0.8287f}, {0.0889f, 0.6739f}, {-0.0894f, 0.8287f}, {-0.0889f, 0.6739f},
    {0.0992f, -0.9847f}, {0.117f, -1.1517f}, {-0.0992f, -0.9847f}, {-0.117f, -1.1517f},
    {0.0894f, -0.8287f}, {0.0889f, -0.6739f}, {-0.0894f, -0.8287f}, {-0.0889f, -0.6739f},
    {1.0516f, 1.2481f}, {0.8742f, 1.0355f}, {-1.0516f, 1.2481f}, {-0.8742f, 1.0355f},
    {0.097f, 0.245f}, {0.1959f, 0.4045f}, {-0.097f, 0.245f}, {-0.1959f, 0.4045f},
    {1.0516f, -1.2481f}, {0.8742f, -1.0355f}, {-1.0516f, -1.2481f}, {-0.8742f, -1.0355f},
    {0.097f, -0.245f}, {0.1959f, -0.4045f}, {-0.097f, -0.245f}, {-0.1959f, -0.4045f},
    {0.615f, 0.7441f}, {0.7345f, 0.8743f}, {-0.615f, 0.7441f}, {-0.7345f, 0.8743f},
    {0.4932f, 0.6301f}, {0.362f, 0.5258f}, {-0.4932f, 0.6301f}, {-0.362f, 0.5258f},
    {0.615f, -0.7441f}, {0.7345f, -0.8743f}, {-0.615f, -0.7441f}, {-0.7345f, -0.8743f},
    {0.4932f, -0.6301f}, {0.362f, -0.5258f}, {-0.4932f, -0.6301f}, {-0.362f, -0.5258f},
    {0.4866f, 1.566f}, {0.4068f, 1.3027f}, {-0.4866f, 1.566f}, {-0.4068f, 1.3027f},
    {0.0732f, 0.0899f}, {0.0877f, 0.4997f}, {-0.0732f, 0.0899f}, {-0.0877f, 0.4997f},
    {0.4866f, -1.566f}, {0.4068f, -1.3027f}, {-0.4866f, -1.566f}, {-0.4068f, -1.3027f},
    {0.0732f, -0.0899f}, {0.0877f, -0.4997f}, {-0.0732f, -0.0899f}, {-0.0877f, -0.4997f},
    {0.2927f, 0.9409f}, {0.3446f, 1.1023f}, {-0.2927f, 0.9409f}, {-0.3446f, 1.1023f},
    {0.235f, 0.7945f}, {0.167f, 0.6529f}, {-0.235f, 0.7945f}, {-0.167f, 0.6529f},
    {0.2927f, -0.9409f}, {0.3446f, -1.1023f}, {-0.2927f, -0.9409f}, {-0.3446f, -1.1023f},
    {0.235f, -0.7945f}, {0.167f, -0.6529f}, {-0.235f, -0.7945f}, {-0.167f, -0.6529f},
    {0.7867f, 1.4356f}, {0.6561f, 1.1927f}, {-0.7867f, 1.4356f}, {-0.6561f, 1.1927f},
    {0.0947f, 0.2451f}, {0.1865f, 0.4121f}, {-0.0947f, 0.2451f}, {-0.1865f, 0.4121f},
    {0.7867f, -1.4356f}, {0.6561f, -1.1927f}, {-0.7867f, -1.4356f}, {-0.6561f, -1.1927f},
    {0.0947f, -0.2451f}, {0.1865f, -0.4121f}, {-0.0947f, -0.2451f}, {-0.1865f, -0.4121f},
    {0.4677f, 0.8579f}, {0.5537f, 1.0081f}, {-0.4677f, 0.8579f}, {-0.5537f, 1.0081f},
    {0.3893f, 0.7143f}, {0.311f, 0.5686f}, {-0.3893f, 0.7143f}, {-0.311f, 0.5686f},
    {0.4677f, -0.8579f}, {0.5537f, -1.0081f}, {-0.4677f, -0.8579f}, {-0.5537f, -1.0081f},
    {0.3893f, -0.7143f}, {0.311f, -0.5686f}, {-0.3893f, -0.7143f}, {-0.311f, -0.5686f},
};

const char *toString(Modulation modulation)
{
    switch (modulation)
    {
    case Modulation::PSK8: return "8PSK";
    case Modulation::APSK8: return "8APSK";
    case Modulation::APSK16: return "16APSK";
    case Modulation::APSK32: return "32APSK";
    case Modulation::APSK64: return "64APSK";
    case Modulation::APSK128: return "128APSK";
    case Modulation::APSK256: return "256APSK";
    case Modulation::BPSK: return "pi/2 BPSK";
    default: return "QPSK";
    }
}

int bitsPerSymbol(Modulation modulation)
{
    switch (modulation)
    {
    case Modulation::PSK8:
    case Modulation::APSK8: return 3;
    case Modulation::APSK16: return 4;
    case Modulation::APSK32: return 5;
    case Modulation::APSK64: return 6;
    case Modulation::APSK128: return 7;
    case Modulation::APSK256: return 8;
    case Modulation::BPSK: return 1;
    default: return 2;
    }
}

bool modcodInfo(int modcod, Modulation& modulation, CodeRate& rate)
{
    if ((modcod >= 1) && (modcod <= 28))
    {
        modulation = S2Tables::m_modcods[modcod - 1].m_modulation;
        rate = S2Tables::m_modcods[modcod - 1].m_rate;
        return true;
    }

    const S2XModcodEntry *entry = S2Tables::s2xEntry(modcod);

    if (!entry) {
        return false;
    }

    modulation = entry->m_modulation;
    rate = entry->m_rate;
    return true;
}

int modcod(Modulation modulation, CodeRate rate, FECType type)
{
    for (int i = 0; i < 28; i++)
    {
        if ((S2Tables::m_modcods[i].m_modulation == modulation) && (S2Tables::m_modcods[i].m_rate == rate) && validModcod(i + 1, type)) {
            return i + 1;
        }
    }

    for (const S2XModcodEntry& entry : S2Tables::m_s2xModcods)
    {
        if ((entry.m_modulation == modulation) && (entry.m_rate == rate) && (entry.m_type == type)) {
            return s2xModcodBase + ((entry.m_pls - 128) >> 1);
        }
    }

    return 0;
}

const char *modcodString(int modcod)
{
    if (modcod == 0) {
        return "Dummy";
    }

    if ((modcod >= 1) && (modcod <= 28)) {
        return S2Tables::m_modcods[modcod - 1].m_name;
    }

    const S2XModcodEntry *entry = S2Tables::s2xEntry(modcod);
    return entry ? entry->m_name : "Reserved";
}

bool fixedFECType(int modcod, FECType& type)
{
    if (!isS2X(modcod) || (modcod >= nbModcods)) {
        return false;
    }

    // Table 17a: PLS codes 216 to 249 are short frames' (table 17b's reserved ones are all normal)
    const int pls = 128 + 2 * (modcod - s2xModcodBase);
    type = (pls >= 216) && (pls < 250) ? FECType::Short : FECType::Normal;
    return true;
}

FECType modcodFECType(int modcod, FECType type)
{
    FECType fixed;
    return fixedFECType(modcod, fixed) ? fixed : type;
}

bool validModcod(int modcod, FECType type)
{
    Modulation modulation;
    CodeRate rate;

    if (!modcodInfo(modcod, modulation, rate)) {
        return false;
    }

    FECType fixed;

    if (fixedFECType(modcod, fixed)) {
        return fixed == type;
    }

    // There are no short frames at 9/10
    return (type == FECType::Normal) || (rate != CodeRate::R9_10);
}

int plsCode(int modcod, int type)
{
    if (isS2X(modcod)) {
        return 128 | (((modcod - s2xModcodBase) & 63) << 1) | (type & 1);
    }

    return ((modcod & 31) << 2) | (type & 3);
}

int slotCount(Modulation modulation, FECType type)
{
    // 128APSK's codeword is padded (6 zeros, then 84 ones after the interleaver) to a whole number of slots
    const int bits = dvbt2::nldpc(type) + (modulation == Modulation::APSK128 ? 90 : 0);
    return bits / bitsPerSymbol(modulation) / slotSymbols;
}

int pilotBlocks(int nbSlots, bool pilots)
{
    return pilots ? (nbSlots - 1) / pilotInterval : 0;
}

int plFrameSymbols(int modcod, FECType type, bool pilots)
{
    if (modcod == dummyModcod) {
        return headerSymbols + dummySlots * slotSymbols;
    }

    Modulation modulation;
    CodeRate rate;
    type = modcodFECType(modcod, type);

    if (modcodInfo(modcod, modulation, rate))
    {
        if (!validModcod(modcod, type)) {
            return 0;
        }

        const int s = slotCount(modulation, type);
        return slotSymbols * (s + 1) + pilotSymbols * pilotBlocks(s, pilots);
    }

    if (!isS2X(modcod)) {
        return 0;
    }

    // EN 302 307-2 table 17b: reserved PLS codes, of known lengths (for the last 6, b7 isn't the pilots)
    switch (plsCode(modcod, pilots ? 1 : 0))
    {
    case 128: return 21690;
    case 129: return vlsnrFrameSymbols(1);
    case 130: return 16290;
    case 131: return vlsnrFrameSymbols(2);
    case 176: return 13050;
    case 177: return 13338;
    case 188:
    case 192:
    case 196: return 10890;
    case 189:
    case 193:
    case 197: return 11142;
    case 250: return 22194;
    case 251: return 16686;
    case 252: return 13338;
    case 253: return 11142;
    case 254: return 8370;
    case 255: return 6714;
    default: return 0;
    }
}

double idealEsN0(int modcod)
{
    if ((modcod >= 1) && (modcod <= 28)) {
        return S2Tables::m_modcods[modcod - 1].m_esn0;
    }

    const S2XModcodEntry *entry = S2Tables::s2xEntry(modcod);
    return entry ? entry->m_esn0 : 0.0;
}

double usefulBitRate(double symbolRate, int modcod, FECType type, bool pilots)
{
    Modulation modulation;
    CodeRate rate;

    if (!modcodInfo(modcod, modulation, rate)) {
        return 0.0;
    }

    type = modcodFECType(modcod, type);
    const int kbch = dvbt2::kbch(type, rate, CodeFamily::S2);
    return symbolRate * (kbch - 80) / plFrameSymbols(modcod, type, pilots);
}

const uint8_t *sofBits()
{
    // 18D2E82 hex, MSB first
    static const uint8_t bits[sofSymbols] = {0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0};
    return bits;
}

const uint8_t *plsScrambling()
{
    static const uint8_t bits[64] = {
        0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1,
        0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0
    };
    return bits;
}

void plsCodeword(int value, uint8_t bits[32])
{
    // b0 multiplies S2X's row, then b1..b6 figure 13b's rows; y1 the MSB
    static const uint32_t rows[7] = {plsB0Row, 0x55555555u, 0x33333333u, 0x0F0F0F0Fu, 0x00FF00FFu, 0x0000FFFFu, 0xFFFFFFFFu};
    uint32_t word = 0;

    for (int b = 0; b < 7; b++)
    {
        if ((value >> (6 - b)) & 1) {
            word ^= rows[b];
        }
    }

    for (int i = 0; i < 32; i++) {
        bits[i] = (word >> (31 - i)) & 1;
    }
}

void plHeaderBits(int modcod, int type, uint8_t bits[headerSymbols])
{
    std::memcpy(bits, sofBits(), sofSymbols);
    // S2's b1..b5: the MODCOD, b6: the TYPE's MSB; S2X's b0 = 1, b1..b6 its MODCOD. Into the (32, 7) code, each bit
    // repeated, complemented if b7 (the TYPE's LSB, the pilots)
    uint8_t y[32];
    const int value = isS2X(modcod) ? 64 | ((modcod - s2xModcodBase) & 63) : ((modcod & 31) << 1) | ((type >> 1) & 1);
    plsCodeword(value, y);
    const uint8_t b7 = type & 1;
    const uint8_t *scrambling = plsScrambling();

    for (int i = 0; i < 32; i++)
    {
        bits[sofSymbols + 2 * i] = y[i] ^ scrambling[2 * i];
        bits[sofSymbols + 2 * i + 1] = (y[i] ^ b7) ^ scrambling[2 * i + 1];
    }
}

Cf headerSymbolZero(int n)
{
    // I = Q = (1 - 2y) / sqrt 2 at odd positions counting from 1, I = -Q = -(1 - 2y) / sqrt 2 at even ones
    const float a = 0.70710678f;
    return (n & 1) ? Cf(-a, a) : Cf(a, a);
}

void plHeaderSymbols(int modcod, int type, Cf symbols[headerSymbols])
{
    uint8_t bits[headerSymbols];
    plHeaderBits(modcod, type, bits);
    // S2X's PLS code: a phase jump of pi/2 after the SOF (EN 302 307-2 5.5.2)
    const Cf turn = isS2X(modcod) ? Cf(0.0f, 1.0f) : Cf(1.0f, 0.0f);

    for (int n = 0; n < headerSymbols; n++) {
        symbols[n] = headerSymbolZero(n) * (bits[n] ? -1.0f : 1.0f) * (n >= sofSymbols ? turn : Cf(1.0f, 0.0f));
    }
}

int timeSlicingParity(int input, int state)
{
    // g0 = 10101, g1 = 10111, g2 = 11011, g3 = 11111, g4 = 11001: taps on (input, S0, S1, S2, S3), MSB first
    static const int g[5] = {0x15, 0x17, 0x1b, 0x1f, 0x19};
    const int reg = ((input & 1) << 4) | (state & 15);
    int bits = 0;

    for (int i = 0; i < 5; i++)
    {
        int parity = g[i] & reg;
        parity ^= parity >> 4;
        parity ^= parity >> 2;
        parity ^= parity >> 1;
        bits |= (parity & 1) << (4 - i);
    }

    return bits;
}

void timeSlicingCodeword(int word, uint8_t bits[timeSlicingCodeBits])
{
    auto u = [word](int j) { return (word >> (15 - j)) & 1; };
    // Tail biting: S0 = u15, S1 = u14, S2 = u13, S3 = u12, the state the last bits leave it in
    int state = (u(15) << 3) | (u(14) << 2) | (u(13) << 1) | u(12);
    int k = 0;

    for (int j = 0; j < 16; j++)
    {
        const int p = timeSlicingParity(u(j), state);

        for (int i = 0; i < (timeSlicingHasP4(j) ? 5 : 4); i++) {
            bits[k++] = (p >> (4 - i)) & 1;
        }

        state = (u(j) << 3) | (state >> 1);
    }
}

const uint8_t *timeSlicingScrambling()
{
    // Made once (a static's initialisation is thread safe)
    struct Table
    {
        uint8_t m_bits[2 * timeSlicingCodeBits];

        Table()
        {
            static const char *sequence =
                "10111100000100011000000010100101010100010111011101011100100111"
                "01100001011000111111011010110011011011100001110001101010011111"
                "000100001100101000000111101111";

            for (int i = 0; i < 2 * timeSlicingCodeBits; i++) {
                m_bits[i] = sequence[i] == '1';
            }
        }
    };

    static const Table table;
    return table.m_bits;
}

void timeSlicingHeaderBits(int modcod, int type, int tsn, uint8_t bits[timeSlicingHeaderSymbols])
{
    std::memcpy(bits, sofBits(), sofSymbols);
    uint8_t c[timeSlicingCodeBits];
    timeSlicingCodeword((plsCode(modcod, type) << 8) | (tsn & 255), c);
    const uint8_t *scrambling = timeSlicingScrambling();

    for (int i = 0; i < 2 * timeSlicingCodeBits; i++) {
        bits[sofSymbols + i] = c[i / 2] ^ scrambling[i];
    }
}

void timeSlicingHeader(int modcod, int type, int tsn, Cf symbols[timeSlicingHeaderSymbols])
{
    uint8_t bits[timeSlicingHeaderSymbols];
    timeSlicingHeaderBits(modcod, type, tsn, bits);

    for (int n = 0; n < timeSlicingHeaderSymbols; n++) {
        symbols[n] = headerSymbolZero(n) * (bits[n] ? -1.0f : 1.0f);
    }
}

// EN 302 307-2 tables 18a, 18b and 19a: by the VL-SNR header's index, the code (the S2 short 1/4 and 1/3 codes are
// "1/5" and "1/3"), spreading, shortening and puncturing
const VLSNRModcod S2Tables::m_vlsnrModcods[vlsnrIndexes] = {
    {true, 1, Modulation::QPSK, CodeRate::R2_9, FECType::Normal, 1, 0, 15, 3240, -2.85, "QPSK 2/9"},
    {true, 1, Modulation::BPSK, CodeRate::R1_5, FECType::Medium, 1, 640, 25, 980, -6.85, "BPSK 1/5"},
    {true, 1, Modulation::BPSK, CodeRate::R11_45, FECType::Medium, 1, 0, 15, 1620, -5.50, "BPSK 11/45"},
    {true, 1, Modulation::BPSK, CodeRate::R1_3, FECType::Medium, 1, 0, 13, 1620, -4.00, "BPSK 1/3"},
    {true, 1, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 2, 560, 30, 250, -9.9, "BPSK-S 1/5"},
    {true, 1, Modulation::BPSK, CodeRate::R11_45, FECType::Short, 2, 0, 15, 810, -8.3, "BPSK-S 11/45"},
    {false, 1, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 1, 0, 0.0, "Unassigned"},
    {false, 1, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 1, 0, 0.0, "Unassigned"},
    {false, 1, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 1, 0, 0.0, "Unassigned"},
    {true, 2, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 10, 1224, -6.1, "BPSK 1/5"},
    {true, 2, Modulation::BPSK, CodeRate::R4_15, FECType::Short, 1, 0, 8, 1224, -4.9, "BPSK 4/15"},
    {true, 2, Modulation::BPSK, CodeRate::R1_3, FECType::Short, 1, 0, 8, 1224, -3.72, "BPSK 1/3"},
    {false, 2, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 1, 0, 0.0, "Dummy"},
    {false, 2, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 1, 0, 0.0, "Unassigned"},
    {false, 2, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 1, 0, 0.0, "Unassigned"},
    {false, 2, Modulation::BPSK, CodeRate::R1_4, FECType::Short, 1, 0, 1, 0, 0.0, "Unassigned"},
};

const VLSNRModcod& vlsnrModcod(int index)
{
    return S2Tables::m_vlsnrModcods[index & (vlsnrIndexes - 1)];
}

int vlsnrFrameSymbols(int set)
{
    return set == 1 ? 33282 : 16686;
}

int vlsnrDataSymbols(int set)
{
    return set == 1 ? 30780 : 14976;
}

// The pilot mask of runs after the VL-SNR header (figures 17 and 18): 540 data symbols and S2's pilot block (16 slots
// from the PLHEADER), then groups of data, an extra block, data and S2's block, then the last data
std::vector<uint8_t> S2Tables::vlsnrPilotMask(std::initializer_list<std::array<int, 4>> groups, int last)
{
    std::vector<uint8_t> mask;
    auto run = [&mask](int n, uint8_t pilot) { mask.insert(mask.end(), (size_t) n, pilot); };
    run(540, 0);
    run(pilotSymbols, 1);

    for (const std::array<int, 4>& g : groups)    // Count, data, extra pilots, data
    {
        for (int i = 0; i < g[0]; i++)
        {
            run(g[1], 0);
            run(g[2], 1);
            run(g[3], 0);
            run(pilotSymbols, 1);
        }
    }

    run(last, 0);
    return mask;
}

const std::vector<uint8_t>& vlsnrPilots(int set)
{
    static const std::vector<uint8_t> set1 = S2Tables::vlsnrPilotMask({{18, 703, 34, 703}, {3, 702, 36, 702}}, 720);
    static const std::vector<uint8_t> set2 = S2Tables::vlsnrPilotMask({{9, 704, 32, 704}, {1, 702, 36, 702}}, 360);
    return set == 1 ? set1 : set2;
}

void vlsnrHeaderBits(int index, uint8_t bits[vlsnrHeaderSymbols])
{
    // 5.5.2.5's 896 bit pattern, 16 rows of 56
    static const char *rows[16] = {
        "11111011111100100011111010000011011111111001101111000100",
        "10011000011100001000111000001011001110010011010001011110",
        "11110110101000101100100111111110000110110001011100110111",
        "10000100000110001101100101011010011011111001100101111010",
        "01111011011111010111101100111110100111111100100111101010",
        "01011110011110001011101000000011101001101101010100011010",
        "00100111100111001100001001100101010000111110110011010000",
        "00110100001010110000010010011000101111110011110101111101",
        "10101101110100000011011011101001110101010011000100101111",
        "00010000011000011100011011011111100000100110001000110111",
        "01110010110100111110000010010000011100111000010011000111",
        "00111011110101011010110011101110001001011110001011001001",
        "01011001000010000111110110000010011000010101101011011010",
        "11101001101011110000000101110010110011111001110110100111",
        "00111111010010000011010110100100000001100011111100000111",
        "00100011110010011010111011101100111100101110110101000001",
    };
    // Table 18b's Walsh-Hadamard sequences: a minus inverts its row
    static const char *sequences[vlsnrIndexes] = {
        "++++++++++++++++",
        "+-+-+-+-+-+-+-+-",
        "++--++--++--++--",
        "+--++--++--++--+",
        "++++----++++----",
        "+--+-++-+--+-++-",
        "++--++----++--++",
        "+--++--+-++--++-",
        "++++--------++++",
        "++----++++----++",
        "+-+--+-++-+--+-+",
        "--------++++++++",
        "+-+-+-+--+-+-+-+",
        "+-+--+-+-+-++-+-",
        "++----++--++++--",
        "+--+-++--++-+--+",
    };
    const char *sequence = sequences[index & (vlsnrIndexes - 1)];
    bits[0] = bits[1] = 0;
    bits[vlsnrHeaderSymbols - 2] = bits[vlsnrHeaderSymbols - 1] = 0;

    for (int r = 0; r < 16; r++)
    {
        const uint8_t invert = sequence[r] == '-' ? 1 : 0;

        for (int i = 0; i < 56; i++) {
            bits[2 + r * 56 + i] = (uint8_t) ((rows[r][i] - '0') ^ invert);
        }
    }
}

void vlsnrHeader(int index, Cf symbols[vlsnrHeaderSymbols])
{
    uint8_t bits[vlsnrHeaderSymbols];
    vlsnrHeaderBits(index, bits);

    // pi/2 BPSK (5.4.0a), as the PLHEADER's
    for (int n = 0; n < vlsnrHeaderSymbols; n++) {
        symbols[n] = headerSymbolZero(n) * (bits[n] ? -1.0f : 1.0f);
    }
}

int vlsnrKbch(int index)
{
    const VLSNRModcod& m = vlsnrModcod(index);
    return dvbt2::kbch(m.m_type, m.m_rate, CodeFamily::S2) - m.m_xs;
}

Scrambler::Scrambler(int n) :
    m_n(-1)
{
    setCode(n);
}

void Scrambler::setCode(int n)
{
    if (n == m_n) {
        return;
    }

    m_n = n;
    const int period = (1 << 18) - 1;
    std::vector<uint8_t> x(period), y(period);
    x[0] = 1;

    for (int i = 1; i < 18; i++) {
        x[i] = 0;
    }

    for (int i = 0; i < 18; i++) {
        y[i] = 1;
    }

    for (int i = 0; i + 18 < period; i++)
    {
        x[i + 18] = x[i + 7] ^ x[i];
        y[i + 18] = y[i + 10] ^ y[i + 7] ^ y[i + 5] ^ y[i];
    }

    auto z = [&](int i) { return (uint8_t) (x[(i + n) % period] ^ y[i]); };
    m_sequence.resize(m_length);

    for (int i = 0; i < m_length; i++) {
        m_sequence[i] = (uint8_t) (2 * z((i + 131072) % period) + z(i));
    }
}

// To unit average power
void S2Tables::normalise(std::vector<Cf>& points)
{
    double power = 0.0;

    for (const Cf& p : points) {
        power += std::norm(p);
    }

    const float scale = (float) (1.0 / std::sqrt(power / points.size()));

    for (Cf& p : points) {
        p *= scale;
    }
}

// EN 302 307-2's label tables give each point's angle / pi for p = q = 0 (a); the others are symmetric: p = 0 q = 1,
// -a; p = 1 q = 0, pi - a; p = q = 1, pi + a. Tables 12c and 13c have p and q's roles the other way round: their
// callers swap them
double S2Tables::tableAngle(double a, int p, int q)
{
    a *= m_pi;
    return p ? (q ? m_pi + a : m_pi - a) : (q ? -a : a);
}

void constellation(Modulation modulation, CodeRate rate, std::vector<Cf>& points, FECType type)
{
    constexpr double pi = S2Tables::m_pi;
    auto polar = [](double r, double angle) { return Cf((float) (r * std::cos(angle)), (float) (r * std::sin(angle))); };

    switch (modulation)
    {
    case Modulation::QPSK:
    {
        const float a = 0.70710678f;
        points = {Cf(a, a), Cf(a, -a), Cf(-a, a), Cf(-a, -a)};
        break;
    }
    case Modulation::PSK8:
    {
        // Figure 10
        static const double angles[8] = {pi / 4, 0.0, pi, 5 * pi / 4, pi / 2, 7 * pi / 4, 3 * pi / 4, 3 * pi / 2};
        points.resize(8);

        for (int i = 0; i < 8; i++) {
            points[i] = polar(1.0, angles[i]);
        }

        break;
    }
    case Modulation::APSK8:
    {
        // S2X's 2+4+2APSK, tables 10a and 10b: label p b1 b2, b1 b2 the ring and angle, p mirroring
        const bool r104 = rate == CodeRate::R104_180;
        const double radius[4] = {1.0, r104 ? 6.39 : 5.32, r104 ? 6.39 : 5.32, r104 ? 8.0 : 6.8};
        static const double angles[4] = {1.0, 1.352, 0.648, 1.0};
        points.resize(8);

        for (int i = 0; i < 8; i++) {
            points[i] = polar(radius[i & 3], S2Tables::tableAngle(angles[i & 3], i >> 2, 0));
        }

        S2Tables::normalise(points);
        break;
    }
    case Modulation::APSK16:
    {
        if ((rate == CodeRate::R18_30) || (rate == CodeRate::R20_30))
        {
            // S2X's 8+8APSK 18/30 and 20/30, table 11e: as points
            const float (*table)[2] = rate == CodeRate::R18_30 ? S2Tables::m_apsk16_18_30 : S2Tables::m_apsk16_20_30;
            points.resize(16);

            for (int i = 0; i < 16; i++) {
                points[i] = Cf(table[i][0], table[i][1]);
            }

            S2Tables::normalise(points);
            break;
        }

        if ((rate == CodeRate::R90_180) || (rate == CodeRate::R96_180) || (rate == CodeRate::R100_180))
        {
            // S2X's 8+8APSK, tables 11c and 11d: label r q p b3, r the ring
            points.resize(16);

            for (int i = 0; i < 16; i++) {
                points[i] = polar((i & 8) ? 2.19 : 1.0, S2Tables::tableAngle((i & 1) ? 3.0 / 8.0 : 1.0 / 8.0, (i >> 1) & 1, (i >> 2) & 1));
            }

            S2Tables::normalise(points);
            break;
        }

        // Figure 11 and table 9: 4 points on the inner ring, 12 on the outer. S2X's 4+12APSK is the same, with its own
        // ratios (EN 302 307-2 tables 11a and 11b)
        double gamma;

        switch (rate)
        {
        case CodeRate::R2_3: gamma = 3.15; break;
        case CodeRate::R3_4: gamma = 2.85; break;
        case CodeRate::R4_5: gamma = 2.75; break;
        case CodeRate::R5_6: gamma = 2.70; break;
        case CodeRate::R8_9: gamma = 2.60; break;
        case CodeRate::R26_45:
        case CodeRate::R3_5: gamma = 3.7; break;
        case CodeRate::R28_45: gamma = 3.5; break;
        case CodeRate::R23_36:
        case CodeRate::R25_36: gamma = 3.1; break;
        case CodeRate::R13_18:
        case CodeRate::R32_45: gamma = 2.85; break;
        case CodeRate::R140_180: gamma = 3.6; break;
        case CodeRate::R154_180: gamma = 3.2; break;
        case CodeRate::R7_15: gamma = 3.32; break;
        case CodeRate::R8_15: gamma = 3.50; break;
        default: gamma = 2.57; break;
        }

        const double r1 = std::sqrt(4.0 / (1.0 + 3.0 * gamma * gamma));
        const double r2 = gamma * r1;
        static const double outer[12] = {pi / 4, -pi / 4, 3 * pi / 4, -3 * pi / 4, pi / 12, -pi / 12, 11 * pi / 12, -11 * pi / 12,
            5 * pi / 12, -5 * pi / 12, 7 * pi / 12, -7 * pi / 12};
        static const double inner[4] = {pi / 4, -pi / 4, 3 * pi / 4, -3 * pi / 4};
        points.resize(16);

        for (int i = 0; i < 12; i++) {
            points[i] = polar(r2, outer[i]);
        }

        for (int i = 0; i < 4; i++) {
            points[12 + i] = polar(r1, inner[i]);
        }

        break;
    }
    case Modulation::APSK32:
    {
        struct Row { int ring; double angle; };

        if ((rate == CodeRate::R2_3) || (rate == CodeRate::R32_45))
        {
            // S2X's 4+12+16rbAPSK, tables 12a to 12c: label p b1 b2 q b4, by b1 b2 b4; q mirrors, p conjugates
            static const Row rows[8] = {{3, 11 / 16.0}, {3, 9 / 16.0}, {2, 3 / 4.0}, {2, 7 / 12.0}, {3, 13 / 16.0}, {3, 15 / 16.0},
                {2, 11 / 12.0}, {1, 3 / 4.0}};
            const bool normal = (rate == CodeRate::R2_3) && (type == FECType::Normal);
            const double radius[4] = {0.0, 1.0, normal ? 2.85 : 2.84, rate == CodeRate::R32_45 ? 5.26 : normal ? 5.55 : 5.54};
            points.resize(32);

            for (int i = 0; i < 32; i++)
            {
                const Row& row = rows[((i >> 1) & 6) | (i & 1)];
                points[i] = polar(radius[row.ring], S2Tables::tableAngle(row.angle, (i >> 1) & 1, (i >> 4) & 1));
            }

            S2Tables::normalise(points);
            break;
        }

        if ((rate == CodeRate::R128_180) || (rate == CodeRate::R132_180) || (rate == CodeRate::R140_180))
        {
            // S2X's 4+8+4+16APSK, tables 12d and 12e: label b0 b1 p q b4, by b0 b1 b4
            static const Row rows[8] = {{1, 1 / 4.0}, {4, 7 / 16.0}, {2, 1 / 12.0}, {4, 1 / 16.0}, {2, 5 / 12.0}, {4, 5 / 16.0},
                {3, 1 / 4.0}, {4, 3 / 16.0}};
            const double radius[5] = {0.0, 1.0, rate == CodeRate::R140_180 ? 2.8 : 2.6,
                rate == CodeRate::R128_180 ? 2.99 : rate == CodeRate::R132_180 ? 2.86 : 3.08, 5.6};
            points.resize(32);

            for (int i = 0; i < 32; i++)
            {
                const Row& row = rows[((i >> 2) & 6) | (i & 1)];
                points[i] = polar(radius[row.ring], S2Tables::tableAngle(row.angle, (i >> 2) & 1, (i >> 1) & 1));
            }

            S2Tables::normalise(points);
            break;
        }

        // Figure 12 and table 10: 4, 12 and 16 points on the rings (as leansdr's and the DATV modulator's tables)
        double g1, g2;

        switch (rate)
        {
        case CodeRate::R3_4: g1 = 2.84; g2 = 5.27; break;
        case CodeRate::R4_5: g1 = 2.72; g2 = 4.87; break;
        case CodeRate::R5_6: g1 = 2.64; g2 = 4.64; break;
        case CodeRate::R8_9: g1 = 2.54; g2 = 4.33; break;
        default: g1 = 2.53; g2 = 4.30; break;
        }

        const double r1 = std::sqrt(8.0 / (1.0 + 3.0 * g1 * g1 + 4.0 * g2 * g2));
        const double r2 = g1 * r1, r3 = g2 * r1;
        // Ring and angle in units of pi/12 (ring 2), pi/8 (ring 3) or pi/4 (ring 1)
        struct Point { int ring; double angle; };
        static const Point table[32] = {
            {2, pi / 4}, {2, 5 * pi / 12}, {2, -pi / 4}, {2, -5 * pi / 12}, {2, 3 * pi / 4}, {2, 7 * pi / 12}, {2, -3 * pi / 4}, {2, -7 * pi / 12},
            {3, pi / 8}, {3, 3 * pi / 8}, {3, -pi / 4}, {3, -pi / 2}, {3, 3 * pi / 4}, {3, pi / 2}, {3, -7 * pi / 8}, {3, -5 * pi / 8},
            {2, pi / 12}, {1, pi / 4}, {2, -pi / 12}, {1, -pi / 4}, {2, 11 * pi / 12}, {1, 3 * pi / 4}, {2, -11 * pi / 12}, {1, -3 * pi / 4},
            {3, 0.0}, {3, pi / 4}, {3, -pi / 8}, {3, -3 * pi / 8}, {3, 7 * pi / 8}, {3, 5 * pi / 8}, {3, pi}, {3, -3 * pi / 4},
        };
        points.resize(32);

        for (int i = 0; i < 32; i++) {
            points[i] = polar(table[i].ring == 1 ? r1 : table[i].ring == 2 ? r2 : r3, table[i].angle);
        }

        break;
    }
    case Modulation::APSK64:
    {
        struct Row { int ring; double angle; };
        points.resize(64);

        if (rate == CodeRate::R128_180)
        {
            // 16+16+16+16APSK, tables 13a and 13b: label b0 b1 q p b4 b5, b0 b1 the ring, b4 b5 the angle
            static const int rings[4] = {1, 2, 4, 3};
            static const double angles[4] = {1 / 16.0, 3 / 16.0, 7 / 16.0, 5 / 16.0};
            static const double radius[5] = {0.0, 1.0, 1.88, 2.72, 3.95};

            for (int i = 0; i < 64; i++) {
                points[i] = polar(radius[rings[i >> 4]], S2Tables::tableAngle(angles[i & 3], (i >> 2) & 1, (i >> 3) & 1));
            }
        }
        else if (rate == CodeRate::R132_180)
        {
            // 4+12+20+28APSK, tables 13e and 13f: label b0 b1 b2 b3 p q, by b0..b3
            static const Row rows[16] = {{4, 1 / 4.0}, {4, 13 / 28.0}, {4, 1 / 28.0}, {1, 1 / 4.0}, {4, 9 / 28.0}, {4, 11 / 28.0},
                {3, 1 / 20.0}, {2, 1 / 12.0}, {4, 5 / 28.0}, {3, 9 / 20.0}, {4, 3 / 28.0}, {2, 5 / 12.0}, {3, 1 / 4.0}, {3, 7 / 20.0},
                {3, 3 / 20.0}, {2, 1 / 4.0}};
            static const double radius[5] = {0.0, 1.0, 2.4, 4.3, 7.0};

            for (int i = 0; i < 64; i++)
            {
                const Row& row = rows[i >> 2];
                points[i] = polar(radius[row.ring], S2Tables::tableAngle(row.angle, (i >> 1) & 1, i & 1));
            }
        }
        else
        {
            // 8+16+20+20APSK 7/9, 4/5 and 5/6, tables 13c and 13d: label p b1 q b3 b4 b5, by b1 b3 b4 b5; q mirrors, p
            // conjugates
            static const Row rows[16] = {{2, 25 / 16.0}, {4, 7 / 4.0}, {2, 27 / 16.0}, {3, 7 / 4.0}, {4, 31 / 20.0}, {4, 33 / 20.0},
                {3, 31 / 20.0}, {3, 33 / 20.0}, {1, 13 / 8.0}, {4, 37 / 20.0}, {2, 29 / 16.0}, {3, 37 / 20.0}, {1, 15 / 8.0},
                {4, 39 / 20.0}, {2, 31 / 16.0}, {3, 39 / 20.0}};
            const bool r56 = rate == CodeRate::R5_6;
            const double radius[5] = {0.0, 1.0, 2.2, r56 ? 3.5 : 3.6, r56 ? 5.0 : 5.2};

            for (int i = 0; i < 64; i++)
            {
                const Row& row = rows[((i >> 1) & 8) | (i & 7)];
                points[i] = polar(radius[row.ring], S2Tables::tableAngle(row.angle, (i >> 3) & 1, i >> 5));
            }
        }

        S2Tables::normalise(points);
        break;
    }
    case Modulation::APSK128:
    {
        // Tables 14a and 14b: label q p b2..b6, by b2..b6
        struct Row { int ring; int num; int den; };
        static const Row rows[32] = {
            {1, 83, 1260}, {6, 11, 105}, {6, 37, 1680}, {6, 11, 168}, {2, 121, 2520}, {3, 23, 280}, {5, 19, 720}, {4, 61, 720},
            {1, 103, 560}, {6, 61, 420}, {6, 383, 1680}, {6, 929, 5040}, {2, 113, 560}, {3, 169, 1008}, {5, 563, 2520}, {4, 139, 840},
            {1, 243, 560}, {6, 1993, 5040}, {6, 43, 90}, {6, 73, 168}, {2, 1139, 2520}, {3, 117, 280}, {5, 341, 720}, {4, 349, 840},
            {1, 177, 560}, {6, 1789, 5040}, {6, 49, 180}, {6, 53, 168}, {2, 167, 560}, {3, 239, 720}, {5, 199, 720}, {4, 281, 840},
        };
        const double radius[7] = {0.0, 1.0, 1.715, 2.118, 2.681, 2.75, rate == CodeRate::R140_180 ? 3.733 : 3.819};
        points.resize(128);

        for (int i = 0; i < 128; i++)
        {
            const Row& row = rows[i & 31];
            points[i] = polar(radius[row.ring], S2Tables::tableAngle((double) row.num / row.den, (i >> 5) & 1, i >> 6));
        }

        S2Tables::normalise(points);
        break;
    }
    case Modulation::APSK256:
    {
        points.resize(256);

        if ((rate == CodeRate::R20_30) || (rate == CodeRate::R22_30))
        {
            // Table 15d: as points
            const float (*table)[2] = rate == CodeRate::R20_30 ? S2Tables::m_apsk256_20_30 : S2Tables::m_apsk256_22_30;

            for (int i = 0; i < 256; i++) {
                points[i] = Cf(table[i][0], table[i][1]);
            }
        }
        else
        {
            // Tables 15a to 15c: label r r r q p a a a, r r r the ring, a a a the angle (in pi / 32)
            static const int rings[8] = {1, 2, 4, 3, 8, 7, 5, 6};
            static const int angles[8] = {1, 3, 7, 5, 15, 13, 9, 11};
            static const double ratios[4][7] = {
                {1.791, 2.405, 2.980, 3.569, 4.235, 5.078, 6.536},      // 116/180 and 124/180
                {1.794, 2.409, 2.986, 3.579, 4.045, 4.6, 5.4},          // 128/180
                {1.794, 2.409, 2.986, 3.579, 4.045, 4.5, 5.2},          // 135/180
            };
            const double *ratio = ratios[rate == CodeRate::R128_180 ? 1 : rate == CodeRate::R135_180 ? 2 : 0];

            for (int i = 0; i < 256; i++)
            {
                const int ring = rings[i >> 5];
                points[i] = polar(ring == 1 ? 1.0 : ratio[ring - 2], S2Tables::tableAngle(angles[i & 7] / 32.0, (i >> 3) & 1, (i >> 4) & 1));
            }
        }

        S2Tables::normalise(points);
        break;
    }
    case Modulation::BPSK:
        break;  // VL-SNR's pi/2 BPSK: none (its cells, turned to the real axis, are its bits')
    }
}

// The bit interleaver's read out order: the column each bit of a symbol, MSB first, is from (EN 302 307-2 tables 9a
// and 9b). S2's are in order, but 8PSK 3/5's, reversed
const char *S2Tables::interleaverPattern(FECType type, Modulation modulation, CodeRate rate)
{
    struct Entry { FECType type; Modulation modulation; CodeRate rate; const char *pattern; };
    const FECType n = FECType::Normal, s = FECType::Short;
    static const Entry entries[] = {
        {n, Modulation::PSK8, CodeRate::R23_36, "012"}, {n, Modulation::PSK8, CodeRate::R25_36, "102"},
        {n, Modulation::PSK8, CodeRate::R13_18, "102"},
        {n, Modulation::APSK8, CodeRate::R100_180, "012"}, {n, Modulation::APSK8, CodeRate::R104_180, "012"},
        {n, Modulation::APSK16, CodeRate::R26_45, "3201"}, {n, Modulation::APSK16, CodeRate::R3_5, "3210"},
        {n, Modulation::APSK16, CodeRate::R28_45, "3012"}, {n, Modulation::APSK16, CodeRate::R23_36, "3021"},
        {n, Modulation::APSK16, CodeRate::R25_36, "2310"}, {n, Modulation::APSK16, CodeRate::R13_18, "3021"},
        {n, Modulation::APSK16, CodeRate::R140_180, "3210"}, {n, Modulation::APSK16, CodeRate::R154_180, "0321"},
        {n, Modulation::APSK16, CodeRate::R18_30, "0123"}, {n, Modulation::APSK16, CodeRate::R20_30, "0123"},
        {n, Modulation::APSK16, CodeRate::R90_180, "3210"}, {n, Modulation::APSK16, CodeRate::R96_180, "2310"},
        {n, Modulation::APSK16, CodeRate::R100_180, "2301"},
        {n, Modulation::APSK32, CodeRate::R2_3, "21430"}, {n, Modulation::APSK32, CodeRate::R128_180, "40312"},
        {n, Modulation::APSK32, CodeRate::R132_180, "40312"}, {n, Modulation::APSK32, CodeRate::R140_180, "40213"},
        {n, Modulation::APSK64, CodeRate::R128_180, "305214"}, {n, Modulation::APSK64, CodeRate::R132_180, "520143"},
        {n, Modulation::APSK64, CodeRate::R7_9, "201543"}, {n, Modulation::APSK64, CodeRate::R4_5, "124053"},
        {n, Modulation::APSK64, CodeRate::R5_6, "421053"},
        {n, Modulation::APSK128, CodeRate::R135_180, "4250316"}, {n, Modulation::APSK128, CodeRate::R140_180, "4130256"},
        {n, Modulation::APSK256, CodeRate::R116_180, "40372156"}, {n, Modulation::APSK256, CodeRate::R20_30, "01234567"},
        {n, Modulation::APSK256, CodeRate::R124_180, "46320571"}, {n, Modulation::APSK256, CodeRate::R128_180, "75642301"},
        {n, Modulation::APSK256, CodeRate::R22_30, "01234567"}, {n, Modulation::APSK256, CodeRate::R135_180, "50743612"},
        {s, Modulation::PSK8, CodeRate::R7_15, "102"}, {s, Modulation::PSK8, CodeRate::R8_15, "102"},
        {s, Modulation::PSK8, CodeRate::R26_45, "102"}, {s, Modulation::PSK8, CodeRate::R32_45, "012"},
        {s, Modulation::APSK16, CodeRate::R7_15, "2103"}, {s, Modulation::APSK16, CodeRate::R8_15, "2103"},
        {s, Modulation::APSK16, CodeRate::R26_45, "2130"}, {s, Modulation::APSK16, CodeRate::R3_5, "3201"},
        {s, Modulation::APSK16, CodeRate::R32_45, "0123"},
        {s, Modulation::APSK32, CodeRate::R2_3, "41230"}, {s, Modulation::APSK32, CodeRate::R32_45, "10423"},
    };

    for (const Entry& entry : entries)
    {
        if ((entry.type == type) && (entry.modulation == modulation) && (entry.rate == rate)) {
            return entry.pattern;
        }
    }

    return (modulation == Modulation::PSK8) && (rate == CodeRate::R3_5) ? "210" : "01234567";
}

void bitMap(FECType type, Modulation modulation, CodeRate rate, std::vector<int>& map)
{
    const int n = dvbt2::nldpc(type);
    const int eta = bitsPerSymbol(modulation);

    if (modulation == Modulation::QPSK)
    {
        map.resize(n);

        for (int i = 0; i < n; i++) {
            map[i] = i;
        }

        return;
    }

    // Written column by column, read row by row: bit b of symbol i is row i of the pattern's column b. 128APSK's
    // codeword has 6 zeros after it, to fill the rows, and the interleaver's output 84 ones, to fill the slots
    const char *pattern = S2Tables::interleaverPattern(type, modulation, rate);
    const int rows = (n + (modulation == Modulation::APSK128 ? 6 : 0)) / eta;
    const int symbols = slotCount(modulation, type) * slotSymbols;
    map.resize((size_t) symbols * eta);

    for (int i = 0; i < symbols; i++)
    {
        for (int b = 0; b < eta; b++)
        {
            const int bit = (pattern[b] - '0') * rows + i;
            map[i * eta + b] = i >= rows ? mapOne : bit < n ? bit : mapZero;
        }
    }
}

void NearestTable::build(const std::vector<Cf>& points)
{
    // The points by their real part: each search tries only those whose x is within its best distance yet, outwards
    // from its own x, starting from the last search's nearest (the grid's walked in order, so it's usually near). A
    // point's distance is never less than its x's part, so the result's the same as trying every point (the nearest,
    // the first of any as near), for a fraction of the time: 256APSK's had been 34M distances, a stall at a new MODCOD
    const int m = (int) points.size();
    std::vector<int> order(m);
    std::vector<float> xs(m);

    for (int k = 0; k < m; k++) {
        order[k] = k;
    }

    std::sort(order.begin(), order.end(), [&points](int a, int b) { return points[a].real() < points[b].real(); });

    for (int i = 0; i < m; i++) {
        xs[i] = points[order[i]].real();
    }

    int last = 0;

    auto nearestTo = [&](float x, float y) {
        if (m == 0) {
            return (uint16_t) 0;
        }

        const Cf c(x, y);
        int nearest = last;
        float best = std::norm(c - points[nearest]);

        // False once x alone is further than the best (with a margin, should the compiler round the sum differently)
        auto consider = [&](int i) {
            const float dx = x - xs[i];

            if (dx * dx > best * 1.001f) {
                return false;
            }

            const int k = order[i];
            const float e = std::norm(c - points[k]);

            if ((e < best) || ((e == best) && (k < nearest)))
            {
                best = e;
                nearest = k;
            }

            return true;
        };

        int right = (int) (std::lower_bound(xs.begin(), xs.end(), x) - xs.begin());
        int left = right - 1;

        while ((right < m) && consider(right)) {
            right++;
        }

        while ((left >= 0) && consider(left)) {
            left--;
        }

        last = nearest;
        return (uint16_t) nearest;
    };

    // Each cell's centre, then its corners (each shared by four cells, so found once)
    const int side = m_grid + 1;
    std::vector<uint16_t> corners((size_t) side * side);

    for (int gy = 0; gy < side; gy++)
    {
        for (int gx = 0; gx < side; gx++) {
            corners[gy * side + gx] = nearestTo(gx / m_cellsPerUnit - m_range, gy / m_cellsPerUnit - m_range);
        }
    }

    m_table.assign((size_t) m_grid * m_grid * m_slots, m_unused);

    for (int gy = 0; gy < m_grid; gy++)
    {
        for (int gx = 0; gx < m_grid; gx++)
        {
            uint16_t *cell = &m_table[(gy * m_grid + gx) * m_slots];
            const uint16_t found[5] = {nearestTo((gx + 0.5f) / m_cellsPerUnit - m_range, (gy + 0.5f) / m_cellsPerUnit - m_range),
                corners[gy * side + gx], corners[gy * side + gx + 1], corners[(gy + 1) * side + gx], corners[(gy + 1) * side + gx + 1]};
            int used = 0;

            for (uint16_t nearest : found)
            {
                if ((used < m_slots) && (std::find(cell, cell + used, nearest) == cell + used)) {
                    cell[used++] = nearest;
                }
            }
        }
    }
}

void DemapTable::build(const std::vector<Cf>& points, int eta)
{
    const int m = (int) points.size();
    m_nearest.build(points);
    m_offsets.assign(m + 1, 0);
    m_candidates.clear();
    std::vector<std::pair<float, int>> byDistance(m);

    for (int k = 0; k < m; k++)
    {
        for (int j = 0; j < m; j++) {
            byDistance[j] = std::make_pair(std::norm(points[j] - points[k]), j);
        }

        std::sort(byDistance.begin(), byDistance.end());
        m_offsets[k] = (int) m_candidates.size();
        m_candidates.push_back((uint8_t) k);

        for (int bit = 0; bit < eta; bit++)
        {
            const int mask = 1 << (eta - 1 - bit);
            int found = 0;

            for (size_t j = 0; (j < byDistance.size()) && (found < m_perBit); j++)
            {
                const int other = byDistance[j].second;

                if ((other ^ k) & mask)
                {
                    found++;

                    if (std::find(m_candidates.begin() + m_offsets[k], m_candidates.end(), (uint8_t) other) == m_candidates.end()) {
                        m_candidates.push_back((uint8_t) other);
                    }
                }
            }
        }
    }

    m_offsets[m] = (int) m_candidates.size();
}

// Round to the nearest, limited to the LDPC decoder's channel LLR limit
inline int8_t S2Tables::quantise(float v)
{
    const float limit = dvbt2::llrLimit;
    v = std::max(-limit, std::min(limit, v));
    return (int8_t) std::lrint(v);
}

void demap(const Cf *cells, const float *scales, int nbCells, const std::vector<Cf>& points, int eta,
    float outlierThreshold, const int *map, int8_t *llrs, float *errors, const DemapTable *table, bool oneScale)
{
    const int m = (int) points.size();
    float px[maxPoints], py[maxPoints];

    for (int k = 0; k < m; k++)
    {
        px[k] = points[k].real();
        py[k] = points[k].imag();
    }

    // A cell's LLRs from each bit's nearest point with it 0 and with it 1
    auto output = [&](int cell, const float *min0, const float *min1) {
        const float nearest = std::min(min0[0], min1[0]);
        float scale = scales[oneScale ? 0 : cell];

        if (errors) {
            errors[cell] = nearest;
        }

        if (outlierThreshold > 0.0f)
        {
            const float relative = nearest * scale * 0.25f;

            if (relative > outlierThreshold) {
                scale *= outlierThreshold / relative;
            }
        }

        for (int bit = 0; bit < eta; bit++)
        {
            const int index = map[cell * eta + bit];

            if (index >= 0) {
                llrs[index] = S2Tables::quantise((min1[bit] - min0[bit]) * scale * 0.25f);
            }
        }
    };

    if (table && !table->empty())
    {
        // From the nearest point's candidates
        const NearestTable& nearestTable = table->nearestTable();

        for (int c = 0; c < nbCells; c++)
        {
            float min0[maxBitsPerSymbol], min1[maxBitsPerSymbol];

            for (int bit = 0; bit < eta; bit++)
            {
                min0[bit] = 1e30f;
                min1[bit] = 1e30f;
            }

            int count;
            const uint8_t *candidates = table->candidates(nearestTable.nearest(cells[c], points), count);

            for (int j = 0; j < count; j++)
            {
                const int k = candidates[j];
                const float dx = cells[c].real() - px[k], dy = cells[c].imag() - py[k];
                const float d = dx * dx + dy * dy;

                for (int bit = 0; bit < eta; bit++)
                {
                    float& target = ((k >> (eta - 1 - bit)) & 1) ? min1[bit] : min0[bit];
                    target = std::min(target, d);
                }
            }

            output(c, min0, min1);
        }

        return;
    }

    int c = 0;
#if defined(DVBS2_SSE2)
    // 4 cells at a time: the distance to each point, and for each bit the nearest point with it 0 and 1
    const __m128 inf = _mm_set1_ps(1e30f);
    const __m128 quarter = _mm_set1_ps(0.25f);
    const __m128 vThreshold = _mm_set1_ps(outlierThreshold);
    __m128 vpx[maxPoints], vpy[maxPoints];  // Each point, broadcast once rather than for every 4 cells

    for (int k = 0; k < m; k++)
    {
        vpx[k] = _mm_set1_ps(px[k]);
        vpy[k] = _mm_set1_ps(py[k]);
    }

    for (; c + 4 <= nbCells; c += 4)
    {
        const float *f = reinterpret_cast<const float *>(cells + c);
        const __m128 a = _mm_loadu_ps(f), b = _mm_loadu_ps(f + 4);  // re0 im0 re1 im1 | re2 im2 re3 im3
        const __m128 x = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
        const __m128 y = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
        __m128 min0[maxBitsPerSymbol], min1[maxBitsPerSymbol];

        for (int bit = 0; bit < eta; bit++)
        {
            min0[bit] = inf;
            min1[bit] = inf;
        }

        for (int k = 0; k < m; k++)
        {
            const __m128 dx = _mm_sub_ps(x, vpx[k]), dy = _mm_sub_ps(y, vpy[k]);
            const __m128 d = _mm_add_ps(_mm_mul_ps(dx, dx), _mm_mul_ps(dy, dy));

            for (int bit = 0; bit < eta; bit++)
            {
                if ((k >> (eta - 1 - bit)) & 1) {
                    min1[bit] = _mm_min_ps(min1[bit], d);
                } else {
                    min0[bit] = _mm_min_ps(min0[bit], d);
                }
            }
        }

        const __m128 nearest = _mm_min_ps(min0[0], min1[0]);
        __m128 scale = oneScale ? _mm_set1_ps(scales[0]) : _mm_loadu_ps(scales + c);

        if (errors) {
            _mm_storeu_ps(errors + c, nearest);
        }

        if (outlierThreshold > 0.0f)
        {
            // Relative to the noise per dimension: error power x scale / 4
            const __m128 relative = _mm_mul_ps(_mm_mul_ps(nearest, scale), quarter);
            const __m128 over = _mm_cmpgt_ps(relative, vThreshold);
            const __m128 limited = _mm_mul_ps(scale, _mm_div_ps(vThreshold, relative));
            scale = _mm_or_ps(_mm_and_ps(over, limited), _mm_andnot_ps(over, scale));
        }

        const __m128 s = _mm_mul_ps(scale, quarter);
        const __m128 limit = _mm_set1_ps(dvbt2::llrLimit), negativeLimit = _mm_set1_ps(-dvbt2::llrLimit);

        for (int bit = 0; bit < eta; bit++)
        {
            // Clamped and rounded 4 at once, as quantise() does each (a call to lrint each was much of the time)
            const __m128 v = _mm_min_ps(limit, _mm_max_ps(negativeLimit, _mm_mul_ps(_mm_sub_ps(min1[bit], min0[bit]), s)));
            alignas(16) int32_t q[4];
            _mm_store_si128(reinterpret_cast<__m128i *>(q), _mm_cvtps_epi32(v));

            for (int j = 0; j < 4; j++)
            {
                const int index = map[(c + j) * eta + bit];

                if (index >= 0) {
                    llrs[index] = (int8_t) q[j];
                }
            }
        }
    }
#endif

    for (; c < nbCells; c++)
    {
        float min0[maxBitsPerSymbol], min1[maxBitsPerSymbol];

        for (int bit = 0; bit < eta; bit++)
        {
            min0[bit] = 1e30f;
            min1[bit] = 1e30f;
        }

        for (int k = 0; k < m; k++)
        {
            const float dx = cells[c].real() - px[k], dy = cells[c].imag() - py[k];
            const float d = dx * dx + dy * dy;

            for (int bit = 0; bit < eta; bit++)
            {
                float& target = ((k >> (eta - 1 - bit)) & 1) ? min1[bit] : min0[bit];
                target = std::min(target, d);
            }
        }

        output(c, min0, min1);
    }
}

} // namespace dvbs2
