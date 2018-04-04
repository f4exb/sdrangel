///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Symbol synchronizer or symbol clock recovery mostly encapsulating             //
// liquid-dsp's symsync "object"                                                 //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "symsync.h"

SymbolSynchronizer::SymbolSynchronizer()
{
    // For now use hardcoded values:
    //   - RRC filter
    //   - 4 samples per symbol
    //   - 5 symbols delay filter
    //   - 0.5 filter excess bandwidth factor
    //   - 32 filter elements for the internal polyphase filter
    m_sync = symsync_crcf_create_rnyquist(LIQUID_FIRFILT_RRC, 4, 5, 0.5f, 32);
    //   - 0.02 loop filter bandwidth factor
    symsync_crcf_set_lf_bw(m_sync, 0.01f);
    //   - 4 samples per symbol output rate
    symsync_crcf_set_output_rate(m_sync, 4);
    m_syncSampleCount = 0;
}

SymbolSynchronizer::~SymbolSynchronizer()
{
    symsync_crcf_destroy(m_sync);
}

Real SymbolSynchronizer::run(const Sample& s)
{
    unsigned int nn;
    Real v = -1.0f;
    liquid_float_complex y = (s.m_real / SDR_RX_SCALEF) + (s.m_imag / SDR_RX_SCALEF)*I;
    symsync_crcf_execute(m_sync, &y, 1, m_z, &nn);

    for (unsigned int i = 0; i < nn; i++)
    {
        if (nn != 1) {
            qDebug("SymbolSynchronizer::run: %u", nn);
        }

        if (m_syncSampleCount % 4 == 0) {
            v = 1.0f;
        }

        if (m_syncSampleCount < 4095) {
            m_syncSampleCount++;
        } else {
            qDebug("SymbolSynchronizer::run: tau: %f", symsync_crcf_get_tau(m_sync));
            m_syncSampleCount = 0;
        }
    }

    return v;
}

liquid_float_complex SymbolSynchronizer::runZ(const Sample& s)
{
    unsigned int nn;
    liquid_float_complex y = (s.m_real / SDR_RX_SCALEF) + (s.m_imag / SDR_RX_SCALEF)*I;
    symsync_crcf_execute(m_sync, &y, 1, m_z, &nn);

    for (unsigned int i = 0; i < nn; i++)
    {
        if (nn != 1) {
            qDebug("SymbolSynchronizer::run: %u", nn);
        }

        if (m_syncSampleCount == 0) {
            m_z0 = m_z[i];
        }

        if (m_syncSampleCount < 3) {
            m_syncSampleCount++;
        } else {
            m_syncSampleCount = 0;
        }
    }

    return m_z0;
}
