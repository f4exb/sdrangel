//---------------------------------------------------------------------------

#ifndef IIRFilterCodeH
#define IIRFilterCodeH
//---------------------------------------------------------------------------
#include "LowPassPrototypes.h"    // defines TFilterPoly and ARRAY_DIM
#define OVERFLOW_LIMIT  1.0E20

namespace kitiirfir
{

enum TIIRPassTypes {iirLPF, iirHPF, iirBPF, iirNOTCH, iirALLPASS};

struct TIIRCoeff {double a0[ARRAY_DIM]; double a1[ARRAY_DIM]; double a2[ARRAY_DIM]; double a3[ARRAY_DIM]; double a4[ARRAY_DIM];
               double b0[ARRAY_DIM]; double b1[ARRAY_DIM]; double b2[ARRAY_DIM]; double b3[ARRAY_DIM]; double b4[ARRAY_DIM];
               int NumSections; };

struct TIIRFilterParams { TIIRPassTypes IIRPassType;     // Defined above: Low pass, High Pass, etc.
                       double OmegaC;                 // The IIR filter's 3 dB corner freq for low pass and high pass, the center freq for band pass and notch.
                       double BW;                     // The IIR filter's 3 dB bandwidth for band pass and notch filters.
                       double dBGain;                 // Sets the Gain of the filter

                       // These define the low pass prototype to be used
                       TFilterPoly ProtoType;  // Butterworth, Cheby, etc.
                       int NumPoles;           // Pole count
                       double Ripple;          // Passband Ripple for the Elliptic and Chebyshev
                       double StopBanddB;      // Stop Band Attenuation in dB for the Elliptic and Inverse Chebyshev
                       double Gamma;           // Controls the transition bandwidth on the Adjustable Gauss. -1 <= Gamma <= 1
                     };

TIIRCoeff CalcIIRFilterCoeff(TIIRFilterParams IIRFilt);
void FilterWithIIR(TIIRCoeff IIRCoeff, double *Signal, double *FilteredSignal, int NumSigPts);
double SectCalc(int j, int k, double x, TIIRCoeff IIRCoeff);
void IIRFreqResponse(TIIRCoeff IIRCoeff, int NumSections, double *RealHofZ, double *ImagHofZ, int NumPts);

} // namespace

#endif
