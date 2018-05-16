//---------------------------------------------------------------------------

#ifndef FIRFilterCodeH
#define FIRFilterCodeH
#include "FFTCode.h"   // For the definition of TWindowType
//---------------------------------------------------------------------------

#define MAX_NUMTAPS 256
#define M_2PI  6.28318530717958647692
#define NUM_FREQ_ERR_PTS  1000    // these are only used in the FIRFreqError function.
#define dNUM_FREQ_ERR_PTS 1000.0

namespace kitiirfir
{

enum TFIRPassTypes {
    firLPF, firHPF, firBPF, firNOTCH, firALLPASS, firNOT_FIR
};

void FilterWithFIR(double *FirCoeff, int NumTaps, double *Signal,
        double *FilteredSignal, int NumSigPts);
void FilterWithFIR2(double *FirCoeff, int NumTaps, double *Signal,
        double *FilteredSignal, int NumSigPts);
void RectWinFIR(double *FirCoeff, int NumTaps, TFIRPassTypes PassType,
        double OmegaC, double BW);
void WindowData(double *Data, int N, TWindowType WindowType, double Alpha,
        double Beta, bool UnityGain);
void FIRFreqError(double *Coeff, int NumTaps, int PassType, double *OmegaC,
        double *BW);
void FIRFilterWindow(double *FIRCoeff, int N, TWindowType WindowType,
        double Beta);
void AdjustDelay(double *FirCoeff, int NumTaps, double Delay);

} // namespace

#endif
