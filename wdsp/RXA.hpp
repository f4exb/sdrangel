/*  RXA.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2015, 2016 Warren Pratt, NR0V
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

#ifndef wdsp_rxa_h
#define wdsp_rxa_h

#include <array>

#include "comm.hpp"
#include "unit.hpp"
#include "export.h"

namespace WDSP {

class METER;
class SHIFT;
class RESAMPLE;
class BANDPASS;
class BPS;
class NOTCHDB;
class NBP;
class BPSNBA;
class SNBA;
class SENDER;
class AMSQ;
class AMD;
class FMD;
class FMSQ;
class EQP;
class ANF;
class ANR;
class EMNR;
class WCPAGC;
class SPEAK;
class MPEAK;
class PANEL;
class SIPHON;
class CBL;
class SSQL;
class ANB;
class NOB;
class BufferProbe;

class WDSP_API RXA : public Unit
{
public:
    enum rxaMode
    {
        RXA_LSB,
        RXA_USB,
        RXA_DSB,
        RXA_CWL,
        RXA_CWU,
        RXA_FM,
        RXA_AM,
        RXA_DIGU,
        RXA_SPEC,
        RXA_DIGL,
        RXA_SAM,
        RXA_DRM
    };

    enum rxaMeterType
    {
        RXA_S_PK,
        RXA_S_AV,
        RXA_ADC_PK,
        RXA_ADC_AV,
        RXA_AGC_GAIN,
        RXA_AGC_PK,
        RXA_AGC_AV,
        RXA_METERTYPE_LAST
    };

    int mode;
    std::array<double, RXA_METERTYPE_LAST>  meter;

    ANB *anb;
    NOB *nob;
    SHIFT *shift;
    RESAMPLE *rsmpin;
    METER *adcmeter;
    NOTCHDB *ndb;
    NBP *nbp0;
    BPSNBA *bpsnba;
    SENDER *sender;
    METER *smeter;
    AMSQ *amsq;
    AMD *amd;
    FMD *fmd;
    FMSQ *fmsq;
    SNBA *snba;
    EQP *eqp;
    ANF *anf;
    ANR *anr;
    EMNR *emnr;
    WCPAGC *agc;
    METER *agcmeter;
    BANDPASS *bp1;
    SIPHON *sip1;
    CBL *cbl;
    SPEAK *speak;
    MPEAK *mpeak;
    SSQL *ssql;
    PANEL *panel;
    RESAMPLE *rsmpout;

    RXA(
        int in_rate,                // input samplerate
        int out_rate,               // output samplerate
        int dsp_rate,               // sample rate for mainstream dsp processing
        int dsp_size                // number complex samples processed per buffer in mainstream dsp processing
    );
    RXA(const RXA&) = delete;
    RXA& operator=(const RXA& other) = delete;
    virtual ~RXA();

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
    void setSpectrumProbe(BufferProbe *_spectrumProbe);

    // RXA Properties
    void setMode (int mode);
    void resCheck ();
    void bp1Check (int amd_run, int snba_run, int emnr_run, int anf_run, int anr_run);
    void bp1Set ();
    void bpsnbaCheck (int mode, int notch_run);
    void bpsnbaSet ();
    // NOTCHDB, NBP, SNBA
    void updateNBPFiltersLightWeight();
    void updateNBPFilters();
    int nbpAddNotch(int notch, double fcenter, double fwidth, int active);
    int nbpGetNotch(int notch, double* fcenter, double* fwidth, int* active) const;
    int nbpDeleteNotch(int notch);
    int nbpEditNotch(int notch, double fcenter, double fwidth, int active);
    void nbpGetNumNotches(int* nnotches) const;
    void nbpSetTuneFrequency(double tunefreq);
    void nbpSetShiftFrequency(double shift);
    void nbpSetNotchesRun(int run);
    void nbpSetWindow(int wintype);
    void nbpSetAutoIncrease(int autoincr);
    // AMD
    void setAMDRun(int run);
    // SNBA
    void setSNBARun(int run);
    // ANF
    void setANFRun(int run);
    void setANFPosition(int position);
    // ANR
    void setANRRun(int run);
    void setANRPosition(int position);
    // EMNR
    void setEMNRRun(int run);
    void setEMNRPosition(int position);
    // WCPAGC
    void setAGCThresh(double thresh, double size, double rate);
    void getAGCThresh(double *thresh, double size, double rate) const;

    // Collectives
    void setPassband(float f_low, float f_high);
    void setNC(int nc);
    void setMP(int mp);
};

} // namespace WDSP

#endif
