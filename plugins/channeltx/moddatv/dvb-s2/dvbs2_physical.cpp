#include "DVBS2.h"

void DVBS2::b_64_7_code( unsigned char in, int *out )
{
    unsigned long temp,bit;

    temp = 0;

    if(in&0x40) temp ^= g[0];
    if(in&0x20) temp ^= g[1];
    if(in&0x10) temp ^= g[2];
    if(in&0x08) temp ^= g[3];
    if(in&0x04) temp ^= g[4];
    if(in&0x02) temp ^= g[5];

    bit = 0x80000000;
    for( int m = 0; m < 32; m++ )
    {
        out[(m*2)]   = (temp&bit)?1:0;
        out[(m*2)+1] = out[m*2]^(in&0x01);
        bit >>= 1;
    }
    // Randomise it
    for( int m = 0; m < 64; m++ )
    {
        out[m] = out[m] ^ ph_scram_tab[m];
    }
}
//[MODCOD 6:2 ][TYPE 1:0 ]
void DVBS2::s2_pl_header_encode( u8 modcod, u8 type, int *out)
{
    unsigned char code;

    code = (modcod<<2) | type;
    //printf("MODCOD %d TYPE %d %d\n",modcod,type,code);
    // Add the modcod and type information and scramble it
    b_64_7_code( code, out );
}
void DVBS2::s2_pl_header_create(void)
{
    int type, modcod;

    modcod = 0;

    if( m_format[0].frame_type == FRAME_NORMAL )
        type = 0;
    else
        type = 2;

    if( m_format[0].pilots ) type |= 1;

    // Mode and code rate
    if( m_format[0].constellation == M_QPSK )
    {
        switch( m_format[0].code_rate )
        {
        case CR_1_4:
            modcod = 1;
            break;
        case CR_1_3:
            modcod = 2;
            break;
        case CR_2_5:
            modcod = 3;
            break;
        case CR_1_2:
            modcod = 4;
            break;
        case CR_3_5:
            modcod = 5;
            break;
        case CR_2_3:
            modcod = 6;
            break;
        case CR_3_4:
            modcod = 7;
            break;
        case CR_4_5:
            modcod = 8;
            break;
        case CR_5_6:
            modcod = 9;
            break;
        case CR_8_9:
            modcod = 10;
            break;
        case CR_9_10:
            modcod = 11;
            break;
        default:
            modcod = 0;
            break;
        }
    }

    if( m_format[0].constellation == M_8PSK )
    {
        switch( m_format[0].code_rate )
        {
            case CR_3_5:
                modcod = 12;
                break;
            case CR_2_3:
                modcod = 13;
                break;
            case CR_3_4:
                modcod = 14;
                break;
            case CR_5_6:
                modcod = 15;
                break;
            case CR_8_9:
                modcod = 16;
                break;
            case CR_9_10:
                modcod = 17;
                break;
            default:
                modcod = 0;
                break;
            }
    }

    if( m_format[0].constellation == M_16APSK )
    {
        switch( m_format[0].code_rate )
        {
        case CR_2_3:
                modcod = 18;
                break;
        case CR_3_4:
                modcod = 19;
                break;
        case CR_4_5:
                modcod = 20;
                break;
        case CR_5_6:
                modcod = 21;
                break;
        case CR_8_9:
                modcod = 22;
                break;
        case CR_9_10:
                modcod = 23;
                break;
        default:
                modcod = 0;
                break;
        }
    }

    if( m_format[0].constellation == M_32APSK )
    {
        switch( m_format[0].code_rate )
        {
        case CR_3_4:
                modcod = 24;
                break;
        case CR_4_5:
                modcod = 25;
                break;
        case CR_5_6:
                modcod = 26;
                break;
        case CR_8_9:
                modcod = 27;
                break;
        case CR_9_10:
                modcod = 28;
                break;
        default:
                modcod = 0;
                break;
        }
    }
    // Now create the PL header.
    int b[90];
    // Add the sync sequence SOF
    for( int i = 0; i < 26; i++ ) b[i] = ph_sync_seq[i];
    // Add the mode and code
    s2_pl_header_encode( modcod, type, &b[26] );

    // BPSK modulate and add the header
    for( int i = 0; i < 90; i++ )
    {
       m_pl[i] =  m_bpsk[i&1][b[i]];
    }
}
//
// m_symbols is the total number of complex symbols in the frame
// Modulate the data starting at symbol 90
//
int  DVBS2::s2_pl_data_pack( void )
{
    int m = 0;
    int n = 90;// Jump over header
    int blocks = m_payload_symbols/90;
    int block_count = 0;

    // See if PSK
    if( m_format[0].constellation == M_QPSK )
    {
        for( int i = 0; i < blocks; i++ )
        {
            for( int j = 0; j < 90; j++ )
            {
                m_pl[n++] = m_qpsk[m_iframe[m++]&0x3];
            }
            block_count = (block_count+1)%16;
            if((block_count == 0)&&(i<blocks-1))
            {
                if( m_format[0].pilots )
                {
                    // Add pilots if needed
                    for( int k = 0; k < 36; k++ )
                    {
                        m_pl[n++] = m_bpsk[0][0];
                    }
                }
            }
        }
    }
    // See if 8 PSK
    if( m_format[0].constellation == M_8PSK )
    {
        for( int i = 0; i < blocks; i++ )
        {
            for( int j = 0; j < 90; j++ )
            {
                m_pl[n++] = m_8psk[m_iframe[m++]&0x7];
            }
            block_count = (block_count+1)%16;
            if((block_count == 0)&&(i<blocks-1))
            {
                if( m_format[0].pilots )
                {
                    // Add pilots if needed
                    for( int k = 0; k < 36; k++ )
                    {
                        m_pl[n++] = m_bpsk[0][0];
                    }
                }
            }
        }
    }
    // See if 16 PSK
    if( m_format[0].constellation == M_16APSK )
    {
        for( int i = 0; i < blocks; i++ )
        {
            for( int j = 0; j < 90; j++ )
            {
                   m_pl[n++] = m_16apsk[m_iframe[m++]&0xF];
            }
            block_count = (block_count+1)%16;
            if((block_count == 0)&&(i<blocks-1))
            {
                if( m_format[0].pilots )
                {
                    // Add pilots if needed
                    for( int k = 0; k < 36; k++ )
                    {
                        m_pl[n++] = m_bpsk[0][0];
                    }
                }
            }
        }
    }
    // See if 32 APSK
    if( m_format[0].constellation == M_32APSK )
    {
        for( int i = 0; i < blocks; i++ )
        {
            for( int j = 0; j < 90; j++ )
            {
                m_pl[n++] = m_32apsk[m_iframe[m++]&0x1F];
            }
            block_count = (block_count+1)%16;
            if((block_count == 0)&&(i<blocks-1))
            {
                if( m_format[0].pilots )
                {
                    // Add pilots if needed
                    for( int k = 0; k < 36; k++ )
                    {
                        m_pl[n++] = m_bpsk[0][0];
                    }
                }
            }
        }
    }
    // Now apply the scrambler to the data part not the header
    pl_scramble_symbols( &m_pl[90], n - 90 );
    // Return the length
    return n;
}
//
// This is not used for Broadcast mode
//
void DVBS2::pl_build_dummy( void )
{
    int n = 0;
    int b[90];

    // Add the sync sequence SOF
    for( int i = 0; i < 26; i++ ) b[i] = ph_sync_seq[i];
    // Add the mode and code and sync sequence
    s2_pl_header_encode( 0, 0, &b[26] );
    // BPSK Modulate
    for( int i = 0; i < 90; i++ )
    {
       m_pl[i].re = m_bpsk[i&1][b[i]].re;
       m_pl[i].im = m_bpsk[i&1][b[i]].im;
    }
    n += (90*36);
    pl_scramble_dummy_symbols( n );
    m_dummy_frame_length = n;
}

scmplx * DVBS2::pl_get_frame( void )
{
    return m_pl;
}

scmplx * DVBS2::pl_get_dummy( int &len )
{
    scmplx * frame;

    len   = m_dummy_frame_length;
    frame = m_pl_dummy;
    return frame;
}
