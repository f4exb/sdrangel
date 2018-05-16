//---------------------------------------------------------------------------

#ifndef PFiftyOneRevEH
#define PFiftyOneRevEH
//---------------------------------------------------------------------------

#include <complex>

// TUpdateStatus represent the status of the UpdateTUV function.
// The first 3 are returned from UpdateTUV. The last two are sent to UpdateTUV.
enum TUpdateStatus {UPDATED, BAD_ANGLE, ZERO_DEL, DAMPER_ON, DAMPER_OFF};


// This value for LDBL_EPSILON is for an 80 bit variable (64 bit significand). It should be in float.h
#define LDBL_EPSILON 1.084202172485504434E-19L    // = 2^-63
#define P51_MAXDEGREE   100       // The max poly order allowed. Used at the top of P51. This was set arbitrarily.
#define P51_ARRAY_SIZE  102       // P51 uses the new operator. P51 arrays must be MaxDegree + 2
#define MAX_NUM_ANGLES  180       // The number of defined angles for initializing TUV.
#define REAL_ITER_MAX   20        // Max number of iterations in RealIterate.
#define QUAD_ITER_MAX   20        // Max number of iterations in QuadIterate.
#define MAX_NUM_K       4         // This is the number of methods we have to define K.
#define MAX_NEWT_ITERS  10        // Used to limit the Newton Iterations in the GetX function.
#define TINY_VALUE  1.0E-30       // This is what we use to test for zero. Usually to avoid divide by zero.
#define HUGE_VALUE  1.0E200       // This gets used to test for imminent overflow.

namespace kitiirfir
{

int FindRoots(int N, double *Coeff, std::complex<double> *Roots);
int PFiftyOne(long double *Coeff, int Degree, long double *RealRoot, long double *ImagRoot);
int QuadIterate(int Iter, long double *P, long double *QP, long double *K, long double *QK, int N, long double *TUV, TUpdateStatus *UpdateStatus);
void UpdateTUV(int Iter, long double *P, int N, long double *QP, long double *K, long double *QK, long double *TUV, TUpdateStatus *UpdateStatus);
int RealIterate(int P51_Iter, long double *P, long double *QP, long double *K, long double *QK, int N, long double *RealZero);
void QuadraticFormula(long double *TUV, long double *RealRoot, long double *ImagRoot);
void QuadSynDiv(long double *P, int N, long double *TUV, long double *Q);
void DerivOfP(long double *P, int N, long double *dP);
void SetTUVandK(long double *P, int N, long double *TUV, long double *RealK, long double *QuadK, long double X, int AngleNumber, int TypeOfQuadK);

} // namespace

#endif


