/*
 By Daniel Klostermann
 Iowa Hills Software, LLC  IowaHills.com
 If you find a problem, please leave a note at:
 http://www.iowahills.com/feedbackcomments.html
 May 1, 2016


 This code calculates the roots for the following filter polynomials.
 Butterworth, Chebyshev, Bessel, Gauss, Adjustable Gauss, Inverse Chebyshev, Papoulis, and Elliptic.

 These filters are described in most filter design texts, except the Papoulis and Adjustable Gauss.

 The Adjustable Gauss is a transitional filter invented by Iowa Hills that can generate a response
 anywhere between a Gauss and a Butterworth. Use Gamma to control the response.
 Gamma = -1.0 nearly a Gauss
 Gamma = -0.7 nearly a Bessel
 Gamma = 1.0  nearly a Butterworth

 The Papoulis (Classic L) provides a response between the Butterworth the Chebyshev.
 It has a faster roll off than the Butterworth but without the Chebyshev ripple.
 It does however have some roll off in the pass band, which can be controlled by the RollOff parameter.

 The limits on the arguments for these functions (such as pole count or ripple) are not
 checked here. See the LowPassPrototypes.cpp for argument limits.
 */

#include <math.h>
#include <complex>
#include "LowPassRoots.h"
#include "PFiftyOneRevE.h"

namespace kitiirfir
{

//---------------------------------------------------------------------------
// This used by several of the functions below. It returns a double so
// we don't overflow and because we need a double for subsequent calculations.
double Factorial(int N) {
    int j;
    double Fact = 1.0;
    for (j = 1; j <= N; j++)
        Fact *= (double) j;
    return (Fact);
}

//---------------------------------------------------------------------------

// Some of the code below generates the coefficients in reverse order
// needed for the root finder. This reverses the poly.
void ReverseCoeff(double *P, int N) {
    int j;
    double Temp;
    for (j = 0; j <= N / 2; j++) {
        Temp = P[j];
        P[j] = P[N - j];
        P[N - j] = Temp;
    }

    for (j = N; j >= 1; j--) {
        if (P[0] != 0.0)
            P[j] /= P[0];
    }
    P[0] = 1.0;

}

//---------------------------------------------------------------------------

// We calculate the roots for a Butterwoth filter directly. (No root finder needed)
// We fill the array Roots[] and return the number of roots.
int ButterworthPoly(int NumPoles, std::complex<double> *Roots) {
    int j, n, N;
    double Theta;

    N = NumPoles;
    n = 0;
    for (j = 0; j < N / 2; j++) {
        Theta = M_PI * (double) (2 * j + N + 1) / (double) (2 * N);
        Roots[n++] = std::complex<double>(cos(Theta), sin(Theta));
        Roots[n++] = std::complex<double>(cos(Theta), -sin(Theta));
    }
    if (N % 2 == 1)
        Roots[n++] = std::complex<double>(-1.0, 0.0); // The real root for odd pole counts.
    return (N);
}

//---------------------------------------------------------------------------

// This calculates the roots for a Chebyshev filter directly. (No root finder needed)
int ChebyshevPoly(int NumPoles, double Ripple, std::complex<double> *Roots) {
    int j, n, N;
    double Sigma, Omega;
    double Arg, Theta, Epsilon;

    N = NumPoles;
    Epsilon = pow(10.0, Ripple / 10.0) - 1.0;
    Epsilon = sqrt(Epsilon);
    if (Epsilon < 0.00001)
        Epsilon = 0.00001;
    if (Epsilon > 0.996)
        Epsilon = 0.996;
    Epsilon = 1.0 / Epsilon;
    Arg = log(Epsilon + sqrt(Epsilon * Epsilon + 1.0)) / (double) N; // = asinh(Epsilon) / (double)N;
    n = 0;
    for (j = 0; j < N / 2; j++) {
        Theta = (2 * j + 1) * M_PI_2 / (double) N;
        Sigma = -sinh(Arg) * sin(Theta);
        Omega = cosh(Arg) * cos(Theta);
        Roots[n++] = std::complex<double>(Sigma, Omega);
        Roots[n++] = std::complex<double>(Sigma, -Omega);
    }
    if (N % 2 == 1)
        Roots[n++] = std::complex<double>(-sinh(Arg), 0.0); // The real root for odd pole counts.
    return (N);
}

//---------------------------------------------------------------------------

// The Gaussian Poly is simply 1 - s^2 + s^4 /2! - s^6 / 3! .... seeHumpherys p. 414.
int GaussianPoly(int NumPoles, std::complex<double> *Roots) {
    int j, N, RootsCount;
    double GaussCoeff[P51_ARRAY_SIZE];

    N = NumPoles;
    GaussCoeff[0] = 1.0;
    GaussCoeff[1] = 0.0;
    for (j = 2; j <= 2 * N; j += 2) {
        GaussCoeff[j] = 1.0 / Factorial(j / 2);
        GaussCoeff[j + 1] = 0.0;
        if ((j / 2) % 2 == 1)
            GaussCoeff[j] *= -1.0;
    }

    // The coefficients are generated in reverse order needed for P51.
    ReverseCoeff(GaussCoeff, N * 2);
    RootsCount = FindRoots(N * 2, GaussCoeff, Roots);
    return (RootsCount);
}

//---------------------------------------------------------------------------

// This function starts with the Gauss and modifies the coefficients.
// The Gaussian Poly is simply 1 - s^2 + s^4 /2! - s^6 / 3! .... seeHumpherys p. 414.
int AdjustablePoly(int NumPoles, std::complex<double> *Roots, double Gamma) {
    int j, N, RootsCount;
    double GaussCoeff[P51_ARRAY_SIZE];

    N = NumPoles;
    if (Gamma > 0.0)
        Gamma *= 2.0; // Gamma < 0 is the orig Gauss and Bessel responses. Gamma > 0 has an asymptotic response, so we double it, which also makes the user interface a bit nicer. i.e. -1 <= Gamma <= 1

    GaussCoeff[0] = 1.0;
    GaussCoeff[1] = 0.0;
    for (j = 2; j <= 2 * N; j += 2) {
        GaussCoeff[j] = pow(Factorial(j / 2), Gamma); // Gamma = -1 is orig Gauss poly, Gamma = 1 approaches a Butterworth response.
        GaussCoeff[j + 1] = 0.0;
        if ((j / 2) % 2 == 1)
            GaussCoeff[j] *= -1.0;
    }

    // The coefficients are generated in reverse order needed for P51.
    ReverseCoeff(GaussCoeff, N * 2);
    RootsCount = FindRoots(N * 2, GaussCoeff, Roots);

    // Scale the imag part of the root by 1.1 to get a response closer to a Butterworth when Gamma = -2
    for (j = 0; j < N * 2; j++)
        Roots[j] = std::complex<double>(Roots[j].real(), Roots[j].imag() * 1.10);
    return (RootsCount);
}

//---------------------------------------------------------------------------

// These Bessel coefficients are calc'd with the formula given in Johnson & Moore. Page 164. eq 7-26
// The highet term is 1, the rest of the terms are calc'd
int BesselPoly(int NumPoles, std::complex<double> *Roots) {
    int k, N, RootsCount;
    double b, PolyCoeff[P51_ARRAY_SIZE];

    N = NumPoles;
    for (k = N - 1; k >= 0; k--) {
        // b is calc'd as a double because of all the division, but the result is essentially a large int.
        b = Factorial(2 * N - k) / Factorial(k) / Factorial(N - k)
                / pow(2.0, (double) (N - k));
        PolyCoeff[k] = b;
    }
    PolyCoeff[N] = 1.0;

    // The coefficients are generated in reverse order needed for P51.
    ReverseCoeff(PolyCoeff, N);
    RootsCount = FindRoots(N, PolyCoeff, Roots);
    return (RootsCount);
}

//---------------------------------------------------------------------------

// This Inverse Cheby code is identical to the Cheby code, except for two things.
// First, epsilon represents stop band atten, not ripple, so this epsilon is 1/epsilon.
// More importantly, the inverse cheby is defined in terms of F(1/s) instead of the usual F(s);
// After a bit of algebra, it is easy to see that the 1/s terms can be made s terms by simply
// multiplying the num and denom by s^2N. Then the 0th term becomes the highest term, so the
// coefficients are simply fed to the root finder in reverse order.
// Since 1 doesn't get added to the numerator, the lowest 1 or 2 terms will be 0, but we can't
// feed the root finder 0's as the highest term, so we have to be sure not to feed them to the root finder.

int InvChebyPoly(int NumPoles, double StopBanddB, std::complex<double> *ChebyPoles,
        std::complex<double> *ChebyZeros, int *ZeroCount) {
    int j, k, N, PolesCount;
    double Arg, Epsilon, ChebPolyCoeff[P51_ARRAY_SIZE],
            PolyCoeff[P51_ARRAY_SIZE];
    std::complex<double> SquaredPolyCoeff[P51_ARRAY_SIZE], A, B;

    N = NumPoles;
    Epsilon = 1.0 / (pow(10.0, StopBanddB / 10.0) - 1.0); // actually Epsilon Squared

    // This algorithm is from the paper by Richard J Mathar. It generates the coefficients for the Cheb poly.
    // It stores the Nth order coefficient in ChebPolyCoeff[N], and so on. Every other Cheb coeff is 0. See Wikipedia for a table that this code will generate.
    for (j = 0; j <= N / 2; j++) {
        Arg = Factorial(N - j - 1) / Factorial(j) / Factorial(N - 2 * j);
        if (j % 2 == 1)
            Arg *= -1.0;
        Arg *= pow(2.0, (double) (N - 2 * j)) * (double) N / 2.0;
        ChebPolyCoeff[N - 2 * j] = Arg;
        if (N - (2 * j + 1) >= 0) {
            ChebPolyCoeff[N - (2 * j + 1)] = 0.0;
        }
    }

    // Now square the Chebshev polynomial where we assume s = jw.  To get the signs correct,
    // we need to take j to the power. Then its a simple matter of adding powers and
    // multiplying coefficients. j and k represent the exponents. That is, j=3 is the x^3 coeff, and so on.
    for (j = 0; j <= 2 * N; j++)
        SquaredPolyCoeff[j] = std::complex<double>(0.0, 0.0);

    for (j = 0; j <= N; j++)
        for (k = 0; k <= N; k++) {
            A = pow(std::complex<double>(0.0, 1.0), (double) j) * ChebPolyCoeff[j];
            B = pow(std::complex<double>(0.0, 1.0), (double) k) * ChebPolyCoeff[k];
            SquaredPolyCoeff[j + k] = SquaredPolyCoeff[j + k] + A * B; // these end up entirely real.
        }

    // Denominator
    // Now we multiply the coefficients by Epsilon and add 1 to the denominator poly.
    k = 0;
    for (j = 0; j <= 2 * N; j++)
        ChebPolyCoeff[j] = SquaredPolyCoeff[j].real() * Epsilon;
    ChebPolyCoeff[0] += 1.0;
    for (j = 0; j <= 2 * N; j++)
        PolyCoeff[k++] = ChebPolyCoeff[j]; // Note this order is reversed from the Chebyshev routine.
    k--;
    PolesCount = FindRoots(k, PolyCoeff, ChebyPoles);

    // Numerator
    k = 0;
    for (j = 0; j <= 2 * N; j++)
        ChebPolyCoeff[j] = SquaredPolyCoeff[j].real(); // Not using Epsilon here so the check for 0 on the next line is easier. Since the root finder normalizes the poly, it gets factored out anyway.
    for (j = 0; j <= 2 * N; j++)
        if (fabs(ChebPolyCoeff[j]) > 0.01)
            break; // Get rid of the high order zeros. There will be eithe 0ne or two zeros to delete.
    for (; j <= 2 * N; j++)
        PolyCoeff[k++] = ChebPolyCoeff[j];
    k--;
    *ZeroCount = FindRoots(k, PolyCoeff, ChebyZeros);

    return (PolesCount);

}

//---------------------------------------------------------------------------

// The complete Papouls poly is 1 + Epsilon * P(n) where P(n) is the 2*N Legendre poly.
int PapoulisPoly(int NumPoles, std::complex<double> *Roots) {
    int j, N, RootsCount;
    double Epsilon, PolyCoeff[P51_ARRAY_SIZE];

    N = NumPoles;
    for (j = 0; j < 2 * N; j++)
        PolyCoeff[j] = 0.0; // so we don't have to fill all the zero's.

    switch (N) {
    case 1:  // 1 pole
        PolyCoeff[2] = 1.0;
        break;

    case 2:  // 2 pole
        PolyCoeff[4] = 1.0;
        break;

    case 3:  // 3 pole
        PolyCoeff[6] = 3.0;
        PolyCoeff[4] = -3.0;
        PolyCoeff[2] = 1.0;
        break;

    case 4:
        PolyCoeff[8] = 6.0;
        PolyCoeff[6] = -8.0;
        PolyCoeff[4] = 3.0;
        break;

    case 5:
        PolyCoeff[10] = 20.0;
        PolyCoeff[8] = -40.0;
        PolyCoeff[6] = 28.0;
        PolyCoeff[4] = -8.0;
        PolyCoeff[2] = 1.0;
        break;

    case 6:
        PolyCoeff[12] = 50.0;
        PolyCoeff[10] = -120.0;
        PolyCoeff[8] = 105.0;
        PolyCoeff[6] = -40.0;
        PolyCoeff[4] = 6.0;
        break;

    case 7:
        PolyCoeff[14] = 175.0;
        PolyCoeff[12] = -525.0;
        PolyCoeff[10] = 615.0;
        PolyCoeff[8] = -355.0;
        PolyCoeff[6] = 105.0;
        PolyCoeff[4] = -15.0;
        PolyCoeff[2] = 1.0;
        break;

    case 8:
        PolyCoeff[16] = 490.0;
        PolyCoeff[14] = -1680.0;
        PolyCoeff[12] = 2310.0;
        PolyCoeff[10] = -1624.0;
        PolyCoeff[8] = 615.0;
        PolyCoeff[6] = -120.0;
        PolyCoeff[4] = 10.0;
        break;

    case 9:
        PolyCoeff[18] = 1764.0;
        PolyCoeff[16] = -7056.0;
        PolyCoeff[14] = 11704.0;
        PolyCoeff[12] = -10416.0;
        PolyCoeff[10] = 5376.0;
        PolyCoeff[8] = -1624.0;
        PolyCoeff[6] = 276.0;
        PolyCoeff[4] = -24.0;
        PolyCoeff[2] = 1.0;
        break;

    case 10:
        PolyCoeff[20] = 5292.0;
        PolyCoeff[18] = -23520.0;
        PolyCoeff[16] = 44100.0;
        PolyCoeff[14] = -45360.0;
        PolyCoeff[12] = 27860.0;
        PolyCoeff[10] = -10416.0;
        PolyCoeff[8] = 2310.0;
        PolyCoeff[6] = -280.0;
        PolyCoeff[4] = 15.0;
        break;

    case 11:
        PolyCoeff[22] = 19404;
        PolyCoeff[20] = -97020.0;
        PolyCoeff[18] = 208740.0;
        PolyCoeff[16] = -252840.0;
        PolyCoeff[14] = 189420.0;
        PolyCoeff[12] = -90804.0;
        PolyCoeff[10] = 27860.0;
        PolyCoeff[8] = -5320.0;
        PolyCoeff[6] = 595.0;
        PolyCoeff[4] = -35.0;
        PolyCoeff[2] = 1.0;
        break;

    case 12:
        PolyCoeff[24] = 60984.0;
        PolyCoeff[22] = -332640.0;
        PolyCoeff[20] = 790020.0;
        PolyCoeff[18] = -1071840.0;
        PolyCoeff[16] = 916020.0;
        PolyCoeff[14] = -512784.0;
        PolyCoeff[12] = 189420.0;
        PolyCoeff[10] = -45360.0;
        PolyCoeff[8] = 6720.0;
        PolyCoeff[6] = -560.0;
        PolyCoeff[4] = 21.0;
        break;

    case 13:
        PolyCoeff[26] = 226512.0;
        PolyCoeff[24] = -1359072.0;
        PolyCoeff[22] = 3597264.0;
        PolyCoeff[20] = -5528160.0;
        PolyCoeff[18] = 5462820.0;
        PolyCoeff[16] = -3632112.0;
        PolyCoeff[14] = 1652232.0;
        PolyCoeff[12] = -512784.0;
        PolyCoeff[10] = 106380.0;
        PolyCoeff[8] = -14160.0;
        PolyCoeff[6] = 1128.0;
        PolyCoeff[4] = -48.0;
        PolyCoeff[2] = 1.0;
        break;

    case 14:
        PolyCoeff[28] = 736164.0;
        PolyCoeff[26] = -4756752.0;
        PolyCoeff[24] = 13675662.0;
        PolyCoeff[22] = -23063040.0;
        PolyCoeff[20] = 25322220.0;
        PolyCoeff[18] = -18993744.0;
        PolyCoeff[16] = 9934617.0;
        PolyCoeff[14] = -3632112.0;
        PolyCoeff[12] = 916020.0;
        PolyCoeff[10] = -154560.0;
        PolyCoeff[8] = 16506.0;
        PolyCoeff[6] = -1008.0;
        PolyCoeff[4] = 28.0;
        break;

    case 15:
        PolyCoeff[30] = 2760615.0;
        PolyCoeff[28] = -19324305.0;
        PolyCoeff[26] = 60747687.0;
        PolyCoeff[24] = -113270157.0;
        PolyCoeff[22] = 139378239.0;
        PolyCoeff[20] = -119144025.0;
        PolyCoeff[18] = 72539775.0;
        PolyCoeff[16] = -31730787.0;
        PolyCoeff[14] = 9934617.0;
        PolyCoeff[12] = -2191959.0;
        PolyCoeff[10] = 331065.0;
        PolyCoeff[8] = -32655.0;
        PolyCoeff[6] = 1953.0;
        PolyCoeff[4] = -63.0;
        PolyCoeff[2] = 1.0;
        break;

    case 16:
        PolyCoeff[32] = 9202050.0;
        PolyCoeff[30] = -68708640.0;
        PolyCoeff[28] = 231891660.0;
        PolyCoeff[26] = -467747280.0;
        PolyCoeff[24] = 628221594.0;
        PolyCoeff[22] = -592431840.0;
        PolyCoeff[20] = 403062660.0;
        PolyCoeff[18] = -200142800.0;
        PolyCoeff[16] = 72539775.0;
        PolyCoeff[14] = -18993744.0;
        PolyCoeff[12] = 3515820.0;
        PolyCoeff[10] = -443520.0;
        PolyCoeff[8] = 35910.0;
        PolyCoeff[6] = -1680.0;
        PolyCoeff[4] = 36.0;
        break;

    case 17:
        PolyCoeff[34] = 34763300.0;
        PolyCoeff[32] = -278106400.0;
        PolyCoeff[30] = 1012634480.0;
        PolyCoeff[28] = -2221579360.0;
        PolyCoeff[26] = 3276433160.0;
        PolyCoeff[24] = -3431908480.0;
        PolyCoeff[22] = 2629731104.0;
        PolyCoeff[20] = -1496123200.0;
        PolyCoeff[18] = 634862800.0;
        PolyCoeff[16] = -200142800.0;
        PolyCoeff[14] = 46307800.0;
        PolyCoeff[12] = -7696304.0;
        PolyCoeff[10] = 888580.0;
        PolyCoeff[8] = -67760.0;
        PolyCoeff[6] = 3160.0;
        PolyCoeff[4] = -80.0;
        PolyCoeff[2] = 1.0;
        break;

    case 18:
        PolyCoeff[36] = 118195220.0;
        PolyCoeff[34] = -1001183040.0;
        PolyCoeff[32] = 3879584280.0;
        PolyCoeff[30] = -9110765664.0;
        PolyCoeff[28] = 14480345880.0;
        PolyCoeff[26] = -16474217760.0;
        PolyCoeff[24] = 13838184360.0;
        PolyCoeff[22] = -8725654080.0;
        PolyCoeff[20] = 4158224928.0;
        PolyCoeff[18] = -1496123200.0;
        PolyCoeff[16] = 403062660.0;
        PolyCoeff[14] = -79999920.0;
        PolyCoeff[12] = 11397540.0;
        PolyCoeff[10] = -1119888.0;
        PolyCoeff[8] = 71280.0;
        PolyCoeff[6] = -2640.0;
        PolyCoeff[4] = 45.0;
        break;

    case 19:
        PolyCoeff[38] = 449141836.0;
        PolyCoeff[36] = -4042276524.0;
        PolyCoeff[34] = 16732271556.0;
        PolyCoeff[32] = -42233237904.0;
        PolyCoeff[30] = 72660859128.0;
        PolyCoeff[28] = -90231621480.0;
        PolyCoeff[26] = 83545742280.0;
        PolyCoeff[24] = -58751550000.0;
        PolyCoeff[22] = 31671113760.0;
        PolyCoeff[20] = -13117232128.0;
        PolyCoeff[18] = 4158224928.0;
        PolyCoeff[16] = -999092952.0;
        PolyCoeff[14] = 178966788.0;
        PolyCoeff[12] = -23315292.0;
        PolyCoeff[10] = 2130876.0;
        PolyCoeff[8] = -129624.0;
        PolyCoeff[6] = 4851.0;
        PolyCoeff[4] = -99.0;
        PolyCoeff[2] = 1.0;
        break;

    case 20:
        PolyCoeff[40] = 1551580888.0;
        PolyCoeff[38] = -14699187360.0;
        PolyCoeff[36] = 64308944700.0;
        PolyCoeff[34] = -172355177280.0;
        PolyCoeff[32] = 316521742680.0;
        PolyCoeff[30] = -422089668000.0;
        PolyCoeff[28] = 422594051880.0;
        PolyCoeff[26] = -323945724960.0;
        PolyCoeff[24] = 192167478360.0;
        PolyCoeff[22] = -88572527680.0;
        PolyCoeff[20] = 31671113760.0;
        PolyCoeff[18] = -8725654080.0;
        PolyCoeff[16] = 1829127300.0;
        PolyCoeff[14] = -286125840.0;
        PolyCoeff[12] = 32458140.0;
        PolyCoeff[10] = -2560272.0;
        PolyCoeff[8] = 131670.0;
        PolyCoeff[6] = -3960.0;
        PolyCoeff[4] = 55.0;
        break;
    }

    Epsilon = 0.1; // This controls the amount of pass band roll off.  0.01 < Epsilon < 0.250

    // The poly is in terms of omega, but we need it in term of s = jw. So we need to
    // multiply the approp coeff by neg 1 to account for j. Then mult by epsilon.
    for (j = 0; j <= 2 * N; j++) {
        if ((j / 2) % 2 == 1)
            PolyCoeff[j] *= -1.0;
        PolyCoeff[j] *= Epsilon;
    }

    // Now add 1 to the poly.
    PolyCoeff[0] = 1.0;

    // The coefficients are in reverse order needed for P51.
    ReverseCoeff(PolyCoeff, N * 2);
    RootsCount = FindRoots(N * 2, PolyCoeff, Roots);

    return (RootsCount);
}

//---------------------------------------------------------------------------

// This code was described in "Elliptic Functions for Filter Design"
// H J Orchard and Alan N Willson  IEE Trans on Circuits and Systems April 97
// The equation numbers in the comments are from the paper.
// As the stop band attenuation -> infinity, the Elliptic -> Chebyshev.
int EllipticPoly(int FiltOrder, double Ripple, double DesiredSBdB,
        std::complex<double> *EllipPoles, std::complex<double> *EllipZeros, int *ZeroCount) {
    int j, k, n, LastK;
    double K[ELLIPARRAYSIZE], G[ELLIPARRAYSIZE], Epsilon[ELLIPARRAYSIZE];
    double A, D, SBdB, dBErr, RealPart, ImagPart;
    double DeltaK, PrevErr, Deriv;
    std::complex<double> C;

    for (j = 0; j < ELLIPARRAYSIZE; j++)
        K[j] = G[j] = Epsilon[j] = 0.0;
    if (Ripple < 0.001)
        Ripple = 0.001;
    if (Ripple > 1.0)
        Ripple = 1.0;
    Epsilon[0] = sqrt(pow(10.0, Ripple / 10.0) - 1.0);

    // Estimate K[0] to get the algorithm started.
    K[0] = (double) (FiltOrder - 2) * 0.1605 + 0.016;
    if (K[0] < 0.01)
        K[0] = 0.01;
    if (K[0] > 0.7)
        K[0] = 0.7;

    // This loop calculates K[0] for the desired stopband attenuation. It typically loops < 5 times.
    for (j = 0; j < MAX_ELLIP_ITER; j++) {
        // Compute K with a forward Landen Transformation.
        for (k = 1; k < 10; k++) {
            K[k] = pow(K[k - 1] / (1.0 + sqrt(1.0 - K[k - 1] * K[k - 1])), 2.0); // eq. 10
            if (K[k] <= 1.0E-6)
                break;
        }
        LastK = k;

        // Compute G with a backwards Landen Transformation.
        G[LastK] = 4.0 * pow(K[LastK] / 4.0, (double) FiltOrder);
        for (k = LastK; k >= 1; k--) {
            G[k - 1] = 2.0 * sqrt(G[k]) / (1.0 + G[k]);  // eq. 9
        }

        if (G[0] <= 0.0)
            G[0] = 1.0E-10;
        SBdB = 10.0 * log10(1.0 + pow(Epsilon[0] / G[0], 2.0)); // Current stopband attenuation dB
        dBErr = DesiredSBdB - SBdB;

        if (fabs(dBErr) < 0.1)
            break;
        if (j == 0) // Do this on the 1st loop so we can calc a derivative.
                {
            if (dBErr > 0)
                DeltaK = 0.005;
            else
                DeltaK = -0.005;
            PrevErr = dBErr;
        } else {
            // Use Newtons Method to adjust K[0].
            Deriv = (PrevErr - dBErr) / DeltaK;
            PrevErr = dBErr;
            if (Deriv == 0.0)
                break; // This happens when K[0] hits one of the limits set below.
            DeltaK = dBErr / Deriv;
            if (DeltaK > 0.1)
                DeltaK = 0.1;
            if (DeltaK < -0.1)
                DeltaK = -0.1;
        }
        K[0] -= DeltaK;
        if (K[0] < 0.001)
            K[0] = 0.001;  // must not be < 0.0
        if (K[0] > 0.990)
            K[0] = 0.990; // if > 0.990 we get a pole in the RHP. This means we were unable to set the stop band atten to the desired level (the Ripple is too large for the Pole Count).
    }

    // Epsilon[0] was calulated above, now calculate Epsilon[LastK] from G
    for (j = 1; j <= LastK; j++) {
        A = (1.0 + G[j]) * Epsilon[j - 1] / 2.0;  // eq. 37
        Epsilon[j] = A + sqrt(A * A + G[j]);
    }

    // Calulate the poles and zeros.
    ImagPart = log(
            (1.0 + sqrt(1.0 + Epsilon[LastK] * Epsilon[LastK]))
                    / Epsilon[LastK]) / (double) FiltOrder;  // eq. 22
    n = 0;
    for (j = 1; j <= FiltOrder / 2; j++) {
        RealPart = (double) (2 * j - 1) * M_PI_2 / (double) FiltOrder; // eq. 19
        C = std::complex<double>(0.0, -1.0) / cos(std::complex<double>(-RealPart, ImagPart));      // eq. 20
        D = 1.0 / cos(RealPart);
        for (k = LastK; k >= 1; k--) {
            C = (C - K[k] / C) / (1.0 + K[k]);  // eq. 36
            D = (D + K[k] / D) / (1.0 + K[k]);
        }

        EllipPoles[n] = 1.0 / C;
        EllipPoles[n + 1] = std::conj(EllipPoles[n]);
        EllipZeros[n] = std::complex<double>(0.0, D / K[0]);
        EllipZeros[n + 1] = std::conj(EllipZeros[n]);
        n += 2;
    }
    *ZeroCount = n; // n is the num zeros

    if (FiltOrder % 2 == 1)   // The real pole for odd pole counts
            {
        A = 1.0 / sinh(ImagPart);
        for (k = LastK; k >= 1; k--) {
            A = (A - K[k] / A) / (1.0 + K[k]);      // eq. 38
        }
        EllipPoles[n] = std::complex<double>(-1.0 / A, 0.0);
        n++;
    }

    return (n); // n is the num poles. There will be 1 more pole than zeros for odd pole counts.

}
//---------------------------------------------------------------------------

} // namespace
