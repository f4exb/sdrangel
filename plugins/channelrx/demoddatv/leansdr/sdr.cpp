#include "sdr.h"

namespace leansdr
{

const char *cstln_base::names[] =
    {
        "BPSK",
        "QPSK",
        "8PSK",
        "16APSK",
        "32APSK",
        "64APSKe",
        "16QAM",
        "64QAM",
        "256QAM"
    };


void softsymb_harden(llr_ss *ss)
{
    for (int b = 0; b < 8; ++b) {
        ss->bits[b] = (ss->bits[b] < 0) ? -127 : 127;
    }
}

void softsymb_harden(hard_ss *ss)
{
    (void)ss; // NOP
}

void softsymb_harden(eucl_ss *ss)
{
    for (int s = 0; s < ss->MAX_SYMBOLS; ++s) {
        ss->dists2[s] = (s == ss->nearest) ? 0 : 1;
    }
}


uint8_t softsymb_to_dump(const llr_ss &ss, int bit)
{
    return 128 - ss.bits[bit];
}

uint8_t softsymb_to_dump(const hard_ss &ss, int bit)
{
    return ((ss >> bit) & 1) ? 255 : 0;
}

uint8_t softsymb_to_dump(const eucl_ss &ss, int bit)
{
    (void)bit;
    return ss.dists2[ss.nearest] >> 8;
}


void to_softsymb(const full_ss *fss, hard_ss *ss)
{
    *ss = fss->nearest;
}

void to_softsymb(const full_ss *fss, eucl_ss *ss)
{
    for (int s = 0; s < ss->MAX_SYMBOLS; ++s) {
        ss->dists2[s] = fss->dists2[s];
    }

    uint16_t best = 65535, best2 = 65535;

    for (int s = 0; s < ss->MAX_SYMBOLS; ++s)
    {
        if (fss->dists2[s] < best)
        {
            best2 = best;
            best = fss->dists2[s];
        }
        else if (fss->dists2[s] < best2)
        {
            best2 = fss->dists2[s];
        }
    }

    ss->discr2 = best2 - best;
    ss->nearest = fss->nearest;
}

void to_softsymb(const full_ss *fss, llr_ss *ss)
{
    for (int b = 0; b < 8; ++b)
    {
        float v = (1.0f - fss->p[b]) / (fss->p[b] + 1e-6);
        int r = logf(v) * 5; // TBD Optimal scaling vs saturation ?

        if (r < -127) {
            r = -127;
        }

        if (r > 127) {
            r = 127;
        }

        ss->bits[b] = r;
    }
}

} // leansdr
