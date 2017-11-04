///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <cstdio>
#include <cstring>
#include <cmath>
#include "devicelimesdr.h"

bool DeviceLimeSDR::setNCOFrequency(lms_device_t *device, bool dir_tx, std::size_t chan, bool enable, float frequency)
{
    if (enable)
    {
        bool positive;
        float_type freqs[LMS_NCO_VAL_COUNT];
        float_type phos[LMS_NCO_VAL_COUNT];

        if (LMS_GetNCOFrequency(device, dir_tx, chan, freqs, phos) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot get NCO frequencies and phases\n");
        }

        if (frequency < 0)
        {
            positive = false;
            frequency = -frequency;
        }
        else
        {
            positive = true;
        }

        freqs[0] = frequency;

        if (LMS_SetNCOFrequency(device, dir_tx, chan, freqs, 0.0f) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot set frequency to %f\n", frequency);
            return false;
        }

        if (LMS_SetNCOIndex(device, dir_tx, chan, 0, !positive) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot set conversion direction %sfreq\n", positive ? "+" : "-");
            return false;
        }

        if (dir_tx)
        {
            if (LMS_WriteParam(device,LMS7param(CMIX_BYP_TXTSP),0) < 0) {
                fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot enable Tx NCO\n");
                return false;
            }
        }
        else
        {
            if (LMS_WriteParam(device,LMS7param(CMIX_BYP_RXTSP),0) < 0) {
                fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot enable Rx NCO\n");
                return false;
            }
        }

        return true;
    }
    else
    {
        if (dir_tx)
        {
            if (LMS_WriteParam(device,LMS7param(CMIX_BYP_TXTSP),1) < 0) {
                fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot disable Tx NCO\n");
                return false;
            }
        }
        else
        {
            if (LMS_WriteParam(device,LMS7param(CMIX_BYP_RXTSP),1) < 0) {
                fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot disable Rx NCO\n");
                return false;
            }
        }

        return true;
    }
}

bool DeviceLimeSDR::SetRFELNA_dB(lms_device_t *device, std::size_t chan, int value)
{
    if (LMS_WriteParam(device, LMS7param(MAC), chan+1) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRFELNA_dB: cannot set channel to #%lu\n", chan);
        return false;
    }

    if (value > 30) {
        value = 30;
    }

    int val = value - 30;

    int g_lna_rfe = 0;
    if (val >= 0) g_lna_rfe = 15;
    else if (val >= -1) g_lna_rfe = 14;
    else if (val >= -2) g_lna_rfe = 13;
    else if (val >= -3) g_lna_rfe = 12;
    else if (val >= -4) g_lna_rfe = 11;
    else if (val >= -5) g_lna_rfe = 10;
    else if (val >= -6) g_lna_rfe = 9;
    else if (val >= -9) g_lna_rfe = 8;
    else if (val >= -12) g_lna_rfe = 7;
    else if (val >= -15) g_lna_rfe = 6;
    else if (val >= -18) g_lna_rfe = 5;
    else if (val >= -21) g_lna_rfe = 4;
    else if (val >= -24) g_lna_rfe = 3;
    else if (val >= -27) g_lna_rfe = 2;
    else g_lna_rfe = 1;

    if (LMS_WriteParam(device, LMS7param(G_LNA_RFE), g_lna_rfe) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRFELNA_dB: cannot set LNA gain to %d (%d)\n", value, g_lna_rfe);
        return false;
    }

    return true;
}

bool DeviceLimeSDR::SetRFETIA_dB(lms_device_t *device, std::size_t chan, int value)
{
    if (LMS_WriteParam(device, LMS7param(MAC), chan+1) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRFETIA_dB: cannot set channel to #%lu\n", chan);
        return false;
    }

    if (value > 3) {
        value = 3;
    } else if (value < 1) {
        value = 1;
    }

    int g_tia_rfe = value;

    if (LMS_WriteParam(device, LMS7param(G_TIA_RFE), g_tia_rfe) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRFELNA_dB: cannot set TIA gain to %d (%d)\n", value, g_tia_rfe);
        return false;
    }

    return true;
}

bool DeviceLimeSDR::SetRBBPGA_dB(lms_device_t *device, std::size_t chan, float value)
{
    if (LMS_WriteParam(device, LMS7param(MAC), chan+1) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRBBPGA_dB: cannot set channel to #%lu\n", chan);
        return false;
    }

    int g_pga_rbb = (int)(value + 12.5);
    if (g_pga_rbb > 0x1f) g_pga_rbb = 0x1f;
    if (g_pga_rbb < 0) g_pga_rbb = 0;

    if (LMS_WriteParam(device, LMS7param(G_PGA_RBB), g_pga_rbb) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRBBPGA_dB: cannot set G_PGA_RBB to %d\n", g_pga_rbb);
        return false;
    }

    int rcc_ctl_pga_rbb = (430.0*pow(0.65, (g_pga_rbb/10.0))-110.35)/20.4516 + 16;

    int c_ctl_pga_rbb = 0;
    if (0 <= g_pga_rbb && g_pga_rbb < 8) c_ctl_pga_rbb = 3;
    if (8 <= g_pga_rbb && g_pga_rbb < 13) c_ctl_pga_rbb = 2;
    if (13 <= g_pga_rbb && g_pga_rbb < 21) c_ctl_pga_rbb = 1;
    if (21 <= g_pga_rbb) c_ctl_pga_rbb = 0;

    if (LMS_WriteParam(device, LMS7param(RCC_CTL_PGA_RBB), rcc_ctl_pga_rbb) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRBBPGA_dB: cannot set RCC_CTL_PGA_RBB to %d\n", rcc_ctl_pga_rbb);
        return false;
    }

    if (LMS_WriteParam(device, LMS7param(C_CTL_PGA_RBB), c_ctl_pga_rbb) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::SetRBBPGA_dB: cannot set C_CTL_PGA_RBB to %d\n", c_ctl_pga_rbb);
        return false;
    }

    return true;
}

bool DeviceLimeSDR::setRxAntennaPath(lms_device_t *device, std::size_t chan, int path)
{
//    if (LMS_WriteParam(device, LMS7param(MAC), chan+1) < 0)
//    {
//        fprintf(stderr, "DeviceLimeSDR::setAntennaPath: cannot set channel to #%lu\n", chan);
//        return false;
//    }
//
//    int sel_path_rfe = 0;
//    switch ((PathRFE) path)
//    {
//    case PATH_RFE_NONE: sel_path_rfe = 0; break;
//    case PATH_RFE_LNAH: sel_path_rfe = 1; break;
//    case PATH_RFE_LNAL: sel_path_rfe = 2; break;
//    case PATH_RFE_LNAW: sel_path_rfe = 3; break;
//    case PATH_RFE_LB1: sel_path_rfe = 3; break;
//    case PATH_RFE_LB2: sel_path_rfe = 2; break;
//    }
//
//    int pd_lna_rfe = 1;
//    switch ((PathRFE) path)
//    {
//    case PATH_RFE_LNAH:
//    case PATH_RFE_LNAL:
//    case PATH_RFE_LNAW: pd_lna_rfe = 0; break;
//    default: break;
//    }
//
//    int pd_rloopb_1_rfe = (path == (int) PATH_RFE_LB1) ? 0 : 1;
//    int pd_rloopb_2_rfe = (path == (int) PATH_RFE_LB2) ? 0 : 1;
//    int en_inshsw_l_rfe = (path == (int) PATH_RFE_LNAL ) ? 0 : 1;
//    int en_inshsw_w_rfe = (path == (int) PATH_RFE_LNAW) ? 0 : 1;
//    int en_inshsw_lb1_rfe = (path == (int) PATH_RFE_LB1) ? 0 : 1;
//    int en_inshsw_lb2_rfe = (path == (int) PATH_RFE_LB2) ? 0 : 1;
//
//    int ret = 0;
//
//    ret += LMS_WriteParam(device, LMS7param(PD_LNA_RFE), pd_lna_rfe);
//    ret += LMS_WriteParam(device, LMS7param(PD_RLOOPB_1_RFE), pd_rloopb_1_rfe);
//    ret += LMS_WriteParam(device, LMS7param(PD_RLOOPB_2_RFE), pd_rloopb_2_rfe);
//    ret += LMS_WriteParam(device, LMS7param(EN_INSHSW_LB1_RFE), en_inshsw_lb1_rfe);
//    ret += LMS_WriteParam(device, LMS7param(EN_INSHSW_LB2_RFE), en_inshsw_lb2_rfe);
//    ret += LMS_WriteParam(device, LMS7param(EN_INSHSW_L_RFE), en_inshsw_l_rfe);
//    ret += LMS_WriteParam(device, LMS7param(EN_INSHSW_W_RFE), en_inshsw_w_rfe);
//    ret += LMS_WriteParam(device, LMS7param(SEL_PATH_RFE), sel_path_rfe);
//
//    if (ret < 0)
//    {
//        fprintf(stderr, "DeviceLimeSDR::setAntennaPath: cannot set channel #%lu to %d\n", chan, path);
//        return false;
//    }
//
//    //enable/disable the loopback path
//    const bool loopback = (path == (int) PATH_RFE_LB1) or (path == (int) PATH_RFE_LB2);
//
//    if (LMS_WriteParam(device, LMS7param(EN_LOOPB_TXPAD_TRF), loopback ? 1 : 0) < 0)
//    {
//        fprintf(stderr, "DeviceLimeSDR::setAntennaPath: cannot %sset loopback on channel #%lu\n", loopback ? "" : "re", chan);
//        return false;
//    }
//
//    //update external band-selection to match
//    //this->UpdateExternalBandSelect();
//
//    return true;

    switch ((PathRxRFE) path)
    {
    case PATH_RFE_LNAH:
        if (LMS_SetAntenna(device, LMS_CH_RX, chan, 1) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setRxAntennaPath: cannot set to LNAH\n");
            return false;
        }
        break;
    case PATH_RFE_LNAL:
        if (LMS_SetAntenna(device, LMS_CH_RX, chan, 2) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setRxAntennaPath: cannot set to LNAL\n");
            return false;
        }
        break;
    case PATH_RFE_LNAW:
        if (LMS_SetAntenna(device, LMS_CH_RX, chan, 3) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setRxAntennaPath: cannot set to LNAW\n");
            return false;
        }
        break;
    case PATH_RFE_LB1:
        if (LMS_SetAntenna(device, LMS_CH_TX, chan, 1) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setRxAntennaPath: cannot set to Loopback TX1\n");
            return false;
        }
        break;
    case PATH_RFE_LB2:
        if (LMS_SetAntenna(device, LMS_CH_TX, chan, 2) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setRxAntennaPath: cannot set to Loopback TX2\n");
            return false;
        }
        break;
    case PATH_RFE_RX_NONE:
    default:
        if (LMS_SetAntenna(device, LMS_CH_RX, chan, 0) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setRxAntennaPath: cannot set to none\n");
            return false;
        }
    }

    return true;
}

bool DeviceLimeSDR::setTxAntennaPath(lms_device_t *device, std::size_t chan, int path)
{
    switch ((PathTxRFE) path)
    {
    case PATH_RFE_TXRF1:
        if (LMS_SetAntenna(device, LMS_CH_TX, chan, 1) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setTxAntennaPath: cannot set to TXRF1\n");
            return false;
        }
        break;
    case PATH_RFE_TXRF2:
        if (LMS_SetAntenna(device, LMS_CH_TX, chan, 2) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setTxAntennaPath: cannot set to TXRF2\n");
            return false;
        }
        break;
    case PATH_RFE_TX_NONE:
    default:
        if (LMS_SetAntenna(device, LMS_CH_TX, chan, 0) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setTxAntennaPath: cannot set to none\n");
            return false;
        }
    }

    return true;
}

bool DeviceLimeSDR::setClockSource(lms_device_t *device, bool extClock, uint32_t extClockFrequency)
{
    if (extClock)
    {
        if (LMS_SetClockFreq(device, LMS_CLOCK_EXTREF, (float) extClockFrequency) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setClockSource: cannot set to external\n");
            return false;
        }
    }
    else
    {
        uint16_t vcoTrimValue;

        if (LMS_VCTCXORead(device, &vcoTrimValue))
        {
            fprintf(stderr, "DeviceLimeSDR::setClockSource: cannot read VCTXO trim value\n");
            return false;
        }

        if (LMS_VCTCXOWrite(device, vcoTrimValue))
        {
            fprintf(stderr, "DeviceLimeSDR::setClockSource: cannot write VCTXO trim value\n");
            return false;
        }
    }

    return true;
}
