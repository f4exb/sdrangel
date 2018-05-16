//---------------------------------------------------------------------------

#ifndef NewParksMcClellanH
#define NewParksMcClellanH

//---------------------------------------------------------------------------
#define PARKS_BIG 1100          // Per the original code, this must be > 8 * MaxNumTaps = 8 * 128 = 1024
#define PARKS_SMALL 256         // This needs to be greater than or equal to MAX_NUM_PARKS_TAPS
#define MAX_NUM_PARKS_TAPS 127  // This was the limit set in the original code.
#define ITRMAX 50               // Max Number of Iterations. Some Notch and BPF are running ~ 43
#define MIN_TEST_VAL 1.0E-6     // Min value used in LeGrangeInterp and GEE

// If the FIRFilterCode.cpp file is in the project along with this NewParksMcClellan file,
// we need to include FIRFilterCode.h for the TFIRPassTypes enum.
#include "FIRFilterCode.h"
//enum TFIRPassTypes {firLPF, firHPF, firBPF, firNOTCH, ftNOT_FIR};

namespace kitiirfir
{

void NewParksMcClellan(double *FirCoeff, int NumTaps, TFIRPassTypes PassType, double OmegaC, double BW, double ParksWidth);
void CalcParkCoeff2(int NBANDS, int NFILT, double *FirCoeff);
double LeGrangeInterp2(int K, int N, int M);
double GEE2(int K, int N);
int Remez2(int GridIndex);
bool ErrTest(int k, int Nut, double Comp, double *Err);
void CalcCoefficients(void);

} // namespace

#endif
