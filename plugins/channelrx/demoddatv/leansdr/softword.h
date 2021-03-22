#ifndef LEANSDR_SOFTWORD_H
#define LEANSDR_SOFTWORD_H

namespace leansdr
{

// This file implements options for SOFTBYTE in s2_frame_receiver
// (representation of bytes after deinterleaving).
// Also used as SOFTWORD in ldpc_engine
// (representation of packed SOFTBITs).

// Interface for ldpc_engine:
//   SOFTBIT softword_get(const SOFTWORD &p, int b)
//   SOFTBIT softwords_xor(const SOFTWORD p1[], const SOFTWORD p2[], int b)
//   void softword_zero(SOFTWORD *p, int b)
//   void softwords_set(SOFTWORD p[], int b)
//   void softwords_flip(SOFTWORD p[], int b)

// HARD SOFTWORD / SOFTBYTE

typedef uint8_t hard_sb; // Bit 7 is transmitted first.

inline bool softword_get(const hard_sb &p, int b)
{
    return p & (1 << (7 - b));
}

inline void softword_set(hard_sb *p, int b, bool v)
{
    hard_sb mask = 1 << (7 - b);
    *p = ((*p) & ~mask) | (v << (7 - b));
}

inline void softword_clear(hard_sb *p) { *p = 0; }

inline bool softword_weight(const bool &l)
{
    (void) l;
    return true;
}

inline void softbit_set(bool *p, bool v) { *p = v; }
inline bool softbit_harden(bool b) { return b; }
inline uint8_t softbyte_harden(const hard_sb &b) { return b; }
inline bool softbit_xor(bool x, bool y) { return x ^ y; }
inline void softbit_clear(bool *p) { *p = false; }

inline bool softwords_xor(const hard_sb p1[], const hard_sb p2[], int b)
{
    return (p1[b / 8] ^ p2[b / 8]) & (1 << (7 - (b & 7)));
}

inline void softword_zero(hard_sb *p)
{
    *p = 0;
}

inline void softwords_set(hard_sb p[], int b)
{
    p[b / 8] |= 1 << (7 - (b & 7));
}

inline void softword_write(hard_sb &p, int b, bool v)
{
    hard_sb mask = 1 << (7 - b);
    p = (p & ~mask) | (hard_sb)v << (7 - b);
}

inline void softwords_write(hard_sb p[], int b, bool v)
{
    softword_write(p[b / 8], b & 7, v);
}

inline void softwords_flip(hard_sb p[], int b)
{
    p[b / 8] ^= 1 << (7 - (b & 7));
}

uint8_t *softbytes_harden(hard_sb p[], int nbytes, uint8_t storage[])
{
    (void) nbytes;
    (void) storage;
    return p;
}

// LLR SOFTWORD

struct llr_sb
{
    llr_t bits[8]; // bits[0] is transmitted first.
};

float prob(llr_t l)
{
    return (127.0 + l) / 254;
}

llr_t llr(float p)
{
    int r = -127 + 254 * p;

    if (r < -127) {
        r = -127;
    }

    if (r > 127) {
        r = 127;
    }

    return r;
}

inline llr_t llr_xor(llr_t lx, llr_t ly)
{
    float px = prob(lx), py = prob(ly);
    return llr(px * (1 - py) + (1 - px) * py);
}

inline llr_t softword_get(const llr_sb &p, int b)
{
    return p.bits[b];
}

// Special case to avoid computing b/8*8+b%7. Assumes llr_sb[] packed.
inline llr_t softwords_get(const llr_sb p[], int b)
{
    return p[0].bits[b]; // Beyond bounds on purpose
}

inline void softword_set(llr_sb *p, int b, llr_t v)
{
    p->bits[b] = v;
}

inline void softword_clear(llr_sb *p)
{
    memset(p->bits, 0, sizeof(p->bits));
}

//  inline llr_t softwords_get(const llr_sb p[], int b) {
//    return softword_get(p[b/8], b&7);
//  }

inline llr_t softword_weight(llr_t l) { return abs(l); }
inline void softbit_set(llr_t *p, bool v) { *p = v ? -127 : 127; }
inline bool softbit_harden(llr_t l) { return (l < 0); }

inline uint8_t softbyte_harden(const llr_sb &b)
{
    // Without conditional jumps
    uint8_t r = (((b.bits[0] & 128) >> 0) |
                 ((b.bits[1] & 128) >> 1) |
                 ((b.bits[2] & 128) >> 2) |
                 ((b.bits[3] & 128) >> 3) |
                 ((b.bits[4] & 128) >> 4) |
                 ((b.bits[5] & 128) >> 5) |
                 ((b.bits[6] & 128) >> 6) |
                 ((b.bits[7] & 128) >> 7));
    return r;
}

inline llr_t softbit_xor(llr_t x, llr_t y) { return llr_xor(x, y); }
inline void softbit_clear(llr_t *p) { *p = -127; }

inline llr_t softwords_xor(const llr_sb p1[], const llr_sb p2[], int b)
{
    return llr_xor(p1[b / 8].bits[b & 7], p2[b / 8].bits[b & 7]);
}

inline void softword_zero(llr_sb *p)
{
    memset(p, -127, sizeof(*p));
}

inline void softwords_set(llr_sb p[], int b)
{
    p[b / 8].bits[b & 7] = 127;
}

inline void softword_write(llr_sb &p, int b, llr_t v)
{
    p.bits[b] = v;
}

inline void softwords_write(llr_sb p[], int b, llr_t v)
{
    softword_write(p[b / 8], b & 7, v);
}

inline void softwords_flip(llr_sb p[], int b)
{
    llr_t *l = &p[b / 8].bits[b & 7];
    *l = -*l;
}

uint8_t *softbytes_harden(llr_sb p[], int nbytes, uint8_t storage[])
{
    for (uint8_t *q = storage; nbytes--; ++p, ++q) {
        *q = softbyte_harden(*p);
    }

    return storage;
}

// Generic wrappers

template <typename SOFTBIT, typename SOFTBYTE>
inline SOFTBIT softwords_get(const SOFTBYTE p[], int b)
{
    return softword_get(p[b / 8], b & 7);
}

template <typename SOFTBIT, typename SOFTBYTE>
inline void softwords_set(SOFTBYTE p[], int b, SOFTBIT v)
{
    softword_set(&p[b / 8], b & 7, v);
}

template <typename SOFTBIT, typename SOFTBYTE>
inline SOFTBIT softwords_weight(const SOFTBYTE p[], int b)
{
    return softword_weight(softwords_get<SOFTBIT, SOFTBYTE>(p, b));
}

} // namespace leansdr

#endif // LEANSDR_SOFTWORD_H
