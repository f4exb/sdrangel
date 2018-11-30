#ifndef LEANSDR_CONVOLUTIONAL_H
#define LEANSDR_CONVOLUTIONAL_H

namespace leansdr
{

// ALGEBRAIC DECONVOLUTION

// QPSK 1/2 only.
// This is a straightforward implementation, provided for reference.
// deconvol_poly2 is functionally equivalent and much faster.

template<typename Tin,         // Input IQ symbols
        typename Thist,       // Input shift register (IQIQIQ...)
        Thist POLY_DECONVOL,  // Taps (IQIQIQ...)
        Thist POLY_ERRORS>
struct deconvol_poly
{
    typedef u8 hardsymbol;

    // Support soft of float input
    inline u8 SYMVAL(const hardsymbol *s)
    {
        return *s;
    }
    inline u8 SYMVAL(const softsymbol *s)
    {
        return s->symbol;
    }

    typedef u8 decoded_byte;

    deconvol_poly() :
            hist(0)
    {
    }

    // Remap and deconvolve [nb*8] symbols into [nb] bytes.
    // Return estimated number of bit errors.

    int run(const Tin *pin, const u8 remap[], decoded_byte *pout, int nb)
    {
        int nerrors = 0;
        int halfway = nb / 2;
        for (; nb--; ++pout)
        {
            decoded_byte byte = 0;
            for (int bit = 8; bit--; ++pin)
            {
                hist = (hist << 2) | remap[SYMVAL(pin)];
                byte = (byte << 1) | parity(hist & POLY_DECONVOL);
                if (nb < halfway)
                    nerrors += parity(hist & POLY_ERRORS);
            }
            *pout = byte;
        }
        return nerrors;
    }

private:
    Thist hist;

};
// deconvol_poly

// ALGEBRAIC DECONVOLUTION, OPTIMIZED

// QPSK 1/2 only.
// Functionally equivalent to deconvol_poly,
// but processing 32 bits in parallel.

template<typename Tin,    // Input IQ symbols
        typename Thist,  // Input shift registers (one for I, one for Q)
        typename Tpoly,  // Taps (interleaved IQIQIQ...)
        Tpoly POLY_DECONVOL, Tpoly POLY_ERRORS>
struct deconvol_poly2
{
    typedef u8 hardsymbol;

    // Support instanciation of template with soft of float input
    inline u8 SYMVAL(const hardsymbol *s)
    {
        return *s;
    }
    inline u8 SYMVAL(const softsymbol *s)
    {
        return s->symbol;
    }

    typedef u8 decoded_byte;

    deconvol_poly2() :
            inI(0), inQ(0)
    {
    }

    // Remap and deconvolve [nb*8] symbols into [nb] bytes.
    // Return estimated number of bit errors or -1 if error.

    int run(const Tin *pin, const u8 remap[], decoded_byte *pout, int nb)
    {
        if (nb & (sizeof(Thist) - 1)) {
            fail("deconvol_poly2::run", "Must deconvolve sizeof(Thist) bytes at a time");
            return -1;
        }
        nb /= sizeof(Thist);
        unsigned long nerrors = 0;
        int halfway = nb / 2;
        Thist histI = inI, histQ = inQ;
        for (; nb--;)
        {
            // This is where we convolve bits in parallel.
            Thist wd = 0;  // decoded bits
            Thist we = 0;  // error bits (should be 0)
#if 0
            // Trust gcc to unroll and evaluate the bit tests at compile-time.
            for ( int bit=sizeof(Thist)*8; bit--; ++pin )
            {
                u8 iq = remap[SYMVAL(pin)];
                histI = (histI<<1) | (iq>>1);
                histQ = (histQ<<1) | (iq&1);
                if ( POLY_DECONVOL & ((Tpoly)2<<(2*bit)) ) wd ^= histI;
                if ( POLY_DECONVOL & ((Tpoly)1<<(2*bit)) ) wd ^= histQ;
                if ( POLY_ERRORS & ((Tpoly)2<<(2*bit)) ) we ^= histI;
                if ( POLY_ERRORS & ((Tpoly)1<<(2*bit)) ) we ^= histQ;
            }
#else
            // Unroll manually.
#define LOOP(bit) {						   \
	  u8 iq = remap[SYMVAL(pin)];				   \
	  histI = (histI<<1) | (iq>>1);				   \
	  histQ = (histQ<<1) | (iq&1);				   \
	  if ( POLY_DECONVOL & ((Tpoly)2<<(2*bit)) ) wd ^= histI;  \
	  if ( POLY_DECONVOL & ((Tpoly)1<<(2*bit)) ) wd ^= histQ;  \
	  if ( POLY_ERRORS   & ((Tpoly)2<<(2*bit)) ) we ^= histI;  \
	  if ( POLY_ERRORS   & ((Tpoly)1<<(2*bit)) ) we ^= histQ;  \
	  ++pin;						   \
	}
            // Don't shift by more than the operand width
            switch (sizeof(Thist) /* 8*/)
            {
#if 0  // Not needed yet - avoid compiler warnings
            case 8:
            LOOP(63); LOOP(62); LOOP(61); LOOP(60);
            LOOP(59); LOOP(58); LOOP(57); LOOP(56);
            LOOP(55); LOOP(54); LOOP(53); LOOP(52);
            LOOP(51); LOOP(50); LOOP(49); LOOP(48);
            LOOP(47); LOOP(46); LOOP(45); LOOP(44);
            LOOP(43); LOOP(42); LOOP(41); LOOP(40);
            LOOP(39); LOOP(38); LOOP(37); LOOP(36);
            LOOP(35); LOOP(34); LOOP(33); LOOP(32);
            // Fall-through
#endif
            case 4:
                LOOP(31)
                ;
                LOOP(30)
                ;
                LOOP(29)
                ;
                LOOP(28)
                ;
                LOOP(27)
                ;
                LOOP(26)
                ;
                LOOP(25)
                ;
                LOOP(24)
                ;
                LOOP(23)
                ;
                LOOP(22)
                ;
                LOOP(21)
                ;
                LOOP(20)
                ;
                LOOP(19)
                ;
                LOOP(18)
                ;
                LOOP(17)
                ;
                LOOP(16)
                ;
                // Fall-through
            case 2:
                LOOP(15)
                ;
                LOOP(14)
                ;
                LOOP(13)
                ;
                LOOP(12)
                ;
                LOOP(11)
                ;
                LOOP(10)
                ;
                LOOP(9)
                ;
                LOOP(8)
                ;
                // Fall-through
            case 1:
                LOOP(7)
                ;
                LOOP(6)
                ;
                LOOP(5)
                ;
                LOOP(4)
                ;
                LOOP(3)
                ;
                LOOP(2)
                ;
                LOOP(1)
                ;
                LOOP(0)
                ;
                break;
            default:
                fail("deconvol_poly2::run", "Thist not supported (1)");
                return -1;
            }
#undef LOOP
#endif
            switch (sizeof(Thist) /* 8*/)
            {
#if 0  // Not needed yet - avoid compiler warnings
            case 8:
            *pout++ = wd >> 56;
            *pout++ = wd >> 48;
            *pout++ = wd >> 40;
            *pout++ = wd >> 32;
            // Fall-through
#endif
            case 4:
                *pout++ = wd >> 24;
                *pout++ = wd >> 16;
                // Fall-through
            case 2:
                *pout++ = wd >> 8;
                // Fall-through
            case 1:
                *pout++ = wd;
                break;
            default:
                fail("deconvol_poly2::run", "Thist not supported (2)");
                return -1;
            }
            // Count errors when the shift registers are full
            if (nb < halfway)
                nerrors += hamming_weight(we);
        }
        inI = histI;
        inQ = histQ;
        return nerrors;
    }
private:
    Thist inI, inQ;
};
// deconvol_poly2

// CONVOLUTIONAL ENCODER

// QPSK 1/2 only.

template<typename Thist, uint64_t POLY1, uint64_t POLY2>
struct convol_poly2
{
    typedef u8 uncoded_byte;
    typedef u8 hardsymbol;

    convol_poly2() :
            hist(0)
    {
    }

    // Convolve [count] bytes into [count*8] symbols, and remap.

    void run(const uncoded_byte *pin, const u8 remap[], hardsymbol *pout, int count)
    {
        for (; count--; ++pin)
        {
            uncoded_byte b = *pin;
            for (int bit = 8; bit--; ++pout)
            {
                hist = (hist >> 1) | (((b >> bit) & 1) << 6);
                u8 s = (parity(hist & POLY1) << 1) | parity(hist & POLY2);
                *pout = remap[s];
            }
        }
    }
private:
    Thist hist;
};
// convol_poly2

// Generic BPSK..256QAM and puncturing

template<typename Thist, int HISTSIZE>
struct convol_multipoly
{
    typedef u8 uncoded_byte;
    typedef u8 hardsymbol;
    int bits_in, bits_out, bps;
    const Thist *polys;    // [bits_out]

    convol_multipoly() :
            bits_in(0), bits_out(0), bps(0), polys(0), hist(0), nhist(0), sersymb(0), nsersymb(0)
    {
    }

    void encode(const uncoded_byte *pin, hardsymbol *pout, int count)
    {
        if (!bits_in || !bits_out || !bps)
        {
            fatal("leansdr::convol_multipoly::encode: convol_multipoly not configured");
            return;
        }
        hardsymbol symbmask = (1 << bps) - 1;
        for (; count--; ++pin)
        {
            uncoded_byte b = *pin;
            for (int bit = 8; bit--;)
            {
                hist = (hist >> 1) | ((Thist) ((b >> bit) & 1) << (HISTSIZE - 1));
                ++nhist;
                if (nhist == bits_in)
                {
                    for (int p = 0; p < bits_out; ++p)
                    {
                        int b = parity((Thist) (hist & polys[p]));
                        sersymb = (sersymb << 1) | b;
                    }
                    nhist = 0;
                    nsersymb += bits_out;
                    while (nsersymb >= bps)
                    {
                        hardsymbol s = (sersymb >> (nsersymb - bps)) & symbmask;
                        *pout++ = s;
                        nsersymb -= bps;
                    }
                }
            }
        }
        // Ensure deterministic output size
        // TBD We can relax this
        if (nhist || nsersymb) {
            fatal("leansdr::convol_multipoly::encode: partial run");
        }
    }
private:
    Thist hist;
    int nhist;
    Thist sersymb;
    int nsersymb;
};
// convol_multipoly

}// namespace

#endif  // LEANSDR_CONVOLUTIONAL_H
