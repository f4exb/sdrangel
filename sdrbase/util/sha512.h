/**
 * @file sha512.hh
 * @author Stefan Wilhelm (stfwi)
 * @ccflags
 * @ldflags
 * @platform linux, bsd, windows
 * @standard >= c++98
 *
 * SHA512 calculation class template.
 *
 * -------------------------------------------------------------------------------------
 * +++ BSD license header +++
 * Copyright (c) 2010, 2012, Stefan Wilhelm (stfwi, <cerbero s@atwilly s.de>)
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met: (1) Redistributions
 * of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer. (2) Redistributions in binary form must reproduce
 * the above copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the distribution.
 * (3) Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without specific
 * prior written permission. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * -------------------------------------------------------------------------------------
 * 01-2013: (stfwi) Class to template class, reformatting
 */
#ifndef SHA512_HH
#define	SHA512_HH

#if defined(OS_WIN) || defined (_WINDOWS_) || defined(_WIN32) || defined(__MSC_VER)
#include <inttypes.h>
#else
#include <stdint.h>
#endif
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>

namespace sw { namespace detail {

/**
 * @class basic_sha512
 * @template
 */
template <typename Char_Type=char>
class basic_sha512
{
public:

  /**
   * Types
   */
  typedef std::basic_string<Char_Type> str_t;

public:

  /**
   * Constructor
   */
  basic_sha512()
  { clear(); }

  /**
   * Destructor
   */
  ~basic_sha512()
  { ; }

public:

  /**
   * Clear/reset all internal buffers and states.
   */
  void clear()
  {
    sum_[0] = 0x6a09e667f3bcc908; sum_[1] = 0xbb67ae8584caa73b;
    sum_[2] = 0x3c6ef372fe94f82b; sum_[3] = 0xa54ff53a5f1d36f1;
    sum_[4] = 0x510e527fade682d1; sum_[5] = 0x9b05688c2b3e6c1f;
    sum_[6] = 0x1f83d9abfb41bd6b; sum_[7] = 0x5be0cd19137e2179;
    sz_ = 0; iterations_ = 0; memset(&block_, 0, sizeof(block_));
  }

  /**
   * Push new binary data into the internal buf_ and recalculate the checksum.
   * @param const void* data
   * @param size_t size
   */
  void update(const void* data, size_t size)
  {
    unsigned nb, n, n_tail;
    const uint8_t *p;
    n = 128 - sz_;
    n_tail = size < n ? size : n;
    memcpy(&block_[sz_], data, n_tail);
    if (sz_ + size < 128) { sz_ += size; return; }
    n = size - n_tail;
    nb = n >> 7;
    p = (const uint8_t*) data + n_tail;
    transform(block_, 1);
    transform(p, nb);
    n_tail = n & 0x7f;
    memcpy(block_, &p[nb << 7], n_tail);
    sz_ = n_tail;
    iterations_ += (nb+1) << 7;
  }

  /**
   * Finanlise checksum, return hex string.
   * @return str_t
   */
  str_t final_data()
  {
    #if (defined (BYTE_ORDER)) && (defined (BIG_ENDIAN)) && ((BYTE_ORDER == BIG_ENDIAN))
    #define U32_B(x,b) *((b)+0)=(uint8_t)((x)); *((b)+1)=(uint8_t)((x)>>8); \
            *((b)+2)=(uint8_t)((x)>>16); *((b)+3)=(uint8_t)((x)>>24);
    #else
    #define U32_B(x,b) *((b)+3)=(uint8_t)((x)); *((b)+2)=(uint8_t)((x)>>8); \
            *((b)+1)=(uint8_t)((x)>>16); *((b)+0)=(uint8_t)((x)>>24);
    #endif
    unsigned nb, n;
    uint64_t n_total;
    nb = 1 + ((0x80-17) < (sz_ & 0x7f));
    n_total = (iterations_ + sz_) << 3;
    n = nb << 7;
    memset(block_ + sz_, 0, n - sz_);
    block_[sz_] = 0x80;
    U32_B(n_total, block_ + n-4);
    transform(block_, nb);
    std::basic_stringstream<Char_Type> ss; // hex string
    for (unsigned i = 0; i < 8; ++i) {
      ss << std::hex << std::setfill('0') << std::setw(16) << (sum_[i]);
    }
    clear();
    return ss.str();
    #undef U32_B
  }

public:

  /**
   * Calculates the SHA256 for a given string.
   * @param const str_t & s
   * @return str_t
   */
  static str_t calculate(const str_t & s)
  {
    basic_sha512 r;
    r.update(s.data(), s.length());
    return r.final_data();
  }

  /**
   * Calculates the SHA256 for a given C-string.
   * @param const char* s
   * @return str_t
   */
  static str_t calculate(const void* data, size_t size)
  { basic_sha512 r; r.update(data, size); return r.final_data(); }

  /**
   * Calculates the SHA256 for a stream. Returns an empty string on error.
   * @param std::istream & is
   * @return str_t
   */
  static str_t calculate(std::istream & is)
  {
    basic_sha512 r;
    char data[64];
    while(is.good() && is.read(data, sizeof(data)).good()) {
      r.update(data, sizeof(data));
    }
    if(!is.eof()) return str_t();
    if(is.gcount()) r.update(data, is.gcount());
    return r.final_data();
  }

  /**
   * Calculates the SHA256 checksum for a given file, either read binary or as text.
   * @param const str_t & path
   * @param bool binary = true
   * @return str_t
   */
  static str_t file(const str_t & path, bool binary=true)
  {
    std::ifstream fs;
    fs.open(path.c_str(), binary ? (std::ios::in|std::ios::binary) : (std::ios::in));
    str_t s = calculate(fs);
    fs.close();
    return s;
  }

private:

  /**
   * Performs the SHA256 transformation on a given block
   * @param uint32_t *block
   */
  void transform(const uint8_t *data, size_t size)
  {
    #define SR(x, n) (x >> n)
    #define RR(x, n) ((x >> n) | (x << ((sizeof(x) << 3) - n)))
    #define RL(x, n) ((x << n) | (x >> ((sizeof(x) << 3) - n)))
    #define CH(x, y, z)  ((x & y) ^ (~x & z))
    #define MJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
    #define F1(x) (RR(x, 28) ^ RR(x, 34) ^ RR(x, 39))
    #define F2(x) (RR(x, 14) ^ RR(x, 18) ^ RR(x, 41))
    #define F3(x) (RR(x,  1) ^ RR(x,  8) ^ SR(x,  7))
    #define F4(x) (RR(x, 19) ^ RR(x, 61) ^ SR(x,  6))
    #if (defined (BYTE_ORDER)) && (defined (BIG_ENDIAN)) && ((BYTE_ORDER == BIG_ENDIAN))
    #define B_U64(b,x) *(x)=((uint64_t)*((b)+0))|((uint64_t)*((b)+1)<<8)|\
      ((uint64_t)*((b)+2)<<16)|((uint64_t)*((b)+3)<<24)|((uint64_t)*((b)+4)<<32)|\
      ((uint64_t)*((b)+5)<<40)|((uint64_t)*((b)+6)<<48)|((uint64_t)*((b)+7)<<56);
    #else
    #define B_U64(b,x) *(x)=((uint64_t)*((b)+7))|((uint64_t)*((b)+6)<<8)|\
      ((uint64_t)*((b)+5)<<16)|((uint64_t)*((b)+4)<<24)|((uint64_t)*((b)+3)<<32)|\
      ((uint64_t)*((b)+2)<<40)|((uint64_t)*((b)+1)<<48)|((uint64_t)*((b)+0)<<56);
    #endif
    uint64_t t, u, v[8], w[80];
    const uint8_t *tblock;
    unsigned j;
    for(unsigned i = 0; i < size; ++i) {
      tblock = data + (i << 7);
      for(j = 0; j < 16; ++j) B_U64(&tblock[j<<3], &w[j]);
      for(j = 16; j < 80; ++j) w[j] = F4(w[j-2]) + w[j-7] + F3(w[j-15]) + w[j-16];
      for(j = 0; j < 8; ++j) v[j] = sum_[j];
      for(j = 0; j < 80; ++j) {
        t = v[7] + F2(v[4]) + CH(v[4], v[5], v[6]) + lut_[j] + w[j];
        u = F1(v[0]) + MJ(v[0], v[1], v[2]); v[7] = v[6]; v[6] = v[5]; v[5] = v[4];
        v[4] = v[3] + t; v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t + u;
      }
      for(j = 0; j < 8; ++j) sum_[j] += v[j];
    }
    #undef SR
    #undef RR
    #undef RL
    #undef CH
    #undef MJ
    #undef F1
    #undef F2
    #undef F3
    #undef F4
    #undef B_U64
  }

private:

  uint64_t iterations_; // Number of iterations
  uint64_t sum_[8];     // Intermediate checksum buffer
  unsigned sz_;         // Number of currently stored bytes in the block
  uint8_t  block_[256];
  static const uint64_t lut_[80]; // Lookup table
};

template <typename CT>
const uint64_t basic_sha512<CT>::lut_[80] = {
  0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
  0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
  0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
  0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
  0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
  0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
  0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
  0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
  0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
  0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
  0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
  0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
  0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
  0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
  0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
  0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
  0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
  0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
  0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
  0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

}}

namespace sw {
  typedef detail::basic_sha512<> sha512;
}

#endif