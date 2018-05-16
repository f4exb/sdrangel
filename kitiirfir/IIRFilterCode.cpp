/*
 By Daniel Klostermann
 Iowa Hills Software, LLC  IowaHills.com
 If you find a problem, please leave a note at:
 http://www.iowahills.com/feedbackcomments.html
 May 1, 2016

 ShowMessage is a C++ Builder function, and it usage has been commented out.
 If you are using C++ Builder, include vcl.h for ShowMessage.
 Otherwise replace ShowMessage with something appropriate for your compiler.

 See the FilterKitMain.cpp file for an example on how to use this code.
 */

#include "IIRFilterCode.h"
#include "PFiftyOneRevE.h"
#include "LowPassPrototypes.h"
#include <complex>
#include <math.h>

namespace kitiirfir
{

//---------------------------------------------------------------------------
/*
 This calculates the coefficients for IIR filters from a set of 2nd order s plane coefficients
 which are obtained by calling CalcLowPassProtoCoeff() in LowPassPrototypes.cpp.
 The s plane filters are frequency scaled so their 3 dB frequency is at s = omega = 1 rad/sec.
 The poles and zeros are also ordered in a manner appropriate for IIR filters.
 For a derivation of the formulas used here, see the IIREquationDerivations.pdf
 This shows how the various poly coefficients are defined.
 H(s) = ( Ds^2 + Es + F ) / ( As^2 + Bs + C )
 H(z) = ( b2z^2 + b1z + b0 ) / ( a2z^2 + a1z + a0 )
 */
TIIRCoeff CalcIIRFilterCoeff(TIIRFilterParams IIRFilt) {
    int j, k;
    double Scalar, SectionGain, Coeff[5];
    double A, B, C, D, E, F, T, Q, Arg;
    double a2[ARRAY_DIM], a1[ARRAY_DIM], a0[ARRAY_DIM];
    double b2[ARRAY_DIM], b1[ARRAY_DIM], b0[ARRAY_DIM];
    std::complex<double> Roots[5];

    TIIRCoeff IIR;                // Gets returned by this function.
    TLowPassParams LowPassFilt; // Passed to the CalcLowPassProtoCoeff() function.
    TSPlaneCoeff SPlaneCoeff; // Filled by the CalcLowPassProtoCoeff() function.

    // We can set the TLowPassParams variables directly from the TIIRFilterParams variables.
    LowPassFilt.ProtoType = IIRFilt.ProtoType;
    LowPassFilt.NumPoles = IIRFilt.NumPoles;
    LowPassFilt.Ripple = IIRFilt.Ripple;
    LowPassFilt.Gamma = IIRFilt.Gamma;
    LowPassFilt.StopBanddB = IIRFilt.StopBanddB;

    // Get the low pass prototype 2nd order s plane coefficients.
    SPlaneCoeff = CalcLowPassProtoCoeff(LowPassFilt);

    // Init the IIR structure.
    for (j = 0; j < ARRAY_DIM; j++) {
        IIR.a0[j] = 0.0;
        IIR.b0[j] = 0.0;
        IIR.a1[j] = 0.0;
        IIR.b1[j] = 0.0;
        IIR.a2[j] = 0.0;
        IIR.b2[j] = 0.0;
        IIR.a3[j] = 0.0;
        IIR.b3[j] = 0.0;
        IIR.a4[j] = 0.0;
        IIR.b4[j] = 0.0;
    }

    // Set the number of IIR filter sections we will be generating.
    IIR.NumSections = (IIRFilt.NumPoles + 1) / 2;
    if (IIRFilt.IIRPassType == iirBPF || IIRFilt.IIRPassType == iirNOTCH)
        IIR.NumSections = IIRFilt.NumPoles;

    // For All Pass filters, the numerator is set to the denominator values as shown here.
    // If the prototype was an Inv Cheby or Elliptic, the S plane numerator is discarded.
    // Use the Gauss as the prototype for the best all pass results (most linear phase).
    // The all pass H(s) = ( As^2 - Bs + C ) / ( As^2 + Bs + C )
    if (IIRFilt.IIRPassType == iirALLPASS) {
        for (j = 0; j < SPlaneCoeff.NumSections; j++) {
            SPlaneCoeff.N2[j] = SPlaneCoeff.D2[j];
            SPlaneCoeff.N1[j] = -SPlaneCoeff.D1[j];
            SPlaneCoeff.N0[j] = SPlaneCoeff.D0[j];
        }
    }

    // T sets the IIR filter's corner frequency, or center freqency.
    // The Bilinear transform is defined as:  s = 2/T * tan(Omega/2) = 2/T * (1 - z)/(1 + z)
    T = 2.0 * tan(IIRFilt.OmegaC * M_PI_2);
    Q = 1.0 + IIRFilt.OmegaC;      // Q is used for band pass and notch filters.
    if (Q > 1.95)
        Q = 1.95;
    Q = 0.8 * tan(Q * M_PI_4);            // This is a correction factor for Q.
    Q = IIRFilt.OmegaC / IIRFilt.BW / Q;  // This is the corrected Q.

    // Calc the IIR coefficients.
    // SPlaneCoeff.NumSections is the number of 1st and 2nd order s plane factors.
    k = 0;
    for (j = 0; j < SPlaneCoeff.NumSections; j++) {
        A = SPlaneCoeff.D2[j]; // We use A - F to make the code easier to read.
        B = SPlaneCoeff.D1[j];
        C = SPlaneCoeff.D0[j];
        D = SPlaneCoeff.N2[j];
        E = SPlaneCoeff.N1[j]; // N1 is always zero, except for the all pass. Consequently, the equations below can be simplified a bit by removing E.
        F = SPlaneCoeff.N0[j];

        // b's are the numerator  a's are the denominator
        if (IIRFilt.IIRPassType == iirLPF || IIRFilt.IIRPassType == iirALLPASS) // Low Pass and All Pass
                {
            if (A == 0.0 && D == 0.0) // 1 pole case
                    {
                Arg = (2.0 * B + C * T);
                IIR.a2[j] = 0.0;
                IIR.a1[j] = (-2.0 * B + C * T) / Arg;
                IIR.a0[j] = 1.0;

                IIR.b2[j] = 0.0;
                IIR.b1[j] = (-2.0 * E + F * T) / Arg * C / F;
                IIR.b0[j] = (2.0 * E + F * T) / Arg * C / F;
            } else // 2 poles
            {
                Arg = (4.0 * A + 2.0 * B * T + C * T * T);
                IIR.a2[j] = (4.0 * A - 2.0 * B * T + C * T * T) / Arg;
                IIR.a1[j] = (2.0 * C * T * T - 8.0 * A) / Arg;
                IIR.a0[j] = 1.0;

                // With all pole filters, our LPF numerator is (z+1)^2, so all our Z Plane zeros are at -1
                IIR.b2[j] = (4.0 * D - 2.0 * E * T + F * T * T) / Arg * C / F;
                IIR.b1[j] = (2.0 * F * T * T - 8.0 * D) / Arg * C / F;
                IIR.b0[j] = (4 * D + F * T * T + 2.0 * E * T) / Arg * C / F;
            }
        }

        if (IIRFilt.IIRPassType == iirHPF) // High Pass
                {
            if (A == 0.0 && D == 0.0) // 1 pole
                    {
                Arg = 2.0 * C + B * T;
                IIR.a2[j] = 0.0;
                IIR.a1[j] = (B * T - 2.0 * C) / Arg;
                IIR.a0[j] = 1.0;

                IIR.b2[j] = 0.0;
                IIR.b1[j] = (E * T - 2.0 * F) / Arg * C / F;
                IIR.b0[j] = (E * T + 2.0 * F) / Arg * C / F;
            } else  // 2 poles
            {
                Arg = A * T * T + 4.0 * C + 2.0 * B * T;
                IIR.a2[j] = (A * T * T + 4.0 * C - 2.0 * B * T) / Arg;
                IIR.a1[j] = (2.0 * A * T * T - 8.0 * C) / Arg;
                IIR.a0[j] = 1.0;

                // With all pole filters, our HPF numerator is (z-1)^2, so all our Z Plane zeros are at 1
                IIR.b2[j] = (D * T * T - 2.0 * E * T + 4.0 * F) / Arg * C / F;
                IIR.b1[j] = (2.0 * D * T * T - 8.0 * F) / Arg * C / F;
                IIR.b0[j] = (D * T * T + 4.0 * F + 2.0 * E * T) / Arg * C / F;
            }
        }

        if (IIRFilt.IIRPassType == iirBPF) // Band Pass
                {
            if (A == 0.0 && D == 0.0) // 1 pole
                    {
                Arg = 4.0 * B * Q + 2.0 * C * T + B * Q * T * T;
                a2[k] = (B * Q * T * T + 4.0 * B * Q - 2.0 * C * T) / Arg;
                a1[k] = (2.0 * B * Q * T * T - 8.0 * B * Q) / Arg;
                a0[k] = 1.0;

                b2[k] = (E * Q * T * T + 4.0 * E * Q - 2.0 * F * T) / Arg * C
                        / F;
                b1[k] = (2.0 * E * Q * T * T - 8.0 * E * Q) / Arg * C / F;
                b0[k] = (4.0 * E * Q + 2.0 * F * T + E * Q * T * T) / Arg * C
                        / F;
                k++;
            } else //2 Poles
            {
                IIR.a4[j] = (16.0 * A * Q * Q + A * Q * Q * T * T * T * T
                        + 8.0 * A * Q * Q * T * T - 2.0 * B * Q * T * T * T
                        - 8.0 * B * Q * T + 4.0 * C * T * T) * F;
                IIR.a3[j] = (4.0 * T * T * T * T * A * Q * Q
                        - 4.0 * Q * T * T * T * B + 16.0 * Q * B * T
                        - 64.0 * A * Q * Q) * F;
                IIR.a2[j] = (96.0 * A * Q * Q - 16.0 * A * Q * Q * T * T
                        + 6.0 * A * Q * Q * T * T * T * T - 8.0 * C * T * T)
                        * F;
                IIR.a1[j] = (4.0 * T * T * T * T * A * Q * Q
                        + 4.0 * Q * T * T * T * B - 16.0 * Q * B * T
                        - 64.0 * A * Q * Q) * F;
                IIR.a0[j] = (16.0 * A * Q * Q + A * Q * Q * T * T * T * T
                        + 8.0 * A * Q * Q * T * T + 2.0 * B * Q * T * T * T
                        + 8.0 * B * Q * T + 4.0 * C * T * T) * F;

                // With all pole filters, our BPF numerator is (z-1)^2 * (z+1)^2 so the zeros come back as +/- 1 pairs
                IIR.b4[j] = (8.0 * D * Q * Q * T * T - 8.0 * E * Q * T
                        + 16.0 * D * Q * Q - 2.0 * E * Q * T * T * T
                        + D * Q * Q * T * T * T * T + 4.0 * F * T * T) * C;
                IIR.b3[j] = (16.0 * E * Q * T - 4.0 * E * Q * T * T * T
                        - 64.0 * D * Q * Q + 4.0 * D * Q * Q * T * T * T * T)
                        * C;
                IIR.b2[j] = (96.0 * D * Q * Q - 8.0 * F * T * T
                        + 6.0 * D * Q * Q * T * T * T * T
                        - 16.0 * D * Q * Q * T * T) * C;
                IIR.b1[j] = (4.0 * D * Q * Q * T * T * T * T - 64.0 * D * Q * Q
                        + 4.0 * E * Q * T * T * T - 16.0 * E * Q * T) * C;
                IIR.b0[j] = (16.0 * D * Q * Q + 8.0 * E * Q * T
                        + 8.0 * D * Q * Q * T * T + 2.0 * E * Q * T * T * T
                        + 4.0 * F * T * T + D * Q * Q * T * T * T * T) * C;

                // T = 2 makes these values approach 0.0 (~ 1.0E-12) The root solver needs 0.0 for numerical reasons.
                if (fabs(T - 2.0) < 0.0005) {
                    IIR.a3[j] = 0.0;
                    IIR.a1[j] = 0.0;
                    IIR.b3[j] = 0.0;
                    IIR.b1[j] = 0.0;
                }

                // We now have a 4th order poly in the form a4*s^4 + a3*s^3 + a2*s^2 + a2*s + a0
                // We find the roots of this so we can form two 2nd order polys.
                Coeff[0] = IIR.a4[j];
                Coeff[1] = IIR.a3[j];
                Coeff[2] = IIR.a2[j];
                Coeff[3] = IIR.a1[j];
                Coeff[4] = IIR.a0[j];
                FindRoots(4, Coeff, Roots);

                // In effect, the root finder scales the poly by 1/a4 so we have to apply this factor back into
                // the two 2nd order equations we are forming.
                Scalar = sqrt(fabs(IIR.a4[j]));

                // Form the two 2nd order polys from the roots.
                a2[k] = Scalar;
                a1[k] = -(Roots[0] + Roots[1]).real() * Scalar;
                a0[k] = (Roots[0] * Roots[1]).real() * Scalar;
                k++;
                a2[k] = Scalar;
                a1[k] = -(Roots[2] + Roots[3]).real() * Scalar;
                a0[k] = (Roots[2] * Roots[3]).real() * Scalar;
                k--;

                // Now do the same with the numerator.
                Coeff[0] = IIR.b4[j];
                Coeff[1] = IIR.b3[j];
                Coeff[2] = IIR.b2[j];
                Coeff[3] = IIR.b1[j];
                Coeff[4] = IIR.b0[j];

                if (IIRFilt.ProtoType == INVERSE_CHEBY
                        || IIRFilt.ProtoType == ELLIPTIC) {
                    FindRoots(4, Coeff, Roots);
                } else // With all pole filters (Butter, Cheb, etc), we know we have these 4 real roots. The root finder won't necessarily pair real roots the way we need, so rather than compute these, we simply set them.
                {
                    Roots[0] = std::complex<double>(-1.0, 0.0);
                    Roots[1] = std::complex<double>(1.0, 0.0);
                    Roots[2] = std::complex<double>(-1.0, 0.0);
                    Roots[3] = std::complex<double>(1.0, 0.0);
                }

                Scalar = sqrt(fabs(IIR.b4[j]));

                b2[k] = Scalar;
                if (IIRFilt.ProtoType == INVERSE_CHEBY
                        || IIRFilt.ProtoType == ELLIPTIC) {
                    b1[k] = -(Roots[0] + Roots[1]).real() * Scalar; // = 0.0
                } else  // else the prototype is an all pole filter
                {
                    b1[k] = 0.0; // b1 = 0 for all pole filters, but the addition above won't always equal zero exactly.
                }
                b0[k] = (Roots[0] * Roots[1]).real() * Scalar;

                k++;

                b2[k] = Scalar;
                if (IIRFilt.ProtoType == INVERSE_CHEBY
                        || IIRFilt.ProtoType == ELLIPTIC) {
                    b1[k] = -(Roots[2] + Roots[3]).real() * Scalar;
                } else // All pole
                {
                    b1[k] = 0.0;
                }
                b0[k] = (Roots[2] * Roots[3]).real() * Scalar;
                k++;
                // Go below to see where we store these 2nd order polys back into IIR
            }
        }

        if (IIRFilt.IIRPassType == iirNOTCH) // Notch
                {
            if (A == 0.0 && D == 0.0) // 1 pole
                    {
                Arg = 2.0 * B * T + C * Q * T * T + 4.0 * C * Q;
                a2[k] = (4.0 * C * Q - 2.0 * B * T + C * Q * T * T) / Arg;
                a1[k] = (2.0 * C * Q * T * T - 8.0 * C * Q) / Arg;
                a0[k] = 1.0;

                b2[k] = (4.0 * F * Q - 2.0 * E * T + F * Q * T * T) / Arg * C
                        / F;
                b1[k] = (2.0 * F * Q * T * T - 8.0 * F * Q) / Arg * C / F;
                b0[k] = (2.0 * E * T + F * Q * T * T + 4.0 * F * Q) / Arg * C
                        / F;
                k++;
            } else {
                IIR.a4[j] = (4.0 * A * T * T - 2.0 * B * T * T * T * Q
                        + 8.0 * C * Q * Q * T * T - 8.0 * B * T * Q
                        + C * Q * Q * T * T * T * T + 16.0 * C * Q * Q) * -F;
                IIR.a3[j] = (16.0 * B * T * Q + 4.0 * C * Q * Q * T * T * T * T
                        - 64.0 * C * Q * Q - 4.0 * B * T * T * T * Q) * -F;
                IIR.a2[j] = (96.0 * C * Q * Q - 8.0 * A * T * T
                        - 16.0 * C * Q * Q * T * T
                        + 6.0 * C * Q * Q * T * T * T * T) * -F;
                IIR.a1[j] = (4.0 * B * T * T * T * Q - 16.0 * B * T * Q
                        - 64.0 * C * Q * Q + 4.0 * C * Q * Q * T * T * T * T)
                        * -F;
                IIR.a0[j] = (4.0 * A * T * T + 2.0 * B * T * T * T * Q
                        + 8.0 * C * Q * Q * T * T + 8.0 * B * T * Q
                        + C * Q * Q * T * T * T * T + 16.0 * C * Q * Q) * -F;

                // Our Notch Numerator isn't simple. [ (4+T^2)*z^2 - 2*(4-T^2)*z + (4+T^2) ]^2
                IIR.b4[j] = (2.0 * E * T * T * T * Q - 4.0 * D * T * T
                        - 8.0 * F * Q * Q * T * T + 8.0 * E * T * Q
                        - 16.0 * F * Q * Q - F * Q * Q * T * T * T * T) * C;
                IIR.b3[j] = (64.0 * F * Q * Q + 4.0 * E * T * T * T * Q
                        - 16.0 * E * T * Q - 4.0 * F * Q * Q * T * T * T * T)
                        * C;
                IIR.b2[j] = (8.0 * D * T * T - 96.0 * F * Q * Q
                        + 16.0 * F * Q * Q * T * T
                        - 6.0 * F * Q * Q * T * T * T * T) * C;
                IIR.b1[j] = (16.0 * E * T * Q - 4.0 * E * T * T * T * Q
                        + 64.0 * F * Q * Q - 4.0 * F * Q * Q * T * T * T * T)
                        * C;
                IIR.b0[j] = (-4.0 * D * T * T - 2.0 * E * T * T * T * Q
                        - 8.0 * E * T * Q - 8.0 * F * Q * Q * T * T
                        - F * Q * Q * T * T * T * T - 16.0 * F * Q * Q) * C;

                // T = 2 (OmegaC = 0.5) makes these values approach 0.0 (~ 1.0E-12). The root solver wants 0.0 for numerical reasons.
                if (fabs(T - 2.0) < 0.0005) {
                    IIR.a3[j] = 0.0;
                    IIR.a1[j] = 0.0;
                    IIR.b3[j] = 0.0;
                    IIR.b1[j] = 0.0;
                }

                // We now have a 4th order poly in the form a4*s^4 + a3*s^3 + a2*s^2 + a2*s + a0
                // We find the roots of this so we can form two 2nd order polys.
                Coeff[0] = IIR.a4[j];
                Coeff[1] = IIR.a3[j];
                Coeff[2] = IIR.a2[j];
                Coeff[3] = IIR.a1[j];
                Coeff[4] = IIR.a0[j];

                // In effect, the root finder scales the poly by 1/a4 so we have to apply this factor back into
                // the two 2nd order equations we are forming.
                FindRoots(4, Coeff, Roots);
                Scalar = sqrt(fabs(IIR.a4[j]));
                a2[k] = Scalar;
                a1[k] = -(Roots[0] + Roots[1]).real() * Scalar;
                a0[k] = (Roots[0] * Roots[1]).real() * Scalar;

                k++;
                a2[k] = Scalar;
                a1[k] = -(Roots[2] + Roots[3]).real() * Scalar;
                a0[k] = (Roots[2] * Roots[3]).real() * Scalar;
                k--;

                // Now do the same with the numerator.
                Coeff[0] = IIR.b4[j];
                Coeff[1] = IIR.b3[j];
                Coeff[2] = IIR.b2[j];
                Coeff[3] = IIR.b1[j];
                Coeff[4] = IIR.b0[j];
                FindRoots(4, Coeff, Roots);

                Scalar = sqrt(fabs(IIR.b4[j]));
                b2[k] = Scalar;
                b1[k] = -(Roots[0] + Roots[1]).real() * Scalar;
                b0[k] = (Roots[0] * Roots[1]).real() * Scalar;

                k++;
                b2[k] = Scalar;
                b1[k] = -(Roots[2] + Roots[3]).real() * Scalar;
                b0[k] = (Roots[2] * Roots[3]).real() * Scalar;
                k++;
            }
        }
    }

    if (IIRFilt.IIRPassType == iirBPF || IIRFilt.IIRPassType == iirNOTCH) {
        // In the calcs above for the BPF and Notch, we didn't set a0=1, so we do it here.
        for (j = 0; j < IIR.NumSections; j++) {
            b2[j] /= a0[j];
            b1[j] /= a0[j];
            b0[j] /= a0[j];
            a2[j] /= a0[j];
            a1[j] /= a0[j];
            a0[j] = 1.0;
        }

        for (j = 0; j < IIR.NumSections; j++) {
            IIR.a0[j] = a0[j];
            IIR.a1[j] = a1[j];
            IIR.a2[j] = a2[j];
            IIR.b0[j] = b0[j];
            IIR.b1[j] = b1[j];
            IIR.b2[j] = b2[j];
        }
    }

    // Adjust the b's or a0 for the desired Gain.
    SectionGain = pow(10.0, IIRFilt.dBGain / 20.0);
    SectionGain = pow(SectionGain, 1.0 / (double) IIR.NumSections);
    for (j = 0; j < IIR.NumSections; j++) {
        IIR.b0[j] *= SectionGain;
        IIR.b1[j] *= SectionGain;
        IIR.b2[j] *= SectionGain;
        // This is an alternative to adjusting the b's
        // IIR.a0[j] = SectionGain;
    }

    return (IIR);
}

//---------------------------------------------------------------------------

// This code implements an IIR filter as a Form 1 Biquad.
// It uses 2 sets of shift registers, RegX on the input side and RegY on the output side.
// There are many ways to implement an IIR filter, some very good, and some extremely bad.
// For numerical reasons, a Form 1 Biquad implementation is among the best.
void FilterWithIIR(TIIRCoeff IIRCoeff, double *Signal, double *FilteredSignal,
        int NumSigPts) {
    double y;
    int j, k;

    for (j = 0; j < NumSigPts; j++) {
        k = 0;
        y = SectCalc(j, k, Signal[j], IIRCoeff);
        for (k = 1; k < IIRCoeff.NumSections; k++) {
            y = SectCalc(j, k, y, IIRCoeff);
        }
        FilteredSignal[j] = y;
    }

}
//---------------------------------------------------------------------------

// This gets used with the function above, FilterWithIIR()
// Note the use of MaxRegVal to avoid a math overflow condition.
double SectCalc(int j, int k, double x, TIIRCoeff IIRCoeff) {
    double y, CenterTap;
    static double RegX1[ARRAY_DIM], RegX2[ARRAY_DIM], RegY1[ARRAY_DIM],
            RegY2[ARRAY_DIM], MaxRegVal;
    static bool MessageShown = false;

    // Zero the regiisters on the 1st call or on an overflow condition. The overflow limit used
    // here is small for double variables, but a filter that reaches this threshold is broken.
    if ((j == 0 && k == 0) || MaxRegVal > OVERFLOW_LIMIT) {
        if (MaxRegVal > OVERFLOW_LIMIT && !MessageShown) {
            // ShowMessage("ERROR: Math Over Flow in IIR Section Calc. \nThe register values exceeded 1.0E20 \n");
            MessageShown = true; // So this message doesn't get shown thousands of times.
        }

        MaxRegVal = 1.0E-12;
        for (int i = 0; i < ARRAY_DIM; i++) {
            RegX1[i] = 0.0;
            RegX2[i] = 0.0;
            RegY1[i] = 0.0;
            RegY2[i] = 0.0;
        }
    }

    CenterTap = x * IIRCoeff.b0[k] + IIRCoeff.b1[k] * RegX1[k]
            + IIRCoeff.b2[k] * RegX2[k];
    y = IIRCoeff.a0[k] * CenterTap - IIRCoeff.a1[k] * RegY1[k]
            - IIRCoeff.a2[k] * RegY2[k];

    RegX2[k] = RegX1[k];
    RegX1[k] = x;
    RegY2[k] = RegY1[k];
    RegY1[k] = y;

    // MaxRegVal is used to prevent overflow.  Overflow seldom occurs, but will
    // if the filter has faulty coefficients. MaxRegVal is usually less than 100.0
    if (fabs(CenterTap) > MaxRegVal)
        MaxRegVal = fabs(CenterTap);
    if (fabs(y) > MaxRegVal)
        MaxRegVal = fabs(y);
    return (y);
}
//---------------------------------------------------------------------------

// This function calculates the frequency response of an IIR filter.
// Probably the easiest way to determine the frequency response of an IIR filter is to send
// an impulse through the filter and do an FFT on the output. This method does a DFT on
// the coefficients of each biquad section. The results from the cascaded sections are
// then multiplied together.

// This approach works better than an FFT when the filter is very narrow. To analyze highly selective
// filters with an FFT can require a very large number of points, which can be quite cumbersome.
// This approach allows you to set the range of frequencies to be analyzed by modifying the statement
// Arg = M_PI * (double)j / (double)NumPts; .
void IIRFreqResponse(TIIRCoeff IIR, int NumSections, double *RealHofZ,
        double *ImagHofZ, int NumPts) {
    int j, n;
    double Arg;
    std::complex<double> z1, z2, HofZ, Denom;
    for (j = 0; j < NumPts; j++) {
        Arg = M_PI * (double) j / (double) NumPts;
        z1 = std::complex<double>(cos(Arg), -sin(Arg));  // z = e^(j*omega)
        z2 = z1 * z1;                     // z squared

        HofZ = std::complex<double>(1.0, 0.0);
        for (n = 0; n < NumSections; n++) {
            HofZ *= IIR.a0[n]; // This can be in the denominator, but only if a0=1. a0 can be other than 1.0 to adjust the filter's gain. See the bottom of the CalcIIRFilterCoeff() function.
            HofZ *= IIR.b0[n] + IIR.b1[n] * z1 + IIR.b2[n] * z2;  // Numerator
            Denom = 1.0 + IIR.a1[n] * z1 + IIR.a2[n] * z2;        // Denominator
            if (std::abs(Denom) < 1.0E-12)
                Denom = 1.0E-12; // A pole on the unit circle would cause this to be zero, so this should never happen. It could happen however if the filter also has a zero at this frequency. Then H(z) needs to be determined by L'Hopitals rule at this freq.
            HofZ /= Denom;
        }
        RealHofZ[j] = HofZ.real();
        ImagHofZ[j] = HofZ.imag();
    }
}

//---------------------------------------------------------------------------

} // namespace
