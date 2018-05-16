/*
 By Daniel Klostermann
 Iowa Hills Software, LLC  IowaHills.com
 If you find a problem, please leave a note at:
 http://www.iowahills.com/feedbackcomments.html
 June 6, 2016

 ShowMessage is a C++ Builder function, and it usage has been commented out.
 If you are using C++ Builder, include vcl.h for ShowMessage.
 Otherwise replace ShowMessage with something appropriate for your compiler.
 */

#include "FFTCode.h"
#include <math.h>

namespace kitiirfir
{

//---------------------------------------------------------------------------
// This calculates the required FFT size for a given number of points.
int RequiredFFTSize(int NumPts) {
    int N = MINIMUM_FFT_SIZE;
    while (N < NumPts && N < MAXIMUM_FFT_SIZE) {
        N *= 2;
    }
    return N;
}

//---------------------------------------------------------------------------

// This verifies that the FFT Size N = 2^M.   M is returned
// N must be >= 8 for the Twiddle calculations
int IsValidFFTSize(int N) {
    if (N < MINIMUM_FFT_SIZE || N > MAXIMUM_FFT_SIZE || (N & (N - 1)) != 0)
        return (0);   // N & (N - 1) ensures a power of 2
    return ((int) (log((double) N) / M_LN2 + 0.5));    // return M where N = 2^M
}

//---------------------------------------------------------------------------

// Fast Fourier Transform
// This code puts DC in bin 0 and scales the output of a forward transform by 1/N.
// InputR and InputI are the real and imaginary input arrays of length N.
// The output values are returned in the Input arrays.
// TTransFormType is either FORWARD or INVERSE (defined in the header file)
// 256 pts in 50 us
void FFT(double *InputR, double *InputI, int N, TTransFormType Type) {
    int j, LogTwoOfN, *RevBits;
    double *BufferR, *BufferI, *TwiddleR, *TwiddleI;
    double OneOverN;

    // Verify the FFT size and type.
    LogTwoOfN = IsValidFFTSize(N);
    if (LogTwoOfN == 0 || (Type != FORWARD && Type != INVERSE)) {
        // ShowMessage("Invalid FFT type or size.");
        return;
    }

    // Memory allocation for all the arrays.
    BufferR = new double[N];
    BufferI = new double[N];
    TwiddleR = new double[N / 2];
    TwiddleI = new double[N / 2];
    RevBits = new int[N];

    if (BufferR == NULL || BufferI == NULL || TwiddleR == NULL
            || TwiddleI == NULL || RevBits == NULL) {
        // ShowMessage("FFT Memory Allocation Error");
        return;
    }

    ReArrangeInput(InputR, InputI, BufferR, BufferI, RevBits, N);
    FillTwiddleArray(TwiddleR, TwiddleI, N, Type);
    Transform(InputR, InputI, BufferR, BufferI, TwiddleR, TwiddleI, N);

    // The ReArrangeInput function swapped Input[] and Buffer[]. Then Transform()
    // swapped them again, LogTwoOfN times. Ultimately, these swaps must be done
    // an even number of times, or the pointer to Buffer gets returned.
    // So we must do one more swap here, for N = 16, 64, 256, 1024, ...
    OneOverN = 1.0;
    if (Type == FORWARD)
        OneOverN = 1.0 / (double) N;

    if (LogTwoOfN % 2 == 1) {
        for (j = 0; j < N; j++)
            InputR[j] = InputR[j] * OneOverN;
        for (j = 0; j < N; j++)
            InputI[j] = InputI[j] * OneOverN;
    } else // if(LogTwoOfN % 2 == 0) then the results are still in Buffer.
    {
        for (j = 0; j < N; j++)
            InputR[j] = BufferR[j] * OneOverN;
        for (j = 0; j < N; j++)
            InputI[j] = BufferI[j] * OneOverN;
    }

    delete[] BufferR;
    delete[] BufferI;
    delete[] TwiddleR;
    delete[] TwiddleI;
    delete[] RevBits;
}
//---------------------------------------------------------------------------

// This puts the input arrays in bit reversed order.
// The while loop generates an array of bit reversed numbers. e.g.
// e.g. N=8: RevBits = 0,4,2,6,1,5,3,7   N=16: RevBits = 0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15
void ReArrangeInput(double *InputR, double *InputI, double *BufferR,
        double *BufferI, int *RevBits, int N) {
    int j, k, J, K;

    J = N / 2;
    K = 1;
    RevBits[0] = 0;
    while (J >= 1) {
        for (k = 0; k < K; k++) {
            RevBits[k + K] = RevBits[k] + J;
        }
        K *= 2;
        J /= 2;
    }

    // Move the rearranged input values to Buffer.
    // Take note of the pointer swaps at the top of the transform algorithm.
    for (j = 0; j < N; j++) {
        BufferR[j] = InputR[RevBits[j]];
        BufferI[j] = InputI[RevBits[j]];
    }

}

//---------------------------------------------------------------------------

/*
 The Pentium takes a surprising amount of time to calculate the sine and cosine.
 You may want to make the twiddle arrays static if doing repeated FFTs of the same size.
 This uses 4 fold symmetry to calculate the twiddle factors. As a result, this function
 requires a minimum FFT size of 8.
 */
void FillTwiddleArray(double *TwiddleR, double *TwiddleI, int N,
        TTransFormType Type) {
    int j;
    double Theta, TwoPiOverN;

    TwoPiOverN = M_2PI / (double) N;

    if (Type == FORWARD) {
        TwiddleR[0] = 1.0;
        TwiddleI[0] = 0.0;
        TwiddleR[N / 4] = 0.0;
        TwiddleI[N / 4] = -1.0;
        TwiddleR[N / 8] = M_SQRT_2;
        TwiddleI[N / 8] = -M_SQRT_2;
        TwiddleR[3 * N / 8] = -M_SQRT_2;
        TwiddleI[3 * N / 8] = -M_SQRT_2;
        for (j = 1; j < N / 8; j++) {
            Theta = (double) j * -TwoPiOverN;
            TwiddleR[j] = cos(Theta);
            TwiddleI[j] = sin(Theta);
            TwiddleR[N / 4 - j] = -TwiddleI[j];
            TwiddleI[N / 4 - j] = -TwiddleR[j];
            TwiddleR[N / 4 + j] = TwiddleI[j];
            TwiddleI[N / 4 + j] = -TwiddleR[j];
            TwiddleR[N / 2 - j] = -TwiddleR[j];
            TwiddleI[N / 2 - j] = TwiddleI[j];
        }
    }

    else {
        TwiddleR[0] = 1.0;
        TwiddleI[0] = 0.0;
        TwiddleR[N / 4] = 0.0;
        TwiddleI[N / 4] = 1.0;
        TwiddleR[N / 8] = M_SQRT_2;
        TwiddleI[N / 8] = M_SQRT_2;
        TwiddleR[3 * N / 8] = -M_SQRT_2;
        TwiddleI[3 * N / 8] = M_SQRT_2;
        for (j = 1; j < N / 8; j++) {
            Theta = (double) j * TwoPiOverN;
            TwiddleR[j] = cos(Theta);
            TwiddleI[j] = sin(Theta);
            TwiddleR[N / 4 - j] = TwiddleI[j];
            TwiddleI[N / 4 - j] = TwiddleR[j];
            TwiddleR[N / 4 + j] = -TwiddleI[j];
            TwiddleI[N / 4 + j] = TwiddleR[j];
            TwiddleR[N / 2 - j] = -TwiddleR[j];
            TwiddleI[N / 2 - j] = TwiddleI[j];
        }
    }

}

//---------------------------------------------------------------------------

// The Fast Fourier Transform.
void Transform(double *InputR, double *InputI, double *BufferR, double *BufferI,
        double *TwiddleR, double *TwiddleI, int N) {
    int j, k, J, K, I, T;
    double *TempPointer;
    double TempR, TempI;

    J = N / 2;     // J increments down to 1
    K = 1;       // K increments up to N/2
    while (J > 0) // Loops Log2(N) times.
    {
        // Swap pointers, instead doing this: for(j=0; j<N; j++) Input[j] = Buffer[j];
        // We start with a swap because of the swap in ReArrangeInput.
        TempPointer = InputR;
        InputR = BufferR;
        BufferR = TempPointer;
        TempPointer = InputI;
        InputI = BufferI;
        BufferI = TempPointer;

        I = 0;
        for (j = 0; j < J; j++) {
            T = 0;
            for (k = 0; k < K; k++) // Loops N/2 times for every value of J and K
                    {
                TempR = InputR[K + I] * TwiddleR[T]
                        - InputI[K + I] * TwiddleI[T];
                TempI = InputR[K + I] * TwiddleI[T]
                        + InputI[K + I] * TwiddleR[T];
                BufferR[I] = InputR[I] + TempR;
                BufferI[I] = InputI[I] + TempI;
                BufferR[I + K] = InputR[I] - TempR;
                BufferI[I + K] = InputI[I] - TempI;
                I++;
                T += J;
            }
            I += K;
        }
        K *= 2;
        J /= 2;
    }

}

//-----------------------------------------------------------------------------------------------

/*
 The only difficulty in writing an FFT is figuring out how to calculate the various array indexes.
 This shows how the index values change when doing a 16 pt FFT.
 This print only has value if you compare it to a butterfly chart. Then you can
 see how an FFT works. Use a 16 point decimation in time butterfly chart. We have one here:
 http://www.iowahills.com/FFTCode.html

 Note: The code above uses real variables. This print out came from code using complex variables as shown here.
 Buffer[I]   = Input[I] + Input[K+I] * Twiddle[T];
 Buffer[I+K] = Input[I] - Input[K+I] * Twiddle[T];

 N = 16
 J = 8    K = 1
 Buffer[0]  = Input[0]  + Input[1]  * Twiddle[0]   I = 0
 Buffer[1]  = Input[0]  - Input[1]  * Twiddle[0]   I = 0
 Buffer[2]  = Input[2]  + Input[3]  * Twiddle[0]   I = 2
 Buffer[3]  = Input[2]  - Input[3]  * Twiddle[0]   I = 2
 Buffer[4]  = Input[4]  + Input[5]  * Twiddle[0]   etc.
 Buffer[5]  = Input[4]  - Input[5]  * Twiddle[0]
 Buffer[6]  = Input[6]  + Input[7]  * Twiddle[0]
 Buffer[7]  = Input[6]  - Input[7]  * Twiddle[0]
 Buffer[8]  = Input[8]  + Input[9]  * Twiddle[0]
 Buffer[9]  = Input[8]  - Input[9]  * Twiddle[0]
 Buffer[10] = Input[10] + Input[11] * Twiddle[0]
 Buffer[11] = Input[10] - Input[11] * Twiddle[0]
 Buffer[12] = Input[12] + Input[13] * Twiddle[0]
 Buffer[13] = Input[12] - Input[13] * Twiddle[0]
 Buffer[14] = Input[14] + Input[15] * Twiddle[0]
 Buffer[15] = Input[14] - Input[15] * Twiddle[0]

 J = 4    K = 2
 Buffer[0]  = Input[0]  + Input[2]  * Twiddle[0]
 Buffer[2]  = Input[0]  - Input[2]  * Twiddle[0]
 Buffer[1]  = Input[1]  + Input[3]  * Twiddle[4]
 Buffer[3]  = Input[1]  - Input[3]  * Twiddle[4]
 Buffer[4]  = Input[4]  + Input[6]  * Twiddle[0]
 Buffer[6]  = Input[4]  - Input[6]  * Twiddle[0]
 Buffer[5]  = Input[5]  + Input[7]  * Twiddle[4]
 Buffer[7]  = Input[5]  - Input[7]  * Twiddle[4]
 Buffer[8]  = Input[8]  + Input[10] * Twiddle[0]
 Buffer[10] = Input[8]  - Input[10] * Twiddle[0]
 Buffer[9]  = Input[9]  + Input[11] * Twiddle[4]
 Buffer[11] = Input[9]  - Input[11] * Twiddle[4]
 Buffer[12] = Input[12] + Input[14] * Twiddle[0]
 Buffer[14] = Input[12] - Input[14] * Twiddle[0]
 Buffer[13] = Input[13] + Input[15] * Twiddle[4]
 Buffer[15] = Input[13] - Input[15] * Twiddle[4]

 J = 2    K = 4
 Buffer[0]  = Input[0]  + Input[4]  * Twiddle[0]
 Buffer[4]  = Input[0]  - Input[4]  * Twiddle[0]
 Buffer[1]  = Input[1]  + Input[5]  * Twiddle[2]
 Buffer[5]  = Input[1]  - Input[5]  * Twiddle[2]
 Buffer[2]  = Input[2]  + Input[6]  * Twiddle[4]
 Buffer[6]  = Input[2]  - Input[6]  * Twiddle[4]
 Buffer[3]  = Input[3]  + Input[7]  * Twiddle[6]
 Buffer[7]  = Input[3]  - Input[7]  * Twiddle[6]
 Buffer[8]  = Input[8]  + Input[12] * Twiddle[0]
 Buffer[12] = Input[8]  - Input[12] * Twiddle[0]
 Buffer[9]  = Input[9]  + Input[13] * Twiddle[2]
 Buffer[13] = Input[9]  - Input[13] * Twiddle[2]
 Buffer[10] = Input[10] + Input[14] * Twiddle[4]
 Buffer[14] = Input[10] - Input[14] * Twiddle[4]
 Buffer[11] = Input[11] + Input[15] * Twiddle[6]
 Buffer[15] = Input[11] - Input[15] * Twiddle[6]

 J = 1    K = 8
 Buffer[0]  = Input[0]  + Input[8]  * Twiddle[0]
 Buffer[8]  = Input[0]  - Input[8]  * Twiddle[0]
 Buffer[1]  = Input[1]  + Input[9]  * Twiddle[1]
 Buffer[9]  = Input[1]  - Input[9]  * Twiddle[1]
 Buffer[2]  = Input[2]  + Input[10] * Twiddle[2]
 Buffer[10] = Input[2]  - Input[10] * Twiddle[2]
 Buffer[3]  = Input[3]  + Input[11] * Twiddle[3]
 Buffer[11] = Input[3]  - Input[11] * Twiddle[3]
 Buffer[4]  = Input[4]  + Input[12] * Twiddle[4]
 Buffer[12] = Input[4]  - Input[12] * Twiddle[4]
 Buffer[5]  = Input[5]  + Input[13] * Twiddle[5]
 Buffer[13] = Input[5]  - Input[13] * Twiddle[5]
 Buffer[6]  = Input[6]  + Input[14] * Twiddle[6]
 Buffer[14] = Input[6]  - Input[14] * Twiddle[6]
 Buffer[7]  = Input[7]  + Input[15] * Twiddle[7]
 Buffer[15] = Input[7]  - Input[15] * Twiddle[7]

 */

//-----------------------------------------------------------------------------------------------

// Discrete Fourier Transform ( textbook code )
// This takes the same arguments as the FFT function.
// 256 pts in 1.720 ms
void DFT(double *InputR, double *InputI, int N, int Type) {
    int j, k, n;
    double *SumR, *SumI, Sign, Arg;
    double *TwiddleReal, *TwiddleImag;

    SumR = new double[N];
    SumI = new double[N];
    TwiddleReal = new double[N];
    TwiddleImag = new double[N];

    if (SumR == NULL || SumI == NULL || TwiddleReal == NULL
            || TwiddleImag == NULL || (Type != FORWARD && Type != INVERSE)) {
        // ShowMessage("Incorrect DFT Type or unable to allocate memory");
        return;
    }

    // Calculate the twiddle factors and initialize the Sum arrays.
    if (Type == FORWARD)
        Sign = -1.0;
    else
        Sign = 1.0;

    for (j = 0; j < N; j++) {
        Arg = M_2PI * (double) j / (double) N;
        TwiddleReal[j] = cos(Arg);
        TwiddleImag[j] = Sign * sin(Arg);
        SumR[j] = SumI[j] = 0.0;
    }

    //Calculate the DFT
    for (j = 0; j < N; j++) // Sum index
        for (k = 0; k < N; k++) // Input index
                {
            n = (j * k) % N;
            SumR[j] += TwiddleReal[n] * InputR[k] - TwiddleImag[n] * InputI[k];
            SumI[j] += TwiddleReal[n] * InputI[k] + TwiddleImag[n] * InputR[k];
        }

    // Scale the result if doing a forward DFT, and move the result to the input arrays.
    if (Type == FORWARD) {
        for (j = 0; j < N; j++)
            InputR[j] = SumR[j] / (double) N;
        for (j = 0; j < N; j++)
            InputI[j] = SumI[j] / (double) N;
    } else  // Inverse DFT
    {
        for (j = 0; j < N; j++)
            InputR[j] = SumR[j];
        for (j = 0; j < N; j++)
            InputI[j] = SumI[j];
    }

    delete[] SumR;
    delete[] SumI;
    delete[] TwiddleReal;
    delete[] TwiddleImag;
}

//-----------------------------------------------------------------------------------------------

// This is a DFT for real valued samples. Since Samples is real, it can only do a forward DFT.
// The results are returned in OutputR  OutputI
// 256 pts in 700 us    30 us to calc the twiddles
void RealSigDFT(double *Samples, double *OutputR, double *OutputI, int N) {
    int j, k;
    double Arg, *TwiddleReal, *TwiddleImag;

    TwiddleReal = new double[N];
    TwiddleImag = new double[N];
    if (TwiddleReal == NULL || TwiddleImag == NULL) {
        // ShowMessage("Failed to allocate memory in RealSigDFT");
        return;
    }

    for (j = 0; j < N; j++) {
        Arg = M_2PI * (double) j / (double) N;
        TwiddleReal[j] = cos(Arg);
        TwiddleImag[j] = -sin(Arg);
    }

    // Compute the DFT.
    // We have a real input, so only do the pos frequencies. i.e. j<N/2
    for (j = 0; j <= N / 2; j++) {
        OutputR[j] = 0.0;
        OutputI[j] = 0.0;
        for (k = 0; k < N; k++) {
            OutputR[j] += Samples[k] * TwiddleReal[(j * k) % N];
            OutputI[j] += Samples[k] * TwiddleImag[(j * k) % N];
        }

        // Scale the result
        OutputR[j] /= (double) N;
        OutputI[j] /= (double) N;
    }

    // The neg freq components are the conj of the pos components because the input signal is real.
    for (j = 1; j < N / 2; j++) {
        OutputR[N - j] = OutputR[j];
        OutputI[N - j] = -OutputI[j];
    }

    delete[] TwiddleReal;
    delete[] TwiddleImag;
}

//---------------------------------------------------------------------------

// This is a single frequency DFT.
// This code uses iteration to calculate the Twiddle factors.
// To evaluate the frequency response of an FIR filter at Omega, set
// Samples[] = FirCoeff[]   N = NumTaps  0.0 <= Omega <= 1.0
// 256 pts in 15.6 us
double SingleFreqDFT(double *Samples, int N, double Omega) {
    int k;
    double SumR, SumI, zR, zI, TwiddleR, TwiddleI, Temp;

    TwiddleR = cos(Omega * M_PI);
    TwiddleI = -sin(Omega * M_PI);
    zR = 1.0;    // z, as in e^(j*omega)
    zI = 0.0;
    SumR = 0.0;
    SumI = 0.0;

    for (k = 0; k < N; k++) {
        SumR += Samples[k] * zR;
        SumI += Samples[k] * zI;

        // Calculate the complex exponential z by taking it to the kth power.
        Temp = zR * TwiddleR - zI * TwiddleI;
        zI = zR * TwiddleI + zI * TwiddleR;
        zR = Temp;
    }

    /*
     // This is the more conventional implementation of the loop above.
     // It is a bit more accurate, but slower.
     for(k=0; k<N; k++)
     {
     SumR += Samples[k] *  cos((double)k * Omega * M_PI);
     SumI += Samples[k] * -sin((double)k * Omega * M_PI);
     }
     */

    return (sqrt(SumR * SumR + SumI * SumI));
    // return( ComplexD(SumR, SumI) );// if phase is needed.
}

//---------------------------------------------------------------------------

// Goertzel is essentially a single frequency DFT, but without phase information.
// Its simplicity allows it to run about 3 times faster than a single frequency DFT.
// It is typically used to find a tone embedded in a signal. A DTMF tone for example.
// 256 pts in 6 us
double Goertzel(double *Samples, int N, double Omega) {
    int j;
    double Reg0, Reg1, Reg2;        // 3 shift registers
    double CosVal, Mag;
    Reg1 = Reg2 = 0.0;

    CosVal = 2.0 * cos(M_PI * Omega);
    for (j = 0; j < N; j++) {
        Reg0 = Samples[j] + CosVal * Reg1 - Reg2;
        Reg2 = Reg1;  // Shift the values.
        Reg1 = Reg0;
    }
    Mag = Reg2 * Reg2 + Reg1 * Reg1 - CosVal * Reg1 * Reg2;

    if (Mag > 0.0)
        Mag = sqrt(Mag);
    else
        Mag = 1.0E-12;

    return (Mag);
}

//---------------------------------------------------------------------------

/*
 These are the window definitions. These windows can be used for either
 FIR filter design or with an FFT for spectral analysis.
 For definitions, see this article:  http://en.wikipedia.org/wiki/Window_function

 This function has 6 inputs
 Data is the array, of length N, containing the data to to be windowed.
 This data is either an FIR filter sinc pulse, or the data to be analyzed by an fft.

 WindowType is an enum defined in the header file.
 e.g. wtKAISER, wtSINC, wtHANNING, wtHAMMING, wtBLACKMAN, ...

 Alpha sets the width of the flat top.
 Windows such as the Tukey and Trapezoid are defined to have a variably wide flat top.
 As can be seen by its definition, the Tukey is just a Hanning window with a flat top.
 Alpha can be used to give any of these windows a partial flat top, except the Flattop and Kaiser.
 Alpha = 0 gives the original window. (i.e. no flat top)
 To generate a Tukey window, use a Hanning with 0 < Alpha < 1
 To generate a Bartlett window (triangular), use a Trapezoid window with Alpha = 0.
 Alpha = 1 generates a rectangular window in all cases. (except the Flattop and Kaiser)


 Beta is used with the Kaiser, Sinc, and Sine windows only.
 These three windows are used primarily for FIR filter design. Then
 Beta controls the filter's transition bandwidth and the sidelobe levels.
 All other windows ignore Beta.

 UnityGain controls whether the gain of these windows is set to unity.
 Only the Flattop window has unity gain by design. The Hanning window, for example, has a gain
 of 1/2.  UnityGain = true  sets the gain to 1, which preserves the signal's energy level
 when these windows are used for spectral analysis.

 Don't use this with FIR filter design however. Since most of the enegy in an FIR sinc pulse
 is in the middle of the window, the window needs a peak amplitude of one, not unity gain.
 Setting UnityGain = true will simply cause the resulting FIR filter to have excess gain.

 If using these windows for FIR filters, start with the Kaiser, Sinc, or Sine windows and
 adjust Beta for the desired transition BW and sidelobe levels (set Alpha = 0).
 While the FlatTop is an excellent window for spectral analysis, don't use it for FIR filter design.
 It has a peak amplitude of ~ 4.7 which causes the resulting FIR filter to have about this much gain.
 It works poorly for FIR filters even if you adjust its peak amplitude.
 The Trapezoid also works poorly for FIR filter design.

 If using these windows with an fft for spectral analysis, start with the Hanning, Gauss, or Flattop.
 When choosing a window for spectral analysis, you must trade off between resolution and amplitude
 accuracy. The Hanning has the best resolution while the Flatop has the best amplitude accuracy.
 The Gauss is midway between these two for both accuracy and resolution. These three were
 the only windows available in the HP 89410A Vector Signal Analyzer. Which is to say, these three
 are the probably the best windows for general purpose signal analysis.
 */

void WindowData(double *Data, int N, TWindowType WindowType, double Alpha,
        double Beta, bool UnityGain) {
    if (WindowType == wtNONE)
        return;

    int j, M, TopWidth;
    double dM, *WinCoeff;

    if (WindowType == wtKAISER || WindowType == wtFLATTOP)
        Alpha = 0.0;

    if (Alpha < 0.0)
        Alpha = 0.0;
    if (Alpha > 1.0)
        Alpha = 1.0;

    if (Beta < 0.0)
        Beta = 0.0;
    if (Beta > 10.0)
        Beta = 10.0;

    WinCoeff = new double[N + 2];
    if (WinCoeff == NULL) {
        // ShowMessage("Failed to allocate memory in WindowData() ");
        return;
    }

    TopWidth = (int) (Alpha * (double) N);
    if (TopWidth % 2 != 0)
        TopWidth++;
    if (TopWidth > N)
        TopWidth = N;
    M = N - TopWidth;
    dM = M + 1;

    // Calculate the window for N/2 points, then fold the window over (at the bottom).
    // TopWidth points will be set to 1.
    if (WindowType == wtKAISER) {
        double Arg;
        for (j = 0; j < M; j++) {
            Arg = Beta * sqrt(1.0 - pow(((double) (2 * j + 2) - dM) / dM, 2.0));
            WinCoeff[j] = Bessel(Arg) / Bessel(Beta);
        }
    }

    else if (WindowType == wtSINC)  // Lanczos
            {
        for (j = 0; j < M; j++)
            WinCoeff[j] = Sinc((double) (2 * j + 1 - M) / dM * M_PI);
        for (j = 0; j < M; j++)
            WinCoeff[j] = pow(WinCoeff[j], Beta);
    }

    else if (WindowType == wtSINE)  // Hanning if Beta = 2
            {
        for (j = 0; j < M / 2; j++)
            WinCoeff[j] = sin((double) (j + 1) * M_PI / dM);
        for (j = 0; j < M / 2; j++)
            WinCoeff[j] = pow(WinCoeff[j], Beta);
    }

    else if (WindowType == wtHANNING) {
        for (j = 0; j < M / 2; j++)
            WinCoeff[j] = 0.5 - 0.5 * cos((double) (j + 1) * M_2PI / dM);
    }

    else if (WindowType == wtHAMMING) {
        for (j = 0; j < M / 2; j++)
            WinCoeff[j] = 0.54 - 0.46 * cos((double) (j + 1) * M_2PI / dM);
    }

    else if (WindowType == wtBLACKMAN) {
        for (j = 0; j < M / 2; j++) {
            WinCoeff[j] = 0.42 - 0.50 * cos((double) (j + 1) * M_2PI / dM)
                    + 0.08 * cos((double) (j + 1) * M_2PI * 2.0 / dM);
        }
    }

    // Defined at: http://www.bth.se/fou/forskinfo.nsf/0/130c0940c5e7ffcdc1256f7f0065ac60/$file/ICOTA_2004_ttr_icl_mdh.pdf
    else if (WindowType == wtFLATTOP) {
        for (j = 0; j <= M / 2; j++) {
            WinCoeff[j] = 1.0
                    - 1.93293488969227 * cos((double) (j + 1) * M_2PI / dM)
                    + 1.28349769674027
                            * cos((double) (j + 1) * M_2PI * 2.0 / dM)
                    - 0.38130801681619
                            * cos((double) (j + 1) * M_2PI * 3.0 / dM)
                    + 0.02929730258511
                            * cos((double) (j + 1) * M_2PI * 4.0 / dM);
        }
    }

    else if (WindowType == wtBLACKMAN_HARRIS) {
        for (j = 0; j < M / 2; j++) {
            WinCoeff[j] = 0.35875 - 0.48829 * cos((double) (j + 1) * M_2PI / dM)
                    + 0.14128 * cos((double) (j + 1) * M_2PI * 2.0 / dM)
                    - 0.01168 * cos((double) (j + 1) * M_2PI * 3.0 / dM);
        }
    }

    else if (WindowType == wtBLACKMAN_NUTTALL) {
        for (j = 0; j < M / 2; j++) {
            WinCoeff[j] = 0.3535819
                    - 0.4891775 * cos((double) (j + 1) * M_2PI / dM)
                    + 0.1365995 * cos((double) (j + 1) * M_2PI * 2.0 / dM)
                    - 0.0106411 * cos((double) (j + 1) * M_2PI * 3.0 / dM);
        }
    }

    else if (WindowType == wtNUTTALL) {
        for (j = 0; j < M / 2; j++) {
            WinCoeff[j] = 0.355768
                    - 0.487396 * cos((double) (j + 1) * M_2PI / dM)
                    + 0.144232 * cos((double) (j + 1) * M_2PI * 2.0 / dM)
                    - 0.012604 * cos((double) (j + 1) * M_2PI * 3.0 / dM);
        }
    }

    else if (WindowType == wtKAISER_BESSEL) {
        for (j = 0; j <= M / 2; j++) {
            WinCoeff[j] = 0.402 - 0.498 * cos(M_2PI * (double) (j + 1) / dM)
                    + 0.098 * cos(2.0 * M_2PI * (double) (j + 1) / dM)
                    + 0.001 * cos(3.0 * M_2PI * (double) (j + 1) / dM);
        }
    }

    else if (WindowType == wtTRAPEZOID) // Rectangle for Alpha = 1  Triangle for Alpha = 0
            {
        int K = M / 2;
        if (M % 2)
            K++;
        for (j = 0; j < K; j++)
            WinCoeff[j] = (double) (j + 1) / (double) K;
    }

    // This definition is from http://en.wikipedia.org/wiki/Window_function (Gauss Generalized normal window)
    // We set their p = 2, and use Alpha in the numerator, instead of Sigma in the denominator, as most others do.
    // Alpha = 2.718 puts the Gauss window response midway between the Hanning and the Flattop (basically what we want).
    // It also gives the same BW as the Gauss window used in the HP 89410A Vector Signal Analyzer.
    else if (WindowType == wtGAUSS) {
        for (j = 0; j < M / 2; j++) {
            WinCoeff[j] = ((double) (j + 1) - dM / 2.0) / (dM / 2.0) * 2.7183;
            WinCoeff[j] *= WinCoeff[j];
            WinCoeff[j] = exp(-WinCoeff[j]);
        }
    }

    else // Error.
    {
        // ShowMessage("Incorrect window type in WindowFFTData");
        delete[] WinCoeff;
        return;
    }

    // Fold the coefficients over.
    for (j = 0; j < M / 2; j++)
        WinCoeff[N - j - 1] = WinCoeff[j];

    // This is the flat top if Alpha > 0. Cannot be applied to a Kaiser or Flat Top.
    if (WindowType != wtKAISER && WindowType != wtFLATTOP) {
        for (j = M / 2; j < N - M / 2; j++)
            WinCoeff[j] = 1.0;
    }

    // UnityGain = true will set the gain of these windows to 1. Don't use this with FIR filter design.
    if (UnityGain) {
        double Sum = 0.0;
        for (j = 0; j < N; j++)
            Sum += WinCoeff[j];
        Sum /= (double) N;
        if (Sum != 0.0)
            for (j = 0; j < N; j++)
                WinCoeff[j] /= Sum;
    }

    // Apply the window to the data.
    for (j = 0; j < N; j++)
        Data[j] *= WinCoeff[j];

    delete[] WinCoeff;

}

//---------------------------------------------------------------------------

// This gets used with the Kaiser window.
double Bessel(double x) {
    double Sum = 0.0, XtoIpower;
    int i, j, Factorial;
    for (i = 1; i < 10; i++) {
        XtoIpower = pow(x / 2.0, (double) i);
        Factorial = 1;
        for (j = 1; j <= i; j++)
            Factorial *= j;
        Sum += pow(XtoIpower / (double) Factorial, 2.0);
    }
    return (1.0 + Sum);
}

//-----------------------------------------------------------------------------

// This gets used with the Sinc window.
double Sinc(double x) {
    if (x > -1.0E-5 && x < 1.0E-5)
        return (1.0);
    return (sin(x) / x);
}

//---------------------------------------------------------------------------

} // namespace
