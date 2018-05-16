//---------------------------------------------------------------------------

#ifndef LowPassRootsH
#define LowPassRootsH

#include <complex>
#define MAX_ELLIP_ITER 15
#define ELLIPARRAYSIZE 20  // needs to be > 10 and >= Max Num Poles + 1

namespace kitiirfir
{

void ReverseCoeff(double *P, int N);
int ButterworthPoly(int NumPoles, std::complex<double> *Roots);
int GaussianPoly(int NumPoles, std::complex<double> *Roots);
int AdjustablePoly(int NumPoles, std::complex<double> *Roots, double Gamma);
int ChebyshevPoly(int NumPoles, double Ripple, std::complex<double> *Roots);
int BesselPoly(int NumPoles, std::complex<double> *Roots);
int InvChebyPoly(int NumPoles, double StopBanddB, std::complex<double> *ChebyPoles, std::complex<double> *ChebyZeros, int *ZeroCount);
int PapoulisPoly(int NumPoles, std::complex<double> *Roots);
int EllipticPoly(int FiltOrder, double Ripple, double DesiredSBdB, std::complex<double> *EllipPoles, std::complex<double> *EllipZeros, int *ZeroCount);

} // namespace

#endif


