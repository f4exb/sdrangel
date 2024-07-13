/*  TXA.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2017 Warren Pratt, NR0V
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

#ifndef wdsp_txa_h
#define wdsp_txa_h

#include <atomic>

#include "comm.hpp"
#include "unit.hpp"

#include "ammod.hpp"
#include "meter.hpp"
#include "resample.hpp"
#include "patchpanel.hpp"
#include "amsq.hpp"
#include "eq.hpp"
#include "iir.hpp"
#include "cfcomp.hpp"
#include "compress.hpp"
#include "bandpass.hpp"
#include "bps.hpp"
#include "osctrl.hpp"
#include "wcpAGC.hpp"
#include "emph.hpp"
#include "fmmod.hpp"
#include "siphon.hpp"
#include "gen.hpp"
#include "slew.hpp"
// #include "calcc.h"
#include "iqc.hpp"
#include "cfir.hpp"
#include "export.h"


namespace WDSP {

class METER;
class RESAMPLE;
class PANEL;
class AMSQ;
class EQP;
class PHROT;
class CFCOMP;
class COMPRESSOR;
class BANDPASS;
class BPS;
class OSCTRL;
class WCPAGC;
class AMMOD;
class EMPHP;
class FMMOD;
class SIPHON;
class GEN;
class USLEW;
class IQC;
class CFIR;

class WDSP_API TXA : public Unit
{
public:
    enum txaMode
    {
        TXA_LSB,
        TXA_USB,
        TXA_DSB,
        TXA_CWL,
        TXA_CWU,
        TXA_FM,
        TXA_AM,
        TXA_DIGU,
        TXA_SPEC,
        TXA_DIGL,
        TXA_SAM,
        TXA_DRM,
        TXA_AM_LSB,
        TXA_AM_USB
    };

    enum txaMeterType
    {
        TXA_MIC_PK,
        TXA_MIC_AV,
        TXA_EQ_PK,
        TXA_EQ_AV,
        TXA_LVLR_PK,
        TXA_LVLR_AV,
        TXA_LVLR_GAIN,
        TXA_CFC_PK,
        TXA_CFC_AV,
        TXA_CFC_GAIN,
        TXA_COMP_PK,
        TXA_COMP_AV,
        TXA_ALC_PK,
        TXA_ALC_AV,
        TXA_ALC_GAIN,
        TXA_OUT_PK,
        TXA_OUT_AV,
        TXA_METERTYPE_LAST
    };

    int mode;
    float f_low;
    float f_high;
    double meter[TXA_METERTYPE_LAST];
    std::atomic<long> upslew;

    struct
    {
        METER *p;
    } micmeter, eqmeter, lvlrmeter, cfcmeter, compmeter, alcmeter, outmeter;
    struct
    {
        RESAMPLE *p;
    } rsmpin, rsmpout;
    struct
    {
        PANEL *p;
    } panel;
    struct
    {
        AMSQ *p;
    } amsq;
    struct
    {
        EQP *p;
    } eqp;
    struct
    {
        PHROT *p;
    } phrot;
    struct
    {
        CFCOMP *p;
    } cfcomp;
    struct
    {
        COMPRESSOR *p;
    } compressor;
    struct
    {
        BANDPASS *p;
    } bp0, bp1, bp2;
    struct
    {
        BPS *p;
    } bps0, bps1, bps2;
    struct
    {
        OSCTRL *p;
    } osctrl;
    struct
    {
        WCPAGC *p;
    } leveler, alc;
    struct
    {
        AMMOD *p;
    } ammod;
    struct
    {
        EMPHP *p;
    } preemph;
    struct
    {
        FMMOD *p;
    } fmmod;
    struct
    {
        SIPHON *p;
    } sip1;
    struct
    {
        GEN *p;
    } gen0, gen1;
    struct
    {
        USLEW *p;
    } uslew;
    // struct
    // {
    //     CALCC *p;
    //     CRITICAL_SECTION cs_update;
    // } calcc;
    struct
    {
        IQC *p0, *p1;
        // p0 for dsp-synchronized reference, p1 for other
    } iqc;
    struct
    {
        CFIR *p;
    } cfir;

    static TXA* create_txa (
        int in_rate,                // input samplerate
        int out_rate,               // output samplerate
        int dsp_rate,               // sample rate for mainstream dsp processing
        int dsp_size                // number complex samples processed per buffer in mainstream dsp processing
    );
    static void destroy_txa (TXA *txa);
    static void flush_txa (TXA *txa);
    static void xtxa (TXA *txa);
    int get_insize() const { return dsp_insize; }
    int get_outsize() const { return dsp_outsize; }
    float *get_inbuff() { return inbuff; }
    float *get_outbuff() { return outbuff; }
    static void setInputSamplerate (TXA *txa, int in_rate);
    static void setOutputSamplerate (TXA *txa, int out_rate);
    static void setDSPSamplerate (TXA *txa, int dsp_rate);
    static void setDSPBuffsize (TXA *txa, int dsp_size);

    // TXA Properties
    static void SetMode (TXA& txa, int mode);
    static void SetBandpassFreqs (TXA& txa, float f_low, float f_high);

    // Collectives
    static void SetNC (TXA& txa, int nc);
    static void SetMP (TXA& txa, int mp);
    static void SetFMAFFilter (TXA& txa, float low, float high);
    static void SetupBPFilters (TXA& txa);
    static int UslewCheck (TXA& txa);

private:
    static void ResCheck (TXA& txa);
    float* inbuff;
    float* midbuff;
    float* outbuff;
};

} // namespace WDSP

#endif
