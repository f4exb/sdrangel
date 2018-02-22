#ifndef LEANSDR_FILTERGEN_H
#define LEANSDR_FILTERGEN_H

#include <math.h>

namespace leansdr {
  namespace filtergen {
  
    template<typename T>
    void normalize_power(int n, T *coeffs, float gain=1) {
      float s2 = 0;
      for ( int i=0; i<n; ++i ) s2 = s2 + coeffs[i]*coeffs[i];  // TBD complex
      if ( s2 ) gain /= gen_sqrt(s2);
      for ( int i=0; i<n; ++i ) coeffs[i] = coeffs[i] * gain;
    }

    template<typename T>
    void normalize_dcgain(int n, T *coeffs, float gain=1) {
      float s = 0;
      for ( int i=0; i<n; ++i ) s = s + coeffs[i];
      if ( s ) gain /= s;
      for ( int i=0; i<n; ++i ) coeffs[i] = coeffs[i] * gain;
    }

    // Generate coefficients for a sinc filter.
    // https://en.wikipedia.org/wiki/Sinc_filter

    template<typename T>
    int lowpass(int order, float Fcut, T **coeffs, float gain=1) {
      int ncoeffs = order + 1;
      *coeffs = new T[ncoeffs];
      for ( int i=0; i<ncoeffs; ++i ) {
	float t = i - (ncoeffs-1)*0.5;
	float sinc = 2*Fcut * (t ? sin(2*M_PI*Fcut*t)/(2*M_PI*Fcut*t) : 1);
#if 0  // Hamming 
	float alpha = 25.0/46, beta = 21.0/46;
	float window = alpha - beta*cos(2*M_PI*i/order);
#else
	float window = 1;
#endif
	(*coeffs)[i] = sinc * window;
      }
      normalize_dcgain(ncoeffs, *coeffs, gain);
      return ncoeffs;
    }

    
    // Generate coefficients for a RRC filter.
    // https://en.wikipedia.org/wiki/Root-raised-cosine_filter

    template<typename T>
    int root_raised_cosine(int order, float Fs, float rolloff, T **coeffs) {
      float B = rolloff, pi = M_PI;
      int ncoeffs = (order+1) | 1;
      *coeffs = new T[ncoeffs];
      for ( int i=0; i<ncoeffs; ++i ) {
	int t = i - ncoeffs/2;
	float c;
	if ( t == 0 )
	  c = sqrt(Fs) * (1-B+4*B/pi);
	else {
	  float tT = t * Fs;
	  float den = pi*tT*(1-(4*B*tT)*(4*B*tT));
	  if ( ! den )
	    c = B*sqrt(Fs/2) * ( (1+2/pi)*sin(pi/(4*B)) +
				 (1-2/pi)*cos(pi/(4*B)) );
	  else
	    c = sqrt(Fs) * ( sin(pi*tT*(1-B)) +
			     4*B*tT*cos(pi*tT*(1+B)) ) / den;
	}
	(*coeffs)[i] = c;
      }
      normalize_dcgain(ncoeffs, *coeffs);
      return ncoeffs;
    }


    // Dump filter coefficients for matlab/octave

    inline void dump_filter(const char *name, int ncoeffs, float *coeffs) {
      fprintf(stderr, "%s = [", name);
      for ( int i=0; i<ncoeffs; ++i )
	fprintf(stderr, "%s %f", (i?",":""), coeffs[i]);
      fprintf(stderr, " ];\n");
    }

  }  // namespace
}  // namespace

#endif  // LEANSDR_FILTERGEN_H
