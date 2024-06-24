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

#include <QRecursiveMutex>

#include "comm.hpp"
#include "unit.hpp"
#include "export.h"

namespace WDSP {

class METER;
class SHIFT;
class RESAMPLE;
class GEN;
class BANDPASS;
class BPS;
class SNB;
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
    double meter[RXA_METERTYPE_LAST];
    QRecursiveMutex *pmtupdate[RXA_METERTYPE_LAST];
    struct
    {
        METER *p;
    } smeter, adcmeter, agcmeter;
    struct
    {
        SHIFT *p;
    } shift;
    struct
    {
        RESAMPLE *p;
    } rsmpin, rsmpout;
    struct
    {
        GEN *p;
    } gen0;
    struct
    {
        BANDPASS *p;
    } bp1;
    struct
    {
        BPS *p;
    } bps1;
    struct
    {
        NOTCHDB *p;
    } ndb;
    struct
    {
        NBP *p;
    } nbp0;
    struct
    {
        BPSNBA *p;
    } bpsnba;
    struct
    {
        SNBA *p;
    } snba;
    struct
    {
        SENDER *p;
    } sender;
    struct
    {
        AMSQ *p;
    } amsq;
    struct
    {
        AMD *p;
    } amd;
    struct
    {
        FMD *p;
    } fmd;
    struct
    {
        FMSQ *p;
    } fmsq;
    struct
    {
        EQP *p;
    } eqp;
    struct
    {
        ANF *p;
    } anf;
    struct
    {
        ANR *p;
    } anr;
    struct
    {
        EMNR *p;
    } emnr;
    struct
    {
        WCPAGC *p;
    } agc;
    struct
    {
        SPEAK *p;
    } speak;
    struct
    {
        MPEAK *p;
    } mpeak;
    struct
    {
        PANEL *p;
    } panel;
    struct
    {
        SIPHON *p;
    } sip1;
    struct
    {
        CBL *p;
    } cbl;
    struct
    {
        SSQL *p;
    } ssql;

    static RXA* create_rxa (
        int in_rate,                // input samplerate
        int out_rate,               // output samplerate
        int dsp_rate,               // sample rate for mainstream dsp processing
        int dsp_size                // number complex samples processed per buffer in mainstream dsp processing
    );
    static void destroy_rxa (RXA *rxa);
    static void flush_rxa (RXA *rxa);
    static void xrxa (RXA *rxa);
    int get_insize() const { return dsp_insize; }
    int get_outsize() const { return dsp_outsize; }
    double *get_inbuff() { return inbuff; }
    double *get_outbuff() { return outbuff; }
    void setSpectrumProbe(BufferProbe *_spectrumProbe);
    static void setInputSamplerate (RXA *rxa, int in_rate);
    static void setOutputSamplerate (RXA *rxa, int out_rate);
    static void setDSPSamplerate (RXA *rxa, int dsp_rate);
    static void setDSPBuffsize (RXA *rxa, int dsp_size);

    // RXA Properties
    static void SetMode (RXA& rxa, int mode);
    static void ResCheck (RXA& rxa);
    static void bp1Check (RXA& rxa, int amd_run, int snba_run, int emnr_run, int anf_run, int anr_run);
    static void bp1Set (RXA& rxa);
    static void bpsnbaCheck (RXA& rxa, int mode, int notch_run);
    static void bpsnbaSet (RXA& rxa);

    // Collectives
    static void SetPassband (RXA& rxa, double f_low, double f_high);
    static void SetNC (RXA& rxa, int nc);
    static void SetMP (RXA& rxa, int mp);

private:
    double* inbuff;
    double* midbuff;
    double* outbuff;
};

} // namespace WDSP

#endif
