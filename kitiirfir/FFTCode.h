//---------------------------------------------------------------------------

#ifndef FFTCodeH
#define FFTCodeH

#define M_2PI     6.28318530717958647692   // 2*Pi
#define M_SQRT_2  0.707106781186547524401  // sqrt(2)/2
#define MAXIMUM_FFT_SIZE  1048576
#define MINIMUM_FFT_SIZE  8

namespace kitiirfir
{

//---------------------------------------------------------------------------
// Must retain the order on the 1st line for legacy FIR code.
enum TWindowType {
    wtFIRSTWINDOW,
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

enum TTransFormType {
    FORWARD, INVERSE
};

int RequiredFFTSize(int NumPts);
int IsValidFFTSize(int x);
void FFT(double *InputR, double *InputI, int N, TTransFormType Type);
void ReArrangeInput(double *InputR, double *InputI, double *BufferR,
        double *BufferI, int *RevBits, int N);
void FillTwiddleArray(double *TwiddleR, double *TwiddleI, int N,
        TTransFormType Type);
void Transform(double *InputR, double *InputI, double *BufferR, double *BufferI,
        double *TwiddleR, double *TwiddleI, int N);
void DFT(double *InputR, double *InputI, int N, int Type);
void RealSigDFT(double *Samples, double *OutputR, double *OutputI, int N);
double SingleFreqDFT(double *Samples, int N, double Omega);
double Goertzel(double *Samples, int N, double Omega);
void WindowData(double *Data, int N, TWindowType WindowType, double Alpha,
        double Beta, bool UnityGain);
double Bessel(double x);
double Sinc(double x);

} // namespace

#endif

