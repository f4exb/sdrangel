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

#include "comm.hpp"
#include "unit.hpp"

#include "ammod.hpp"
#include "meter.hpp"
#include "resample.hpp"
#include "patchpanel.hpp"
#include "amsq.hpp"
#include "eqp.hpp"
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
    long upslew;

    METER *micmeter;
    METER *eqmeter;
    METER *lvlrmeter;
    METER *cfcmeter;
    METER *compmeter;
    METER *alcmeter;
    METER *outmeter;
    RESAMPLE *rsmpin;
    RESAMPLE *rsmpout;
    PANEL *panel;
    AMSQ *amsq;
    EQP *eqp;
    PHROT *phrot;
    CFCOMP *cfcomp;
    COMPRESSOR *compressor;
    BANDPASS *bp0;
    BANDPASS *bp1;
    BANDPASS *bp2;
    BPS *bps0;
    BPS *bps1;
    BPS *bps2;
    OSCTRL *osctrl;
    WCPAGC *leveler;
    WCPAGC *alc;
    AMMOD *ammod;
    EMPHP *preemph;
    FMMOD *fmmod;
    SIPHON *sip1;
    GEN *gen0;
    GEN *gen1;
    USLEW *uslew;
    struct
    {
        IQC *p0, *p1;
        // p0 for dsp-synchronized reference, p1 for other
    } iqc;
    CFIR *cfir;

    TXA(
        int in_rate,                // input samplerate
        int out_rate,               // output samplerate
        int dsp_rate,               // sample rate for mainstream dsp processing
        int dsp_size                // number complex samples processed per buffer in mainstream dsp processing
    );
    TXA(const TXA&) = delete;
    TXA& operator=(const TXA& other) = delete;
    virtual ~TXA();

    void flush();
    void execute();
    void setInputSamplerate(int _in_rate);
    void setOutputSamplerate(int _out_rate);
    void setDSPSamplerate(int _dsp_rate);
    void setDSPBuffsize(int _dsp_size);
    int get_insize() const { return Unit::dsp_insize; }
    int get_outsize() const { return Unit::dsp_outsize; }
    float *get_inbuff() { return Unit::inbuff; }
    float *get_outbuff() { return Unit::outbuff; }

    // TXA Properties
    void setMode(int mode);
    void setBandpassFreqs(float f_low, float f_high);
    void setBandpassNC(int nc);
    void setBandpassMP(int mp);
    // BPS
    static void SetBPSRun (TXA& txa, int run);
    static void SetBPSFreqs (TXA& txa, double low, double high);
    static void SetBPSWindow (TXA& txa, int wintype);
    // COMPRESSOR
    static void SetCompressorRun (TXA& txa, int run);
    // OSCTRL
    static void SetosctrlRun (TXA& txa, int run);
    // IQC
    static void GetiqcValues (TXA& txa, std::vector<double>& cm, std::vector<double>& cc, std::vector<double>& cs);
    static void SetiqcValues (TXA& txa, const std::vector<double>& cm, const std::vector<double>& cc, const std::vector<double>& cs);
    static void SetiqcSwap (TXA& txa, const std::vector<double>& cm, const std::vector<double>& cc, const std::vector<double>& cs);
    static void SetiqcStart (TXA& txa, const std::vector<double>& cm, const std::vector<double>& cc, const std::vector<double>& cs);
    static void SetiqcEnd (TXA& txa);
    static void GetiqcDogCount (TXA& txa, int* count);
    static void SetiqcDogCount (TXA& txa, int  count);

    // Collectives
    void setNC(int nc);
    void setMP(int mp);
    void setFMAFFilter(float low, float high);
    void setupBPFilters();
    int uslewCheck();

private:
    void resCheck();
};

} // namespace WDSP

#endif
