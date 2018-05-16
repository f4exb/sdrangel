/*
 By Daniel Klostermann
 Iowa Hills Software, LLC  IowaHills.com
 If you find a problem, please leave a note at:
 http://www.iowahills.com/feedbackcomments.html
 May 1, 2016

 ShowMessage is a C++ Builder function, and it usage has been commented out.
 If you are using C++ Builder, include vcl.h for ShowMessage.
 Otherwise replace ShowMessage with something appropriate for your compiler.

 This code generates 2nd order S plane coefficients for the following low pass filter prototypes.
 Butterworth, Chebyshev, Bessel, Gauss, Adjustable Gauss, Inverse Chebyshev, Papoulis, and Elliptic.
 This code does 3 things.
 1. Gets the filter's roots from the code in LowPassRoots.cpp
 2. Uses the left hand plane roots to form 2nd order polynomials.
 3. Frequency scales the polynomials so the 3 dB corner frequency is at omege = 1 rad/sec.

 Note that we range check the Filt struct variables here, but we don't report
 back any changes we make.
 */

#include <complex>
#include "LowPassPrototypes.h"
#include "PFiftyOneRevE.h"
#include "LowPassRoots.h"

namespace kitiirfir
{

//---------------------------------------------------------------------------

// TLowPassParams defines the low pass prototype (NumPoles, Ripple, etc.).
// We return SPlaneCoeff filled with the 2nd order S plane coefficients.
TSPlaneCoeff CalcLowPassProtoCoeff(TLowPassParams Filt) {
    int j, DenomCount = 0, NumerCount, NumRoots, ZeroCount;
    std::complex<double> Poles[ARRAY_DIM], Zeros[ARRAY_DIM];
    TSPlaneCoeff Coeff;  // The return value.

    // Init the S Plane Coeff. H(s) = (N2*s^2 + N1*s + N0) / (D2*s^2 + D1*s + D0)
    for (j = 0; j < ARRAY_DIM; j++) {
        Coeff.N2[j] = 0.0;
        Coeff.N1[j] = 0.0;
        Coeff.N0[j] = 1.0;
        Coeff.D2[j] = 0.0;
        Coeff.D1[j] = 0.0;
        Coeff.D0[j] = 1.0;
    }
    Coeff.NumSections = 0;

    // We need to range check the various argument values here.
    // These are the practical limits the max number of poles.
    if (Filt.NumPoles < 1)
        Filt.NumPoles = 1;
    if (Filt.NumPoles > MAX_POLE_COUNT)
        Filt.NumPoles = MAX_POLE_COUNT;
    if (Filt.ProtoType == ELLIPTIC || Filt.ProtoType == INVERSE_CHEBY) {
        if (Filt.NumPoles > 15)
            Filt.NumPoles = 15;
    }
    if (Filt.ProtoType == GAUSSIAN || Filt.ProtoType == BESSEL) {
        if (Filt.NumPoles > 12)
            Filt.NumPoles = 12;
    }

    // Gamma is used by the Adjustable Gauss.
    if (Filt.Gamma < -1.0)
        Filt.Gamma = -1.0; // -1 gives ~ Gauss response
    if (Filt.Gamma > 1.0)
        Filt.Gamma = 1.0;   // +1 gives ~ Butterworth response.

    // Ripple is used by the Chebyshev and Elliptic
    if (Filt.Ripple < 0.0001)
        Filt.Ripple = 0.0001;
    if (Filt.Ripple > 1.0)
        Filt.Ripple = 1.0;

    // With the Chebyshev we need to use less ripple for large pole counts to keep the poles out of the RHP.
    if (Filt.ProtoType == CHEBYSHEV && Filt.NumPoles > 15) {
        double MaxRipple = 1.0;
        if (Filt.NumPoles == 16)
            MaxRipple = 0.5;
        if (Filt.NumPoles == 17)
            MaxRipple = 0.4;
        if (Filt.NumPoles == 18)
            MaxRipple = 0.25;
        if (Filt.NumPoles == 19)
            MaxRipple = 0.125;
        if (Filt.NumPoles >= 20)
            MaxRipple = 0.10;
        if (Filt.Ripple > MaxRipple)
            Filt.Ripple = MaxRipple;
    }

    // StopBanddB is used by the Inverse Chebyshev and the Elliptic
    // It is given in positive dB values.
    if (Filt.StopBanddB < 20.0)
        Filt.StopBanddB = 20.0;
    if (Filt.StopBanddB > 120.0)
        Filt.StopBanddB = 120.0;

    // There isn't such a thing as a 1 pole Chebyshev, or 1 pole Bessel, etc.
    // A one pole filter is simply 1/(s+1).
    NumerCount = 0; // init
    if (Filt.NumPoles == 1) {
        Coeff.D1[0] = 1.0;
        DenomCount = 1; // DenomCount is the number of denominator factors (1st or 2nd order).
    } else if (Filt.ProtoType == BUTTERWORTH) {
        NumRoots = ButterworthPoly(Filt.NumPoles, Poles);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        // A Butterworth doesn't require frequncy scaling with SetCornerFreq().
    }

    else if (Filt.ProtoType == ADJUSTABLE) // Adjustable Gauss
            {
        NumRoots = AdjustablePoly(Filt.NumPoles, Poles, Filt.Gamma);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        SetCornerFreq(DenomCount, Coeff.D2, Coeff.D1, Coeff.D0, Coeff.N2,
                Coeff.N1, Coeff.N0);
    }

    else if (Filt.ProtoType == CHEBYSHEV) {
        NumRoots = ChebyshevPoly(Filt.NumPoles, Filt.Ripple, Poles);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        SetCornerFreq(DenomCount, Coeff.D2, Coeff.D1, Coeff.D0, Coeff.N2,
                Coeff.N1, Coeff.N0);
    }

    else if (Filt.ProtoType == INVERSE_CHEBY) {
        NumRoots = InvChebyPoly(Filt.NumPoles, Filt.StopBanddB, Poles, Zeros,
                &ZeroCount);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        NumerCount = GetFilterCoeff(ZeroCount, Zeros, Coeff.N2, Coeff.N1,
                Coeff.N0);
        SetCornerFreq(DenomCount, Coeff.D2, Coeff.D1, Coeff.D0, Coeff.N2,
                Coeff.N1, Coeff.N0);
    }

    else if (Filt.ProtoType == ELLIPTIC) {
        NumRoots = EllipticPoly(Filt.NumPoles, Filt.Ripple, Filt.StopBanddB,
                Poles, Zeros, &ZeroCount);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        NumerCount = GetFilterCoeff(ZeroCount, Zeros, Coeff.N2, Coeff.N1,
                Coeff.N0);
        SetCornerFreq(DenomCount, Coeff.D2, Coeff.D1, Coeff.D0, Coeff.N2,
                Coeff.N1, Coeff.N0);
    }

    // Papoulis works OK, but it doesn't accomplish anything the Chebyshev can't.
    else if (Filt.ProtoType == PAPOULIS) {
        NumRoots = PapoulisPoly(Filt.NumPoles, Poles);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        SetCornerFreq(DenomCount, Coeff.D2, Coeff.D1, Coeff.D0, Coeff.N2,
                Coeff.N1, Coeff.N0);
    }

    else if (Filt.ProtoType == BESSEL) {
        NumRoots = BesselPoly(Filt.NumPoles, Poles);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        SetCornerFreq(DenomCount, Coeff.D2, Coeff.D1, Coeff.D0, Coeff.N2,
                Coeff.N1, Coeff.N0);
    }

    else if (Filt.ProtoType == GAUSSIAN) {
        NumRoots = GaussianPoly(Filt.NumPoles, Poles);
        DenomCount = GetFilterCoeff(NumRoots, Poles, Coeff.D2, Coeff.D1,
                Coeff.D0);
        SetCornerFreq(DenomCount, Coeff.D2, Coeff.D1, Coeff.D0, Coeff.N2,
                Coeff.N1, Coeff.N0);
    }

    Coeff.NumSections = DenomCount;

    // If we have an odd pole count, there will be 1 less zero than poles, so we need to shift the
    // zeros down in the arrays so the 1st zero (which is zero) and aligns with the real pole.
    if (NumerCount != 0 && Filt.NumPoles % 2 == 1) {
        for (j = NumerCount; j >= 0; j--) {
            Coeff.N2[j + 1] = Coeff.N2[j];  // Coeff.N1's are always zero
            Coeff.N0[j + 1] = Coeff.N0[j];
        }
        Coeff.N2[0] = 0.0;   // Set the 1st zero to zero for odd pole counts.
        Coeff.N0[0] = 1.0;
    }

    return (Coeff);

}

//---------------------------------------------------------------------------

// This sets the polynomial's the 3 dB corner to 1 rad/sec. This isn' required for a Butterworth,
// but the rest of the polynomials need correction. Esp the Adj Gauss, Inv Cheby and Bessel.
// Poly Count is the number of 2nd order sections. D and N are the Denom and Num coefficients.
// H(s) = (N2*s^2 + N1*s + N0) / (D2*s^2 + D1*s + D0)
void SetCornerFreq(int PolyCount, double *D2, double *D1, double *D0,
        double *N2, double *N1, double *N0) {
    int j, n;
    double Omega, FreqScalar, Zeta, Gain;
    std::complex<double> s, H;

    Gain = 1.0;
    for (j = 0; j < PolyCount; j++)
        Gain *= D0[j] / N0[j];

    // Evaluate H(s) by increasing Omega until |H(s)| < -3 dB
    for (j = 1; j < 6000; j++) {
        Omega = (double) j / 512.0; // The step size for Omega is 1/512 radians.
        s = std::complex<double>(0.0, Omega);

        H = std::complex<double>(1.0, 0.0);
        for (n = 0; n < PolyCount; n++) {
            H = H * (N2[n] * s * s + N1[n] * s + N0[n])
                    / (D2[n] * s * s + D1[n] * s + D0[n]);
        }
        H *= Gain;
        if (std::abs(H) < 0.7071)
            break;  // -3 dB
    }

    FreqScalar = 1.0 / Omega;

    // Freq scale the denominator. We hold the damping factor Zeta constant.
    for (j = 0; j < PolyCount; j++) {
        Omega = sqrt(D0[j]);
        if (Omega == 0.0)
            continue;   // should never happen
        Zeta = D1[j] / Omega / 2.0;
        if (D2[j] != 0.0)           // 2nd degree poly
                {
            D0[j] = Omega * Omega * FreqScalar * FreqScalar;
            D1[j] = 2.0 * Zeta * Omega * FreqScalar;
        } else  // 1st degree poly
        {
            D0[j] *= FreqScalar;
        }
    }

    // Scale the numerator.   H(s) = (N2*s^2 + N1*s + N0) / (D2*s^2 + D1*s + D0)
    // N1 is always zero. N2 is either 1 or 0. If N2 = 0, then N0 = 1 and there isn't a zero to scale.
    // For all pole filters (Butter, Cheby, etc) N2 = 0 and N0 = 1.
    for (j = 0; j < PolyCount; j++) {
        if (N2[j] == 0.0)
            continue;
        N0[j] *= FreqScalar * FreqScalar;
    }

}

//---------------------------------------------------------------------------

// Some of the Polys generate both left hand and right hand plane roots.
// We use this function to get the left hand plane poles and imag axis zeros to
// create the 2nd order polynomials with coefficients A2, A1, A0.
// We return the Polynomial count.

// We first sort the roots according the the real part (a zeta sort). Then all the left
// hand plane roots are grouped and in the correct order for IIR and Opamp filters.
// We then check for duplicate roots, and set an inconsequential real or imag part to zero.
// Then the 2nd order coefficients are calculated.
int GetFilterCoeff(int RootCount, std::complex<double> *Roots, double *A2, double *A1,
        double *A0) {
    int PolyCount, j, k;

    SortRootsByZeta(Roots, RootCount, stMin); // stMin places the most negative real part 1st.

    // Check for duplicate roots. The Inv Cheby generates duplcate imag roots, and the
    // Elliptic generates duplicate real roots. We set duplicates to a RHP value.
    for (j = 0; j < RootCount - 1; j++) {
        for (k = j + 1; k < RootCount; k++) {
            if (fabs(Roots[j].real() - Roots[k].real()) < 1.0E-3
                    && fabs(Roots[j].imag() - Roots[k].imag()) < 1.0E-3) {
                Roots[k] = std::complex<double>((double) k, 0.0); // RHP roots are ignored below, Use k is to prevent duplicate checks for matches.
            }
        }
    }

    // This forms the 2nd order coefficients from the root value.
    // We ignore roots in the Right Hand Plane.
    PolyCount = 0;
    for (j = 0; j < RootCount; j++) {
        if (Roots[j].real() > 0.0)
            continue; // Right Hand Plane
        if (Roots[j].real() == 0.0 && Roots[j].imag() == 0.0)
            continue; // At the origin.  This should never happen.

        if (Roots[j].real() == 0.0) // Imag Root (A poly zero)
                {
            A2[PolyCount] = 1.0;
            A1[PolyCount] = 0.0;
            A0[PolyCount] = Roots[j].imag() * Roots[j].imag();
            j++;
            PolyCount++;
        } else if (Roots[j].imag() == 0.0) // Real Pole
                {
            A2[PolyCount] = 0.0;
            A1[PolyCount] = 1.0;
            A0[PolyCount] = -Roots[j].real();
            PolyCount++;
        } else // Complex Pole
        {
            A2[PolyCount] = 1.0;
            A1[PolyCount] = -2.0 * Roots[j].real();
            A0[PolyCount] = Roots[j].real() * Roots[j].real()
                    + Roots[j].imag() * Roots[j].imag();
            j++;
            PolyCount++;
        }
    }

    return (PolyCount);

}

//---------------------------------------------------------------------------

// This rebuilds an Nth order poly from its 2nd order constituents.
// PolyCount is the number of 2nd order polys. e.g. NumPoles = 7  PolyCount = 4
// PolyCoeff gets filled such that PolyCoeff[5]*s^5 + PolyCoeff[4]*s^4 + PolyCoeff[3]*s^3 + ..
// The poly order is returned.
int RebuildPoly(int PolyCount, double *PolyCoeff, double *A2, double *A1,
        double *A0) {
    int j, k, n;
    double Sum[P51_ARRAY_SIZE];
    for (j = 0; j <= 2 * PolyCount; j++)
        PolyCoeff[j] = 0.0;
    for (j = 0; j < P51_ARRAY_SIZE; j++)
        Sum[j] = 0.0;

    PolyCoeff[2] = A2[0];
    PolyCoeff[1] = A1[0];
    PolyCoeff[0] = A0[0];

    for (j = 1; j < PolyCount; j++) {
        for (n = 0; n <= 2 * j; n++) {
            Sum[n + 2] += PolyCoeff[n] * A2[j];
            Sum[n + 1] += PolyCoeff[n] * A1[j];
            Sum[n + 0] += PolyCoeff[n] * A0[j];
        }
        for (k = 0; k <= 2 * j + 2; k++)
            PolyCoeff[k] = Sum[k];
        for (k = 0; k < P51_ARRAY_SIZE; k++)
            Sum[k] = 0.0;
    }

    // Want to return the poly order. This will be 2 * PolyCount if there aren't any 1st order polys.
    // 1st order Polys create leading zeros. N 1st order polys Gives N leading zeros.
    for (j = 2 * PolyCount; j >= 0; j--)
        if (PolyCoeff[j] != 0.0)
            break;
    return (j);
}

//---------------------------------------------------------------------------
// This sorts on the real part if the real part of the 1st root != 0 (a Zeta sort)
// else we sort on the imag part. If SortType == stMin for both the poles and zeros, then
// the poles and zeros will be properly matched.
// This also sets an inconsequential real or imag part to zero.
// A matched pair of z plane real roots, such as +/- 1, don't come out together.
// Used above in GetFilterCoeff and the FIR zero plot.
void SortRootsByZeta(std::complex<double> *Roots, int Count, TOurSortTypes SortType) {
    if (Count >= P51_MAXDEGREE) {
        //ShowMessage("Count > P51_MAXDEGREE in TPolyForm::SortRootsByZeta()");
        return;
    }

    int j, k, RootJ[P51_ARRAY_SIZE];
    double SortValue[P51_ARRAY_SIZE];
    std::complex<double> TempRoots[P51_ARRAY_SIZE];

    // Set an inconsequential real or imag part to zero.
    for (j = 0; j < Count; j++) {
        if (fabs(Roots[j].real()) * 1.0E3 < fabs(Roots[j].imag()))
            Roots[j].real(0.0);
        if (fabs(Roots[j].imag()) * 1.0E3 < fabs(Roots[j].real()))
            Roots[j].imag(0.0);
    }

    // Sort the roots.
    for (j = 0; j < Count; j++)
        RootJ[j] = j;  // Needed for HeapIndexSort
    if (Roots[0].real() != 0.0) // Cplx roots
            {
        for (j = 0; j < Count; j++)
            SortValue[j] = Roots[j].real();
    } else  // Imag roots, so we sort on imag part.
    {
        for (j = 0; j < Count; j++)
            SortValue[j] = fabs(Roots[j].imag());
    }
    HeapIndexSort(SortValue, RootJ, Count, SortType); // stMin gives the most negative root on top

    for (j = 0; j < Count; j++) {
        k = RootJ[j];   // RootJ is the sort index
        TempRoots[j] = Roots[k];
    }
    for (j = 0; j < Count; j++) {
        Roots[j] = TempRoots[j];
    }

}
//---------------------------------------------------------------------------

// Remember to set the Index array to 0, 1, 2, 3, ... N-1
bool HeapIndexSort(double *Data, int *Index, int N, TOurSortTypes SortType) {
    int i, j, k, m, IndexTemp;
    long long FailSafe, NSquared; // need this for big sorts

    NSquared = (long long) N * (long long) N;
    m = N / 2;
    k = N - 1;
    for (FailSafe = 0; FailSafe < NSquared; FailSafe++) // typical FailSafe value on return is N*log2(N)
            {
        if (m > 0)
            IndexTemp = Index[--m];
        else {
            IndexTemp = Index[k];
            Index[k] = Index[0];
            if (--k == 0) {
                Index[0] = IndexTemp;
                return (true);
            }
        }

        i = m + 1;
        j = 2 * i;

        if (SortType == stMax)
            while (j < k + 2) {
                FailSafe++;
                if (j <= k && Data[Index[j - 1]] > Data[Index[j]])
                    j++;
                if (Data[IndexTemp] > Data[Index[j - 1]]) {
                    Index[i - 1] = Index[j - 1];
                    i = j;
                    j += i;
                } else
                    break;
            }

        else
            // SortType == stMin
            while (j < k + 2) {
                FailSafe++;
                if (j <= k && Data[Index[j - 1]] < Data[Index[j]])
                    j++;
                if (Data[IndexTemp] < Data[Index[j - 1]]) {
                    Index[i - 1] = Index[j - 1];
                    i = j;
                    j += i;
                } else
                    break;
            }

        Index[i - 1] = IndexTemp;
    }
    return (false);
}

//----------------------------------------------------------------------------------

} // namespace

