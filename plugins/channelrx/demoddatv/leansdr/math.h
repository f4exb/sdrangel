#ifndef LEANSDR_MATH_H
#define LEANSDR_MATH_H

#include <math.h>
#include <stdint.h>

namespace leansdr {

  template<typename T>
  struct complex {
    T re, im;
    complex() : re(0), im(0) { }
    complex(T x) : re(x), im(0) { }
    complex(T x, T y) : re(x), im(y) { }
    inline void operator +=(const complex<T> &x) { re+=x.re; im+=x.im; }
  };

  template<typename T>
  complex<T> operator +(const complex<T> &a, const complex<T> &b) {
    return complex<T>(a.re+b.re, a.im+b.im);
  }

  template<typename T>
  complex<T> operator *(const complex<T> &a, const complex<T> &b) {
    return complex<T>(a.re*b.re-a.im*b.im, a.re*b.im+a.im*b.re);
  }

  template<typename T>
  complex<T> operator *(const complex<T> &a, const T &k) {
    return complex<T>(a.re*k, a.im*k);
  }

  template<typename T>
  complex<T> operator *(const T &k, const complex<T> &a) {
    return complex<T>(k*a.re, k*a.im);
  }

  // TBD Optimize with dedicated instructions
  inline int hamming_weight(uint8_t x) {
    static const int lut[16] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4 };
    return lut[x&15] + lut[x>>4];
  }
  inline int hamming_weight(uint16_t x) {
    return hamming_weight((uint8_t)x)
      +    hamming_weight((uint8_t)(x>>8));
  }
  inline int hamming_weight(uint32_t x) {
    return hamming_weight((uint16_t)x)
      +    hamming_weight((uint16_t)(x>>16));
  }
  inline int hamming_weight(uint64_t x) {
    return hamming_weight((uint32_t)x)
      +    hamming_weight((uint32_t)(x>>32));
  }

  inline unsigned char parity(uint8_t x) {
    x ^= x>>4;
    return (0x6996 >> (x&15)) & 1;  // 16-entry look-up table
  }
  inline unsigned char parity(uint16_t x) {
    return parity((uint8_t)(x^(x>>8)));
  }
  inline unsigned char parity(uint32_t x) {
    return parity((uint16_t)(x^(x>>16)));
  }
  inline unsigned char parity(uint64_t x) {
    return parity((uint32_t)(x^(x>>32)));
  }

  inline int log2i(uint64_t x) {
    int n = -1;
    for ( ; x; ++n,x>>=1 ) ;
    return n;
  }

  // Pre-computed sin/cos for 16-bit angles

  struct trig16 {
    complex<float> lut[65536];  // TBD static and shared
    trig16() {
      for ( int a=0; a<65536; ++a ) {
	float af = a * 2*M_PI / 65536;
	lut[a].re = cosf(af);
	lut[a].im = sinf(af);
      }
    }
    inline const complex<float> &expi(uint16_t a) const {
      return lut[a];
    }
    // a must fit in a int32_t, otherwise behaviour is undefined
    inline const complex<float> &expi(float a) const {
      return expi((uint16_t)(int16_t)(int32_t)a);
    }
  };

}  // namespace

#endif  // LEANSDR_MATH_H
