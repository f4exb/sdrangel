
//---------------------------------------------------------------------------

#include "PFiftyOneRevE.h"
#include "QuadRootsRevH.h"
#include <math.h>
#include <complex>

//---------------------------------------------------------------------------
/*
 By Daniel Klostermann
 Iowa Hills Software, LLC  IowaHills.com
 Sept 21, 2016  Rev E

 If you find a problem with this code, please leave a note on:
 http://www.iowahills.com/feedbackcomments.html

 This is for polynomials with real coefficients, up to 100th order.

 PFiftyOne has 4 loops. The outermost while loop runs until the polynomial's order
 N, has been reduced to 1 or 2.

 The TypeOfK loop controls how the K polynomial is initialized. About 99.999% of all
 roots are found with TypeOfK = 0.

 The AngleNumber loop controls how we initialize the 2nd order polynomial TUV.
 The angles move between the 1st and 2nd quadrants and are defined in the
 SetTUVandK() function. About 99% of roots are found in the 1st 4 angles.

 The Iter loop first calls the QuadIterate routine, which finds quadratic factors,
 real or complex. If QuadIterate fails to converge in QUAD_ITER_MAX iterations,
 the RealIterate routine is called to try to extract a single real root.

 The input array Coeff[] contains the coefficients arranged in descending order.
 For example, a 4th order poly would be:
 P(x) = Coeff[0]*x^4 + Coeff[1]*x^3 + Coeff[2]*x^2 + Coeff[3]*x + Coeff[4]

 On return, there is no particular ordering to the roots, but complex root pairs
 will be in adjacent array cells. It may be helpful to know that the complex root pairs
 are exact cunjugates of each other, meaning that you can use the == operator
 to search for complex pairs in the arrays.F

 This returns Degree on success, 0 on failure.
 */

namespace kitiirfir
{

//---------------------------------------------------------------------------
// None of the code uses long double variables except for the P51 root finder code
// where long doubles were essential. FindRoots is an interface to P51 for code that is
// using complex variables CplxD.
int FindRoots(int N, double *Coeff, std::complex<double> *Roots) {
    int j;
    long double P[P51_ARRAY_SIZE], RealRoot[P51_ARRAY_SIZE],
            ImagRoot[P51_ARRAY_SIZE];

    for (j = 0; j <= N; j++)
        P[j] = Coeff[j];      // double to long double
    N = PFiftyOne(P, N, RealRoot, ImagRoot);
    for (j = 0; j < N; j++)
        Roots[j] = std::complex<double>((double) RealRoot[j],
                (double) ImagRoot[j]); // long double to double
    return (N);
}

//---------------------------------------------------------------------------

int PFiftyOne(long double *Coeff, int Degree, long double *RealRoot,
        long double *ImagRoot) {
    if (Degree > P51_MAXDEGREE || Degree < 0) {
        //ShowMessage("Poly Degree is out of range in the P51 Root Finder.");
        return (0);
    }

    TUpdateStatus UpdateStatus;
    int N, NZ, j, Iter, AngleNumber, TypeOfK;
    long double RealZero, QuadX;
    long double TUV[3];
    long double *P, *QuadQP, *RealQP, *QuadK, *RealK, *QK;

    N = Degree;  // N is decremented as roots are found.

    P = new (std::nothrow) long double[N + 2];
    QuadQP = new (std::nothrow) long double[N + 2];
    RealQP = new (std::nothrow) long double[N + 2];
    QuadK = new (std::nothrow) long double[N + 2];
    RealK = new (std::nothrow) long double[N + 2];
    QK = new (std::nothrow) long double[N + 2];
    if (P == 0 || QuadQP == 0 || RealQP == 0 || QuadK == 0
            || RealK == 0 || QK == 0) {
        //ShowMessage("Memory not Allocated in PFiftyOne root finder.");
        return (0);
    }

    for (j = 0; j <= N; j++)
        P[j] = Coeff[j];                // Copy the coeff. P gets modified.
    for (j = 0; j < N; j++)
        RealRoot[j] = ImagRoot[j] = 0.0; // Init to zero, in case any leading or trailing zeros are removed.

    // Remove trailing zeros. A tiny P[N] relative to P[N-1] is not allowed.
    while (N > 0 && fabsl(P[N]) <= TINY_VALUE * fabsl(P[N - 1])) {
        N--;
    }

    // Remove leading zeros.
    while (N > 0 && P[0] == 0.0) {
        for (j = 0; j < N; j++)
            P[j] = P[j + 1];
        N--;
    }

    // P[0] must = 1
    if (P[0] != 1.0) {
        for (j = 1; j <= N; j++)
            P[j] /= P[0];
        P[0] = 1.0;
    }

    TypeOfK = 0;
    while (N > 4 && TypeOfK < MAX_NUM_K) {
        NZ = 0;    // Num Zeros found. (Used in the loop controls below.)
        QuadX = powl(fabsl(P[N]), 1.0 / (long double) N) / 2.0; // QuadX is used to init TUV

        for (TypeOfK = 0; TypeOfK < MAX_NUM_K && NZ == 0; TypeOfK++) // Iterate on the different possible QuadK inits.
                {
            for (AngleNumber = 0; AngleNumber < MAX_NUM_ANGLES && NZ == 0;
                    AngleNumber++)  // Iterate on the angle used to init TUV.
                    {
                SetTUVandK(P, N, TUV, RealK, QuadK, QuadX, AngleNumber,
                        TypeOfK);     // Init TUV and both K's
                for (Iter = 0; Iter < N && NZ == 0; Iter++) // Allow N calls to QuadIterate for N*QUAD_ITER_MAX iterations, then try a different angle.
                        {
                    NZ = QuadIterate(Iter, P, QuadQP, QuadK, QK, N, TUV,
                            &UpdateStatus); // NZ = 2 for a pair of complex roots or 2 real roots.

                    if (NZ == 0) // Try for a real root.
                            {
                        if (fabsl(QuadK[N - 2]) > TINY_VALUE * fabsl(P[N]))
                            RealZero = -P[N] / QuadK[N - 2]; // This value gets refined by QuadIterate.
                        else
                            RealZero = 0.0;
                        NZ = RealIterate(Iter, P, RealQP, RealK, QK, N,
                                &RealZero);    // NZ = 1 for a single real root.
                    }

                    if (NZ == 0 && UpdateStatus == BAD_ANGLE)
                        break; // If RealIterate failed and UpdateTUV called this a bad angle, it's pointless to iterate further on this angle.

                } // Iter loop       Note the use of NZ in the loop controls.
            } // AngleNumber loop
        } // TypeOfK loop

        // Done iterating. If NZ==0 at this point, we failed and will exit below.
        // Decrement N, and set P to the quotient QP. QP = P/TUV or QP = P/(x-RealZero)
        if (NZ == 2) // Store a pair of complex roots or 2 real roots.
                {
            j = Degree - N;
            QuadRoots(TUV, &RealRoot[j], &ImagRoot[j]);
            N -= NZ;
            for (j = 0; j <= N; j++)
                P[j] = QuadQP[j];
            TypeOfK = 0;
        }

        if (NZ == 1)  // Store a single real root
                {
            j = Degree - N;
            RealRoot[j] = RealZero;
            ImagRoot[j] = 0.0;
            N -= NZ;
            for (j = 0; j <= N; j++)
                P[j] = RealQP[j];
            TypeOfK = 0;
        }

        // Remove any trailing zeros on P.  P[N] should never equal zero, but can approach  zero
        // because of roundoff errors. If P[N] is zero or tiny relative to P[N-1], we take the hit,
        // and place a root at the origin. This needs to be checked, but virtually never happens.
        while (fabsl(P[N]) <= TINY_VALUE * fabsl(P[N - 1]) && N > 0) {
            j = Degree - N;
            RealRoot[j] = 0.0;
            ImagRoot[j] = 0.0;
            N--;
            //ShowMessage("Removed a zero at the origin.");
        }

    }  // The outermost loop while(N > 2)

    delete[] QuadQP;
    delete[] RealQP;
    delete[] QuadK;
    delete[] RealK;
    delete[] QK;

    // Done, except for the last 1 or 2 roots. If N isn't 1 or 2 at this point, we failed.
    if (N == 1) {
        j = Degree - N;
        RealRoot[j] = -P[1] / P[0];
        ImagRoot[j] = 0.0;
        delete[] P;
        return (Degree);
    }

    if (N == 2) {
        j = Degree - N;
        QuadRoots(P, &RealRoot[j], &ImagRoot[j]);
        delete[] P;
        return (Degree);
    }

    if (N == 3) {
        j = Degree - N;
        CubicRoots(P, &RealRoot[j], &ImagRoot[j]);
        delete[] P;
        return (Degree);
    }

    if (N == 4) {
        j = Degree - N;
        BiQuadRoots(P, &RealRoot[j], &ImagRoot[j]);
        delete[] P;
        return (Degree);
    }

    // ShowMessage("The P51 root finder failed to converge on a solution.");
    return (0);

}

//---------------------------------------------------------------------------
// The purpose of this function is to find a 2nd order polynomial, TUV, which is a factor of P.
// When called, the TUV and K polynomials have been initialized to our best guess.
// This function can call UpdateTUV() at most QUAD_ITER_MAX times. This returns 2 if we find
// 2 roots, else 0. UpdateStatus has two possible values on return, UPDATED and BAD_ANGLE.
// UPDATED means the UpdateTUV function is able to do its calculations with TUV at this angle.
// BAD_ANGLE tells P51 to move TUV to a different angle before calling this function again.
// We use ErrScalar to account for the wide variations in N and P[N]. ErrScalar can vary by
// as much as 1E50 if all the root locations are much less than 1 or much greater than 1.
// The ErrScalar equation was determined empirically. If this root finder is used in an app
// where the root locations and poly orders fall within a narrow range, esp low order, then
// ErrScalar can be modified to give more accurate results.

int QuadIterate(int P51_Iter, long double *P, long double *QP, long double *K,
        long double *QK, int N, long double *TUV, TUpdateStatus *UpdateStatus) {
    int Iter;
    long double Err, MinErr, ErrScalar, QKCheck;

    ErrScalar = 1.0 / (16.0 * powl((long double) N, 3.0) * fabsl(P[N]));

    P51_Iter *= QUAD_ITER_MAX;
    Err = MinErr = 1.0E100;
    *UpdateStatus = UPDATED;
    QuadSynDiv(P, N, TUV, QP);    // Init QP
    QuadSynDiv(K, N - 1, TUV, QK);  // Init QK

    for (Iter = 0; Iter < QUAD_ITER_MAX; Iter++) {
        UpdateTUV(P51_Iter + Iter, P, N, QP, K, QK, TUV, UpdateStatus);
        if (*UpdateStatus == BAD_ANGLE) {
            return (0);  // Failure, need a different angle.
        }

        Err = fabsl(QP[N - 1]) + fabsl(QP[N + 1]); // QP[N-1] & QP[N+1] are the remainder terms of P/TUV.
        Err *= ErrScalar;                         // Normalize the error.

        if (Err < LDBL_EPSILON) {
            return (2); // Success!!  2 roots have been found.
        }

        // ZERO_DEL means both DelU and DelV were ~ 0 in UpdateTUV which means the algorithm has stalled.
        // It might be stalled in a dead zone with large error, or stalled because it can't adjust u and v with a resolution fine enough to meet our convergence criteria.
        if (*UpdateStatus == ZERO_DEL) {
            if (Err < 4.0 * (long double) N * LDBL_EPSILON) // Small error, this is the best we can do.
            {
                *UpdateStatus = UPDATED;
                return (2);
            } else  // Large error, get a different angle
            {
                *UpdateStatus = BAD_ANGLE;
                return (0);
            }
        }

        QKCheck = fabsl(QK[N - 2]) + fabsl(QK[N]); // QK[N-2] & QK[N] are the remainder terms of K/TUV.
        QKCheck *= ErrScalar;

        // Huge values indicate the algorithm is diverging and overflow is imminent. This can indicate
        // a single real root, or that we simply need a different angle on TUV. This happens frequently.
        if (Err > HUGE_VALUE || QKCheck > HUGE_VALUE) {
            *UpdateStatus = BAD_ANGLE;
            return (0);
        }

        // Record our best result thus far. We turn on the damper in UpdateTUV if the errs increase.
        if (Err < MinErr) {
            *UpdateStatus = DAMPER_OFF;
            MinErr = Err;
        } else if (Iter > 2) {
            *UpdateStatus = DAMPER_ON;
        }
    }

    // If we get here, we didn't converge, but TUV is getting updated.
    // If RealIterate can't find a real zero, this function will get called again.
    *UpdateStatus = UPDATED;
    return (0);
}

//---------------------------------------------------------------------------

// This function updates TUV[1] and TUV[2] using the J-T mathematics.
// When called, UpdateStatus equals either DAMPER_ON or DAMPER_OFF, which controls the damping factor.
// On return UpdateStatus equals UPDATED, BAD_ANGLE or ZERO_DEL;
// BAD_ANGLE indicates imminent overflow, or TUV[2] is going to zero. In either case the QuadIterate
// function will immediately return to P51 which will change the angle on TUV. ZERO_DEL indicates
// we have either stalled in a dead zone, or we lack the needed precision to further refine TUV.
// TUV = tx^2 + ux + v    t = 1 always.   v = 0 (a root at the origin) isn't allowed.
// The poly degrees are P[N] QP[N-2] K[N-1] QK[N-3]
void UpdateTUV(int Iter, long double *P, int N, long double *QP, long double *K,
        long double *QK, long double *TUV, TUpdateStatus *UpdateStatus) {
    int j;
    long double DelU, DelV, Denom;
    long double E2, E3, E4, E5;
    static int FirstDamperlIter;

    if (Iter == 0)
        FirstDamperlIter = 0;  // Reset this static var.
    if (*UpdateStatus == DAMPER_ON)
        FirstDamperlIter = Iter;

    // Update K, unless E3 is tiny relative to E2. The algorithm will work its way out of a tiny E3.
    // These equations are from the Jenkins and Traub paper "A Three Stage Algorithm for Real Polynomials Using Quadratic Iteration" Equation 9.8
    E2 = QP[N] * QP[N] + TUV[1] * QP[N] * QP[N - 1]
            + TUV[2] * QP[N - 1] * QP[N - 1];
    E3 = QP[N] * QK[N - 1] + TUV[1] * QP[N] * QK[N - 2]
            + TUV[2] * QP[N - 1] * QK[N - 2];

    if (fabsl(E3) * HUGE_VALUE > fabsl(E2)) {
        E2 /= -E3;
        for (j = 1; j <= N - 2; j++)
            K[j] = E2 * QK[j - 1] + QP[j]; // At covergence, E2 ~ 0, so K ~ QP.
    } else {
        for (j = 1; j <= N - 2; j++)
            K[j] = QK[j - 1];
    }
    K[0] = QP[0]; // QP[0] = 1.0 always
    K[N - 1] = 0.0;

    QuadSynDiv(K, N - 1, TUV, QK); // Update QK   QK = K/TUV

    // These equations are modified versions of those used in the original Jenkins Traub Fortran algorithm RealPoly, found at  www.netlib.org/toms/493
    E3 = QP[N] * QK[N - 1] + TUV[1] * QP[N] * QK[N - 2]
            + TUV[2] * QP[N - 1] * QK[N - 2];
    E4 = QK[N - 1] * QK[N - 1] + TUV[1] * QK[N - 1] * QK[N - 2]
            + TUV[2] * QK[N - 2] * QK[N - 2];
    E5 = QP[N - 1] * QK[N - 1] - QP[N] * QK[N - 2];

    Denom = E5 * K[N - 2] * TUV[2] + E4 * P[N];
    if (fabsl(Denom) * HUGE_VALUE < fabsl(P[N])) {
        *UpdateStatus = BAD_ANGLE; // Denom is tiny, overflow is imminent, get a new angle.
        return;
    }

    // Calc DelU and DelV. If they are effectively zero, bump them by epsilon.
    DelU = E3 * K[N - 2] * TUV[2] / Denom;
    if (fabsl(DelU) < LDBL_EPSILON * fabsl(TUV[1])) {
        if (DelU < 0.0)
            DelU = -fabsl(TUV[1]) * LDBL_EPSILON;
        else
            DelU = fabsl(TUV[1]) * LDBL_EPSILON;
    }

    DelV = -E5 * K[N - 2] * TUV[2] * TUV[2] / Denom;
    if (fabsl(DelV) < LDBL_EPSILON * fabsl(TUV[2])) {
        if (DelV < 0.0)
            DelV = -fabsl(TUV[2]) * LDBL_EPSILON;
        else
            DelV = fabsl(TUV[2]) * LDBL_EPSILON;
    }

    // If we haven't converged by QUAD_ITER_MAX iters, we need to test DelU and DelV for effectiveness.
    if (Iter >= QUAD_ITER_MAX - 1) {
        // We can't improve u and v any further because both DelU and DelV ~ 0 This usually happens when we are near a solution, but we don't have the precision needed to ine u and v enough to meet our convergence criteria. This can also happen in a dead zone where the errors are large, which means we need a different angle on TUV. We test for this in the QuadIterate function.
        if (fabsl(DelU) < 8.0 * LDBL_EPSILON * fabsl(TUV[1])
                && fabsl(DelV) < 8.0 * LDBL_EPSILON * fabsl(TUV[2])) {
            *UpdateStatus = ZERO_DEL;
            return;
        }
        // A big change after this many iterations means we are wasting our time on this angle.
        if (fabsl(DelU) > 10.0 * fabsl(TUV[1])
                || fabsl(DelV) > 10.0 * fabsl(TUV[2])) {
            *UpdateStatus = BAD_ANGLE;
            return;
        }
    }

    // Dampen the changes for 3 iterations after Damper was set in QuadIterate.
    if (Iter - FirstDamperlIter < 3) {
        DelU *= 0.75;
        DelV *= 0.75;
    }

    // Update U and V
    TUV[1] += DelU;
    if (fabsl(TUV[2] + DelV) < TINY_VALUE)
        DelV *= 0.9; // If this, we would set TUV[2] = 0 which we can't allow, so we use 90% of DelV.
    TUV[2] += DelV;

    if (fabsl(TUV[2]) < fabsl(TUV[1]) * TINY_VALUE) {
        *UpdateStatus = BAD_ANGLE; // TUV[2] is effectively 0, which is never allowed.
        return;
    }

    *UpdateStatus = UPDATED;    // TUV was updated.
    QuadSynDiv(P, N, TUV, QP);  // Update QP  QP = P/TUV
}

//---------------------------------------------------------------------------

// This function is used to find single real roots. It is similar to Newton's Method.
// Horners method is used to calculate QK and QP. For an explanation of these methods, see these 2 links.
// http://mathworld.wolfram.com/NewtonsMethod.html  http://en.wikipedia.org/wiki/Horner%27s_method

// When called, RealZero contains our best guess for a root, and K is init to the 1st derivative of P.
// The return value is either 1 or 0, the number of roots found. On return, RealZero contains
// the root, and QP contains the next P (i.e. P with this root removed).
// As with QuadIterate, at convergence, K = QP
int RealIterate(int P51_Iter, long double *P, long double *QP, long double *K,
        long double *QK, int N, long double *RealZero) {
    int Iter, k;
    long double X, DelX, Damper, Err, ErrScalar;
    static long double PrevQPN;

    ErrScalar = 1.0 / (16.0 * powl((long double) N, 2.0) * fabsl(P[N]));

    X = *RealZero;        // Init with our best guess for X.
    if (P51_Iter == 0)
        PrevQPN = 0.0;
    QK[0] = K[0];
    for (k = 1; k <= N - 1; k++) {
        QK[k] = QK[k - 1] * X + K[k];
    }

    for (Iter = 0; Iter < REAL_ITER_MAX; Iter++) {
        // Calculate a new QP.  This is poly division  QP = P/(x+X)  QP[0] to QP[N-1] is the quotient.
        // The remainder is QP[N], which is P(X), the error term.
        QP[0] = P[0];
        for (k = 1; k <= N; k++) {
            QP[k] = QP[k - 1] * X + P[k];
        }
        Err = fabsl(QP[N]) * ErrScalar; // QP[N] is the error. ErrScalar accounts for the wide variations in N and P[N].

        if (Err < LDBL_EPSILON) {
            *RealZero = X;
            return (1);      // Success!!
        } else if (Err > HUGE_VALUE)
            return (0);  // Overflow is imminent.

        // Calculate a new K.  QK[N-1] is the remainder of K /(x-X).
        // QK[N-1] is approximately P'(X) when P(X) = QP[N] ~ 0
        if (fabsl(QK[N - 1]) > fabsl(P[N] * TINY_VALUE)) {
            DelX = -QP[N] / QK[N - 1];
            K[0] = QP[0];
            for (k = 1; k <= N - 1; k++) {
                K[k] = DelX * QK[k - 1] + QP[k];
            }
        } else  // Use this if QK[N-1] ~ 0
        {
            K[0] = 0.0;
            for (k = 1; k <= N - 1; k++)
                K[k] = QK[k - 1];
        }

        if (fabsl(K[N - 1]) > HUGE_VALUE)
            return (0); // Overflow is imminent.

        // Calculate a new QK.  This is poly division QK = K /(x+X).  QK[0] to QK[N-2] is the quotient.
        QK[0] = K[0];
        for (k = 1; k <= N - 1; k++) {
            QK[k] = QK[k - 1] * X + K[k];
        }
        if (fabsl(QK[N - 1]) <= TINY_VALUE * fabsl(P[N]))
            return (0);  // QK[N-1] ~ 0 will cause overflow below.

        // This algorithm routinely oscillates back and forth about a zero with nearly equal pos and neg error magnitudes.
        // If the current and previous error add to give a value less than the current error, we dampen the change.
        Damper = 1.0;
        if (fabsl(QP[N] + PrevQPN) < fabsl(QP[N]))
            Damper = 0.5;
        PrevQPN = QP[N];

        // QP[N] is P(X) and at convergence QK[N-1] ~ P'(X), so this is ~ Newtons Method.
        DelX = QP[N] / QK[N - 1] * Damper;
        if (X != 0.0) {
            if (fabsl(DelX) < LDBL_EPSILON * fabsl(X)) // If true, the algorithm is stalled, so bump X by 2 epsilons.
                    {
                if (DelX < 0.0)
                    DelX = -2.0 * X * LDBL_EPSILON;
                else
                    DelX = 2.0 * X * LDBL_EPSILON;
            }
        } else  // X = 0
        {
            if (DelX == 0.0)
                return (0); // Stalled at the origin, so exit.
        }
        X -= DelX;  // Update X

    } // end of loop

      // If we get here, we failed to converge.
    return (0);
}

//---------------------------------------------------------------------------

// Derivative of P. Called from SetTUVandK().
void DerivOfP(long double *P, int N, long double *dP) {
    int j;
    long double Power;
    for (j = 0; j < N; j++) {
        Power = N - j;
        dP[j] = Power * P[j];
    }
    dP[N] = 0.0;

}
//---------------------------------------------------------------------------

// Synthetic Division of P by x^2 + ux + v (TUV)
// The qotient is Q[0] to Q[N-2].  The actual poly remainder terms are Q[N-1] and Q[N+1]
// The JT math requires the values Q[N-1] and Q[N] to calculate K.
// These form a 1st order remainder b*x + (b*u + a).
void QuadSynDiv(long double *P, int N, long double *TUV, long double *Q) {
    int j;
    Q[0] = P[0];
    Q[1] = P[1] - TUV[1] * Q[0];
    for (j = 2; j <= N; j++) {
        Q[j] = P[j] - TUV[1] * Q[j - 1] - TUV[2] * Q[j - 2];
    }

// Here we calculate the final remainder term used to calculate the convergence error.
// This and Q[N-1] are the remainder values you get if you do this poly division manually.
    Q[N + 1] = Q[N - 1] * TUV[1] + Q[N];  // = b*u + a
}

//---------------------------------------------------------------------------

// This function intializes the TUV and K polynomials.
void SetTUVandK(long double *P, int N, long double *TUV, long double *RealK,
        long double *QuadK, long double X, int AngleNumber, int TypeOfQuadK) {
    int j;
    long double a, Theta;

    // These angles define our search pattern in the complex plane. We start in the 1st quadrant,
    // go to the 2nd, then the real axis, etc. The roots are conjugates, so there isn't a need
    // to go to the 3rd or 4th quadrants. The first 2 angles find about 99% of all roots.
    const long double Angle[] = { 45.0, 135.0, 0.0, 90.0, 15.0, 30.0, 60.0,
            75.0, 105.0, 120.0, 150.0,
            165.0,  // 12 angles
            6.0, 51.0, 96.0, 141.0, 12.0, 57.0, 102.0, 147.0, 21.0, 66.0, 111.0,
            156.0, 27.0, 72.0, 117.0, 162.0, 36.0, 81.0, 126.0, 171.0, 42.0,
            87.0, 132.0, 177.0, 3.0, 48.0, 93.0, 138.0, 9.0, 54.0, 99.0, 144.0,
            18.0, 63.0, 108.0, 153.0, 24.0, 69.0, 114.0, 159.0, 33.0, 78.0,
            123.0, 168.0, 39.0, 84.0, 129.0,
            174.0,  // 60 angles
            46.0, 136.0, 91.0, 1.0, 16.0, 31.0, 61.0, 76.0, 106.0, 121.0, 151.0,
            166.0, 7.0, 52.0, 97.0, 142.0, 13.0, 58.0, 103.0, 148.0, 22.0, 67.0,
            112.0, 157.0, 28.0, 73.0, 118.0, 163.0, 37.0, 82.0, 127.0, 172.0,
            43.0, 88.0, 133.0, 178.0, 4.0, 49.0, 94.0, 139.0, 10.0, 55.0, 100.0,
            145.0, 19.0, 64.0, 109.0, 154.0, 25.0, 70.0, 115.0, 160.0, 34.0,
            79.0, 124.0, 169.0, 40.0, 85.0, 130.0, 175.0, 47.0, 137.0, 92.0,
            2.0, 17.0, 32.0, 62.0, 77.0, 107.0, 122.0, 152.0, 167.0, 8.0, 53.0,
            98.0, 143.0, 14.0, 59.0, 104.0, 149.0, 23.0, 68.0, 113.0, 158.0,
            29.0, 74.0, 119.0, 164.0, 38.0, 83.0, 128.0, 173.0, 44.0, 89.0,
            134.0, 179.0, 5.0, 50.0, 95.0, 140.0, 11.0, 56.0, 101.0, 146.0,
            20.0, 65.0, 110.0, 155.0, 26.0, 71.0, 116.0, 161.0, 35.0, 80.0,
            125.0, 170.0, 41.0, 86.0, 131.0, 176.0 }; // 180 angles

    // Initialize TUV to form  (x - (a + jb)) * (x - (a - jb)) =  x^2 - 2ax + a^2 + b^2
    // We init TUV for complex roots, except at angle 0, where we use real roots at +/- X
    if (AngleNumber == 2) // At 0 degrees we int to 2 real roots at +/- X.
            {
        TUV[0] = 1.0;   // t
        TUV[1] = 0.0;   // u
        TUV[2] = -(X * X);  // v
    } else  // We init to a complex root at  a +/- jb
    {
        Theta = Angle[AngleNumber] / 180.0 * M_PI;
        a = X * cosl(Theta);
        //b = X * sinl(Theta);
        TUV[0] = 1.0;
        TUV[1] = -2.0 * a;
        TUV[2] = X * X;   // = a*a + b*b because cos^2 + sin^2 = 1
    }

    // The code below initializes the K polys used by RealIterate and QuadIterate.

    // Initialize the K poly used in RealIterate to P'.
    DerivOfP(P, N, RealK);
    RealK[N] = 0.0;

    // Initialize the K poly used in QuadIterate. Initializing QuadK to P" works virtually
    // 100% of the time, but if P51 stalls on a difficult root, these alternate inits give
    // us a way to work out of the stall. All these inits work almost as well as P".
    if (TypeOfQuadK == 0)  // Init QuadK 2nd derivative of P
            {
        long double *Temp = new (std::nothrow) long double[N + 2];
        if (Temp == 0) {
            //ShowMessage("Memory not Allocated in PFiftyOne SetTUVandK.");
            return;
        }

        DerivOfP(P, N, Temp);
        DerivOfP(Temp, N - 1, QuadK);
        QuadK[N] = QuadK[N - 1] = 0.0;
        delete[] Temp;
    }

    else if (TypeOfQuadK == 1) // Set QuadK to QP, because K = QP at convergence.
            {
        QuadSynDiv(P, N, TUV, QuadK);
        QuadK[N] = QuadK[N - 1] = 0.0;
    }

    else if (TypeOfQuadK == 2) // Set QuadK to the 1st derivative of P
            {
        for (j = 0; j <= N - 2; j++)
            QuadK[j] = RealK[j + 1];
        QuadK[N] = QuadK[N - 1] = 0.0;
    }

    else  // Set QuadK to zero, except QuadK[0].
    {
        for (j = 1; j <= N; j++)
            QuadK[j] = 0.0;
        QuadK[0] = 1.0;
    }

    if (QuadK[0] == 0.0)
        QuadK[0] = 1.0; // This can happen if TypeOfQuadK == 2 and P[1] == 0.0
    for (j = N - 2; j > 0; j--)
        QuadK[j] /= QuadK[0];
    QuadK[0] = 1.0;
}

//---------------------------------------------------------------------------

} // namespace
