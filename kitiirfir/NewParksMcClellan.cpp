/*
 By Daniel Klostermann
 Iowa Hills Software, LLC  IowaHills.com
 If you find a problem, please leave a note at:
 http://www.iowahills.com/feedbackcomments.html
 May 1, 2016

 ShowMessage is a C++ Builder function, and it usage has been commented out.
 If you are using C++ Builder, include vcl.h for ShowMessage.
 Otherwise replace ShowMessage with something appropriate for your compiler.

 This is a C translation of the Parks McClellan algorithm originally done in Fortran.

 The original fortran code came from the Parks McClellan article on Wikipedia.
 http://en.wikipedia.org/wiki/Parks%E2%80%93McClellan_filter_design_algorithm

 This code is quite different from the original. The original code had 69 goto statements,
 which made it nearly impossible to follow. And of course, it was Fortran code, so many changes
 had to be made regardless of style.

 Apparently, Fortran doesn't use global variables. Instead, is uses something called
 common memory space. e.g. COMMON PI2,AD,DEV,X,Y,GRID,DES,WT,ALPHA,IEXT,NFCNS,NGRID
 I simply converted these to globals. It isn't pretty, but for this purpose, who cares?

 The first step was to get a C version of the code working with as few changes as possible.
 That version is also available on:  http://www.iowahills.com/A7ExampleCodePage.html
 Then, in our desire to see if the code could be made more understandable, we decided to
 remove as many goto statements as possible. We checked our work by comparing the coefficients
 between this code and our original translation on more than 1000 filters while varying all the parameters.

 Ultimately, we were able to reduce the goto count from 69 to 7, all of which are in the Remez
 function. Of the 7 remaining, 3 of these are at the very bottom of the function, and go
 back to the very top of the function. These could have been removed, but our goal was to
 clarify the code, not restyle it, and since they are clear, we let them be.

 The other 4 goto statements are intertwined in a rather nasty way. We recommend you print out
 the Remez code, tape the sheets end to end, and trace out the goto's. It wasn't apparent to
 us that they can be removed without an extensive study of the code.

 For better or worse, we also removed any code that was obviously related to Hilbert transforms
 and Differentiators. We did this because we aren't interested in these, and we also don't
 believe this algorithm does a very good job with them (far too much ripple).

 We added the functions CalcCoefficients() and ErrTest() as a way to simplify things a bit.

 We also found 3 sections of code that never executed. Two of the sections were just a few lines
 that the goto's always went around. The third section represented nearly half of the CalcCoefficients()
 function. This statement always tested the same, which never allowed the code to execute.
 if(GRID[1] < 0.01 && GRID[NGRID] > 0.49) KKK = 1;
 This may be due to the 0.01 minimum width limit we set for the bands.

 Note our use of MIN_TEST_VAL. The original code wasn't guarding against division by zero.
 Limiting the return values as we have also helped the algorithm's convergence behavior.

 In an effort to improve readability, we made a large number of variable name changes and also
 deleted a large number of variables. We left many variable names in tact, in part as an aid when
 comparing to the original code, and in part because a better name wasn't obvious.

 This code is essentially straight c, and should compile with few, if any changes. Note the error
 message in CalcParkCoeff2. It warns of the possibility of convergence failure, but you will
 find that the iteration count NITER, isn't always an indicator of convergence problems when
 it is less than 3, as stated in the original Fortran code comments.

 If you find a problem with this code, please leave us a note on:
 http://www.iowahills.com/feedbackcomments.html

 */

#include "NewParksMcClellan.h"
#include <math.h>
#define M_2PI  6.28318530717958647692

namespace kitiirfir
{

//---------------------------------------------------------------------------

// Global variables.
int HalfTapCount, ExchangeIndex[PARKS_SMALL];
double LeGrangeD[PARKS_SMALL], Alpha[PARKS_SMALL], CosOfGrid[PARKS_SMALL],
        DesPlus[PARKS_SMALL];
double Coeff[PARKS_SMALL], Edge[PARKS_SMALL], BandMag[PARKS_SMALL],
        InitWeight[PARKS_SMALL];
double DesiredMag[PARKS_BIG], Grid[PARKS_BIG], Weight[PARKS_BIG];

void NewParksMcClellan(double *FirCoeff, int NumTaps, TFIRPassTypes PassType,
        double OmegaC, double BW, double ParksWidth) {
    if (NumTaps > MAX_NUM_PARKS_TAPS)
        return;
    int j, NumBands;

    // Note: There is no feedback to the caller if ParksWidth or NumTaps are modified here.
    if (PassType == firBPF || PassType == firNOTCH || NumTaps > 70) {
        if (ParksWidth > 0.15)
            ParksWidth = 0.15; // Else we have covergence problems.
    }

    if (PassType == firNOTCH || PassType == firHPF) {
        if (NumTaps % 2 == 0)
            NumTaps++;  // High pass and notch filters must have odd tap counts.
    }

    if (NumTaps > MAX_NUM_PARKS_TAPS)
        NumTaps = MAX_NUM_PARKS_TAPS;

    // It helps the algorithm a great deal if each band is at least 0.01 wide.
    // The weights used here came from the orig PM code.
    if (PassType == firLPF) {
        NumBands = 2;
        Edge[1] = 0.0;                       // Omega = 0
        Edge[2] = OmegaC;                    // Pass band edge
        if (Edge[2] < 0.01)
            Edge[2] = 0.01;
        if (Edge[2] > 0.98)
            Edge[2] = 0.98;
        Edge[3] = Edge[2] + ParksWidth;      // Stop band edge
        if (Edge[3] > 0.99)
            Edge[3] = 0.99;
        Edge[4] = 1.0;                       // Omega = Pi
        BandMag[1] = 1.0;
        BandMag[2] = 0.0;
        InitWeight[1] = 1.0;
        InitWeight[2] = 10.0;
    }

    if (PassType == firHPF) {
        NumBands = 2;
        Edge[1] = 0.0;                       // Omega = 0
        Edge[3] = OmegaC;                    // Pass band edge
        if (Edge[3] > 0.99)
            Edge[3] = 0.99;
        if (Edge[3] < 0.02)
            Edge[3] = 0.02;
        Edge[2] = Edge[3] - ParksWidth;      // Stop band edge
        if (Edge[2] < 0.01)
            Edge[2] = 0.01;
        Edge[4] = 1.0;                       // Omega = Pi
        BandMag[1] = 0.0;
        BandMag[2] = 1.0;
        InitWeight[1] = 10.0;
        InitWeight[2] = 1.0;
    }

    if (PassType == firBPF) {
        NumBands = 3;
        Edge[1] = 0.0;                       // Omega = 0
        Edge[3] = OmegaC - BW / 2.0;           // Left pass band edge.
        if (Edge[3] < 0.02)
            Edge[3] = 0.02;
        Edge[2] = Edge[3] - ParksWidth;      // Left stop band edge
        if (Edge[2] < 0.01)
            Edge[2] = 0.01;
        Edge[4] = OmegaC + BW / 2.0;           // Right pass band edge
        if (Edge[4] > 0.98)
            Edge[4] = 0.98;
        Edge[5] = Edge[4] + ParksWidth;      // Right stop band edge
        if (Edge[5] > 0.99)
            Edge[5] = 0.99;
        Edge[6] = 1.0;                       // Omega = Pi

        BandMag[1] = 0.0;
        BandMag[2] = 1.0;
        BandMag[3] = 0.0;
        InitWeight[1] = 10.0;
        InitWeight[2] = 1.0;
        InitWeight[3] = 10.0;
    }

    // This algorithm tends to have problems with narrow band notch filters.
    if (PassType == firNOTCH) {
        NumBands = 3;
        Edge[1] = 0.0;                        // Omega = 0
        Edge[3] = OmegaC - BW / 2.0;            // Left stop band edge.
        if (Edge[3] < 0.02)
            Edge[3] = 0.02;
        Edge[2] = Edge[3] - ParksWidth;       // Left pass band edge
        if (Edge[2] < 0.01)
            Edge[2] = 0.01;
        Edge[4] = OmegaC + BW / 2.0;            // Right stop band edge
        if (Edge[4] > 0.98)
            Edge[4] = 0.98;
        Edge[5] = Edge[4] + ParksWidth;       // Right pass band edge
        if (Edge[5] > 0.99)
            Edge[5] = 0.99;
        Edge[6] = 1.0;                        // Omega = Pi

        BandMag[1] = 1.0;
        BandMag[2] = 0.0;
        BandMag[3] = 1.0;
        InitWeight[1] = 1.0;
        InitWeight[2] = 10.0;
        InitWeight[3] = 1.0;
    }

    // Parks McClellan's edges are based on 2Pi, we are based on Pi.
    for (j = 1; j <= 2 * NumBands; j++)
        Edge[j] /= 2.0;

    CalcParkCoeff2(NumBands, NumTaps, FirCoeff);

}
//---------------------------------------------------------------------------

void CalcParkCoeff2(int NumBands, int TapCount, double *FirCoeff) {
    int j, k, GridCount, GridIndex, BandIndex, NumIterations;
    double LowFreqEdge, UpperFreq, TempVar, Change;
    bool OddNumTaps;
    GridCount = 16;               // Grid Density

    if (TapCount % 2)
        OddNumTaps = true;
    else
        OddNumTaps = false;

    HalfTapCount = TapCount / 2;
    if (OddNumTaps)
        HalfTapCount++;

    Grid[1] = Edge[1];
    LowFreqEdge = GridCount * HalfTapCount;
    LowFreqEdge = 0.5 / LowFreqEdge;
    j = 1;
    k = 1;
    BandIndex = 1;
    while (BandIndex <= NumBands) {
        UpperFreq = Edge[k + 1];
        while (Grid[j] <= UpperFreq) {
            TempVar = Grid[j];
            DesiredMag[j] = BandMag[BandIndex];
            Weight[j] = InitWeight[BandIndex];
            j++;
            ;
            Grid[j] = TempVar + LowFreqEdge;
        }

        Grid[j - 1] = UpperFreq;
        DesiredMag[j - 1] = BandMag[BandIndex];
        Weight[j - 1] = InitWeight[BandIndex];
        k += 2;
        BandIndex++;
        if (BandIndex <= NumBands)
            Grid[j] = Edge[k];
    }

    GridIndex = j - 1;
    if (!OddNumTaps && Grid[GridIndex] > (0.5 - LowFreqEdge))
        GridIndex--;

    if (!OddNumTaps) {
        for (j = 1; j <= GridIndex; j++) {
            Change = cos(M_PI * Grid[j]);
            DesiredMag[j] = DesiredMag[j] / Change;
            Weight[j] = Weight[j] * Change;
        }
    }

    TempVar = (double) (GridIndex - 1) / (double) HalfTapCount;
    for (j = 1; j <= HalfTapCount; j++) {
        ExchangeIndex[j] = (int) ((double) (j - 1) * TempVar + 1.0);
    }
    ExchangeIndex[HalfTapCount + 1] = GridIndex;

    NumIterations = Remez2(GridIndex);
    CalcCoefficients();

    // Calculate the impulse response.
    if (OddNumTaps) {
        for (j = 1; j <= HalfTapCount - 1; j++) {
            Coeff[j] = 0.5 * Alpha[HalfTapCount + 1 - j];
        }
        Coeff[HalfTapCount] = Alpha[1];
    } else {
        Coeff[1] = 0.25 * Alpha[HalfTapCount];
        for (j = 2; j <= HalfTapCount - 1; j++) {
            Coeff[j] =
                    0.25
                            * (Alpha[HalfTapCount + 1 - j]
                                    + Alpha[HalfTapCount + 2 - j]);
        }
        Coeff[HalfTapCount] = 0.5 * Alpha[1] + 0.25 * Alpha[2];
    }

    // Output section.
    for (j = 1; j <= HalfTapCount; j++)
        FirCoeff[j - 1] = Coeff[j];
    if (OddNumTaps)
        for (j = 1; j < HalfTapCount; j++)
            FirCoeff[HalfTapCount + j - 1] = Coeff[HalfTapCount - j];
    else
        for (j = 1; j <= HalfTapCount; j++)
            FirCoeff[HalfTapCount + j - 1] = Coeff[HalfTapCount - j + 1];

    // Display the iteration count.
    if (NumIterations < 3) {
        // ShowMessage("Parks McClellan unable to coverge");
    }

}

//---------------------------------------------------------------------------------------
int Remez2(int GridIndex) {
    int j, JET, K, k, NU, JCHNGE, K1, KNZ, KLOW, NUT, KUP;
    int NUT1 = 0, LUCK, KN, NITER;
    double Deviation, DNUM, DDEN, TempVar;
    double DEVL, COMP = 0.0, YNZ = 0.0, Y1 = 0.0, ERR;

    LUCK = 0;
    DEVL = -1.0;
    NITER = 1; // Init this to 1 to be consistent with the orig code.

    TOP_LINE:  // We come back to here from 3 places at the bottom.
    ExchangeIndex[HalfTapCount + 2] = GridIndex + 1;

    for (j = 1; j <= HalfTapCount + 1; j++) {
        TempVar = Grid[ExchangeIndex[j]];
        CosOfGrid[j] = cos(TempVar * M_2PI);
    }

    JET = (HalfTapCount - 1) / 15 + 1;
    for (j = 1; j <= HalfTapCount + 1; j++) {
        LeGrangeD[j] = LeGrangeInterp2(j, HalfTapCount + 1, JET);
    }

    DNUM = 0.0;
    DDEN = 0.0;
    K = 1;
    for (j = 1; j <= HalfTapCount + 1; j++) {
        k = ExchangeIndex[j];
        DNUM += LeGrangeD[j] * DesiredMag[k];
        DDEN += (double) K * LeGrangeD[j] / Weight[k];
        K = -K;
    }
    Deviation = DNUM / DDEN;

    NU = 1;
    if (Deviation > 0.0)
        NU = -1;
    Deviation = -(double) NU * Deviation;
    K = NU;
    for (j = 1; j <= HalfTapCount + 1; j++) {
        k = ExchangeIndex[j];
        TempVar = (double) K * Deviation / Weight[k];
        DesPlus[j] = DesiredMag[k] + TempVar;
        K = -K;
    }

    if (Deviation <= DEVL)
        return (NITER); // Ouch

    DEVL = Deviation;
    JCHNGE = 0;
    K1 = ExchangeIndex[1];
    KNZ = ExchangeIndex[HalfTapCount + 1];
    KLOW = 0;
    NUT = -NU;

    //Search for the extremal frequencies of the best approximation.

    j = 1;
    while (j < HalfTapCount + 2) {
        KUP = ExchangeIndex[j + 1];
        k = ExchangeIndex[j] + 1;
        NUT = -NUT;
        if (j == 2)
            Y1 = COMP;
        COMP = Deviation;

        if (k < KUP && !ErrTest(k, NUT, COMP, &ERR)) {
            L210: COMP = (double) NUT * ERR;
            for (k++; k < KUP; k++) {
                if (ErrTest(k, NUT, COMP, &ERR))
                    break; // for loop
                COMP = (double) NUT * ERR;
            }

            ExchangeIndex[j] = k - 1;
            j++;
            KLOW = k - 1;
            JCHNGE++;
            continue;  // while loop
        }

        k--;

        L225: k--;
        if (k <= KLOW) {
            k = ExchangeIndex[j] + 1;
            if (JCHNGE > 0) {
                ExchangeIndex[j] = k - 1;
                j++;
                KLOW = k - 1;
                JCHNGE++;
                continue;  // while loop
            } else  // JCHNGE <= 0
            {
                for (k++; k < KUP; k++) {
                    if (ErrTest(k, NUT, COMP, &ERR))
                        continue; // for loop
                    goto L210;
                }

                KLOW = ExchangeIndex[j];
                j++;
                continue; // while loop
            }
        }
        // Can't use a do while loop here, it would outdent the two continue statements.
        if (ErrTest(k, NUT, COMP, &ERR) && JCHNGE <= 0)
            goto L225;

        if (ErrTest(k, NUT, COMP, &ERR)) {
            KLOW = ExchangeIndex[j];
            j++;
            continue; // while loop
        }

        COMP = (double) NUT * ERR;

        L235: for (k--; k > KLOW; k--) {
            if (ErrTest(k, NUT, COMP, &ERR))
                break; // for loop
            COMP = (double) NUT * ERR;
        }

        KLOW = ExchangeIndex[j];
        ExchangeIndex[j] = k + 1;
        j++;
        JCHNGE++;
    }  // end while(j<HalfTapCount

    if (j == HalfTapCount + 2)
        YNZ = COMP;

    while (j <= HalfTapCount + 2) {
        if (K1 > ExchangeIndex[1])
            K1 = ExchangeIndex[1];
        if (KNZ < ExchangeIndex[HalfTapCount + 1])
            KNZ = ExchangeIndex[HalfTapCount + 1];
        NUT1 = NUT;
        NUT = -NU;
        k = 0;
        KUP = K1;
        COMP = YNZ * 1.00001;
        LUCK = 1;

        for (k++; k < KUP; k++) {
            if (ErrTest(k, NUT, COMP, &ERR))
                continue; // for loop
            j = HalfTapCount + 2;
            goto L210;
        }
        LUCK = 2;
        break;  // break while(j <= HalfTapCount+2) loop
    } // end while(j <= HalfTapCount+2)

    if (LUCK == 1 || LUCK == 2) {
        if (LUCK == 1) {
            if (COMP > Y1)
                Y1 = COMP;
            K1 = ExchangeIndex[HalfTapCount + 2];
        }

        k = GridIndex + 1;
        KLOW = KNZ;
        NUT = -NUT1;
        COMP = Y1 * 1.00001;

        for (k--; k > KLOW; k--) {
            if (ErrTest(k, NUT, COMP, &ERR))
                continue;  // for loop
            j = HalfTapCount + 2;
            COMP = (double) NUT * ERR;
            LUCK = 3;   // last time in this if(LUCK == 1 || LUCK == 2)
            goto L235;
        }

        if (LUCK == 2) {
            if (JCHNGE > 0 && NITER++ < ITRMAX)
                goto TOP_LINE;
            else
                return (NITER);
        }

        for (j = 1; j <= HalfTapCount; j++) {
            ExchangeIndex[HalfTapCount + 2 - j] = ExchangeIndex[HalfTapCount + 1
                    - j];
        }
        ExchangeIndex[1] = K1;
        if (NITER++ < ITRMAX)
            goto TOP_LINE;
    }  // end if(LUCK == 1 || LUCK == 2)

    KN = ExchangeIndex[HalfTapCount + 2];
    for (j = 1; j <= HalfTapCount; j++) {
        ExchangeIndex[j] = ExchangeIndex[j + 1];
    }
    ExchangeIndex[HalfTapCount + 1] = KN;
    if (NITER++ < ITRMAX)
        goto TOP_LINE;

    return (NITER);

}

//-----------------------------------------------------------------------
// Function to calculate the lagrange interpolation coefficients for use in the function gee.
double LeGrangeInterp2(int K, int N, int M) // D
        {
    int j, k;
    double Dee, Q;
    Dee = 1.0;
    Q = CosOfGrid[K];
    for (k = 1; k <= M; k++)
        for (j = k; j <= N; j += M) {
            if (j != K)
                Dee = 2.0 * Dee * (Q - CosOfGrid[j]);
        }
    if (fabs(Dee) < MIN_TEST_VAL) {
        if (Dee < 0.0)
            Dee = -MIN_TEST_VAL;
        else
            Dee = MIN_TEST_VAL;
    }
    return (1.0 / Dee);
}

//-----------------------------------------------------------------------
// Function to evaluate the frequency response using the Lagrange interpolation
// formula in the barycentric form.
double GEE2(int K, int N) {
    int j;
    double P, C, Dee, XF;
    P = 0.0;
    XF = Grid[K];
    XF = cos(M_2PI * XF);
    Dee = 0.0;
    for (j = 1; j <= N; j++) {
        C = XF - CosOfGrid[j];
        if (fabs(C) < MIN_TEST_VAL) {
            if (C < 0.0)
                C = -MIN_TEST_VAL;
            else
                C = MIN_TEST_VAL;
        }
        C = LeGrangeD[j] / C;
        Dee = Dee + C;
        P = P + C * DesPlus[j];
    }
    if (fabs(Dee) < MIN_TEST_VAL) {
        if (Dee < 0.0)
            Dee = -MIN_TEST_VAL;
        else
            Dee = MIN_TEST_VAL;
    }
    return (P / Dee);
}

//-----------------------------------------------------------------------

bool ErrTest(int k, int Nut, double Comp, double *Err) {
    *Err = GEE2(k, HalfTapCount + 1);
    *Err = (*Err - DesiredMag[k]) * Weight[k];
    if ((double) Nut * *Err - Comp <= 0.0)
        return (true);
    else
        return (false);
}

//-----------------------------------------------------------------------

// Calculation of the coefficients of the best approximation using the inverse discrete fourier transform.
void CalcCoefficients(void) {
    int j, k, n;
    double GTempVar, OneOverNumTaps;
    double Omega, TempVar, FreqN, TempX, GridCos;
    double GeeArray[PARKS_SMALL];

    GTempVar = Grid[1];
    CosOfGrid[HalfTapCount + 2] = -2.0;
    OneOverNumTaps = 1.0 / (double) (2 * HalfTapCount - 1);
    k = 1;

    for (j = 1; j <= HalfTapCount; j++) {
        FreqN = (double) (j - 1) * OneOverNumTaps;
        TempX = cos(M_2PI * FreqN);

        GridCos = CosOfGrid[k];
        if (TempX <= GridCos) {
            while (TempX <= GridCos && (GridCos - TempX) >= MIN_TEST_VAL) // MIN_TEST_VAL = 1.0E-6
            {
                k++;
                ;
                GridCos = CosOfGrid[k];
            }
        }
        if (TempX <= GridCos || (TempX - GridCos) < MIN_TEST_VAL) {
            GeeArray[j] = DesPlus[k]; // Desired Response
        } else {
            Grid[1] = FreqN;
            GeeArray[j] = GEE2(1, HalfTapCount + 1);
        }
        if (k > 1)
            k--;
    }

    Grid[1] = GTempVar;
    for (j = 1; j <= HalfTapCount; j++) {
        TempVar = 0.0;
        Omega = (double) (j - 1) * M_2PI * OneOverNumTaps;
        for (n = 1; n <= HalfTapCount - 1; n++) {
            TempVar += GeeArray[n + 1] * cos(Omega * (double) n);
        }
        TempVar = 2.0 * TempVar + GeeArray[1];
        Alpha[j] = TempVar;
    }

    Alpha[1] = Alpha[1] * OneOverNumTaps;
    for (j = 2; j <= HalfTapCount; j++) {
        Alpha[j] = 2.0 * Alpha[j] * OneOverNumTaps;
    }

}

//-----------------------------------------------------------------------

} // namespace

