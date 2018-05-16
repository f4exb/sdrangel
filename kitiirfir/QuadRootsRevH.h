//---------------------------------------------------------------------------

#ifndef QuadRootsRevHH
#define QuadRootsRevHH
//---------------------------------------------------------------------------

#define LDBL_EPSILON 1.084202172485504434E-19L
// #define M_SQRT3 1.7320508075688772935274463L   // sqrt(3)
#define M_SQRT3_2 0.8660254037844386467637231L   // sqrt(3)/2
// #define DBL_EPSILON  2.2204460492503131E-16    // 2^-52  typically defined in the compiler's float.h
#define ZERO_PLUS   8.88178419700125232E-16      // 2^-50 = 4*DBL_EPSILON
#define ZERO_MINUS -8.88178419700125232E-16
#define TINY_VALUE  1.0E-30                      // This is what we use to test for zero. Usually to avoid divide by zero.

namespace kitiirfir
{

void QuadRoots(long double *P, long double *RealPart, long double *ImagPart);
void CubicRoots(long double *P, long double *RealPart, long double *ImagPart);
void BiQuadRoots(long double *P, long double *RealPart, long double *ImagPart);
void ReversePoly(long double *P, int N);
void InvertRoots(int N, long double *RealRoot, long double *ImagRoot);

} // namespace

#endif

