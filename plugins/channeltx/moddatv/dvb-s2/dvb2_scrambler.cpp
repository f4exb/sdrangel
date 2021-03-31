#include "DVB2.h"

void DVB2::init_bb_randomiser(void)
{
    int sr = 0x4A80;
    for( int i = 0; i < FRAME_SIZE_NORMAL; i++ )
    {
        int b = ((sr)^(sr>>1))&1;
        m_bb_randomise[i] = b;
        sr >>= 1;
        if( b ) sr |= 0x4000;
    }
}
//
// Randomise the data bits
//
void DVB2::bb_randomise(void)
{
    for( int i = 0; i < m_format[0].kbch; i++ )
   {
        m_frame[i] ^= m_bb_randomise[i];
    }
}
