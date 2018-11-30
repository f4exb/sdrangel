/*
 July 15, 2015
 Iowa Hills Software LLC
 http://www.iowahills.com

 If you find a problem with this code, please leave us a note on:
 http://www.iowahills.com/feedbackcomments.html

 Source: ~Projects\Common\BasicFIRFilterCode.cpp

 This generic FIR filter code is described in most textbooks.
 e.g. Discrete Time Signal Processing, Oppenheim and Shafer

 A nice paper on this topic is:
 http://dea.brunel.ac.uk/cmsp/Home_Saeed_Vaseghi/Chapter05-DigitalFilters.pdf

 This code first generates either a low pass, high pass, band pass, or notch
 impulse response for a rectangular window. It then applies a window to this
 impulse response.

 There are several windows available, including the Kaiser, Sinc, Hanning,
 Blackman, and Hamming. Of these, the Kaiser and Sinc are probably the most useful
 for FIR filters because their sidelobe levels can be controlled with the Beta parameter.

 This is a typical function call:
 BasicFIR(FirCoeff, NumTaps, PassType, OmegaC, BW, wtKAISER, Beta);
 BasicFIR(FirCoeff, 33, LPF, 0.2, 0.0, wtKAISER, 3.2);
 33 tap, low pass, corner frequency at 0.2, BW=0 (ignored in the low pass code),
 Kaiser window, Kaiser Beta = 3.2

 These variables should be defined similar to this:
 double FirCoeff[MAXNUMTAPS];
 int NumTaps;                        NumTaps can be even or odd, but must be less than the FirCoeff array size.
 TPassTypeName PassType;             PassType is an enum defined in the header file. LPF, HPF, BPF, or NOTCH
 double OmegaC  0.0 < OmegaC < 1.0   The filters corner freq, or center freq if BPF or NOTCH
 double BW      0.0 < BW < 1.0       The filters band width if BPF or NOTCH
 TWindowType WindowType;             WindowType is an enum defined in the header to be one of these.
 wtNONE, wtKAISER, wtSINC, wtHANNING, .... and others.
 double Beta;  0 <= Beta <= 10.0     Beta is used with the Kaiser, Sinc, and Sine windows only.
 It controls the transition BW and sidelobe level of the filters.


 If you want to use it, Kaiser originally defined Beta as follows.
 He derived its value based on the desired sidelobe level, dBAtten.
 double dBAtten, Beta, Beta1=0.0, Beta2=0.0;
 if(dBAtten < 21.0)dBAtten = 21.0;
 if(dBAtten > 50.0)Beta1 = 0.1102 * (dBAtten - 8.7);
 if(dBAtten >= 21.0 && dBAtten <= 50.0) Beta2 = 0.5842 * pow(dBAtten - 21.0, 0.4) + 0.07886 * (dBAtten - 21.0);
 Beta = Beta1 + Beta2;

 */

#ifndef _WFIR_H_
#define _WFIR_H_

#include "export.h"

class SDRBASE_API WFIR
{
public:
    enum TPassTypeName
    {
        LPF, HPF, BPF, NOTCH
    };

    enum TWindowType
    {
        wtNONE,
        wtKAISER,
        wtSINC,
        wtHANNING,
        wtHAMMING,
        wtBLACKMAN,
        wtFLATTOP,
        wtBLACKMAN_HARRIS,
        wtBLACKMAN_NUTTALL,
        wtNUTTALL,
        wtKAISER_BESSEL,
        wtTRAPEZOID,
        wtGAUSS,
        wtSINE,
        wtTEST
    };

    static void BasicFIR(double *FirCoeff, int NumTaps, TPassTypeName PassType,
            double OmegaC, double BW, TWindowType WindowType, double WinBeta);

private:
    static void WindowData(double *Data, int N, TWindowType WindowType,
            double Alpha, double Beta, bool UnityGain);
    static double Bessel(double x);
    static double Sinc(double x);
};

#endif
