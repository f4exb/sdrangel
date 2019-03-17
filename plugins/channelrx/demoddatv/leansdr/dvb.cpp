#include "dvb.h"

namespace leansdr
{

deconvol_sync_simple *make_deconvol_sync_simple(scheduler *sch,
                                                pipebuf<eucl_ss> &_in,
                                                pipebuf<u8> &_out,
                                                enum code_rate rate)
{
    // EN 300 421, section 4.4.3 Inner coding
    uint32_t pX, pY;
    switch (rate)
    {
    case FEC12:
        pX = 0x1; // 1
        pY = 0x1; // 1
        break;
    case FEC23:
    case FEC46:
        pX = 0xa; // 1010  (Handle as FEC4/6, no half-symbols)
        pY = 0xf; // 1111
        break;
    case FEC34:
        pX = 0x5; // 101
        pY = 0x6; // 110
        break;
    case FEC56:
        pX = 0x15; // 10101
        pY = 0x1a; // 11010
        break;
    case FEC78:
        pX = 0x45; // 1000101
        pY = 0x7a; // 1111010
        break;
    default:
        //fail("Code rate not implemented");
        // For testing DVB-S2 constellations.
        fprintf(stderr, "Code rate not implemented; proceeding anyway\n");
        pX = pY = 1;
    }

    return new deconvol_sync_simple(sch, _in, _out, DVBS_G1, DVBS_G2, pX, pY);
}

} // leansdr