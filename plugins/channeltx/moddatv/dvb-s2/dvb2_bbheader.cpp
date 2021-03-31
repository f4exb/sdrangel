#include "DVB2.h"
#include "memory.h"
//
// This file adds the BB header and new transport packets
//
#define CRC_POLY 0xAB
// Reversed
#define CRC_POLYR 0xD5

void DVB2::build_crc8_table( void )
{
    int r,crc;

    for( int i = 0; i < 256; i++ )
    {
        r = i;
        crc = 0;
        for( int j = 7; j >= 0; j-- )
        {
            if((r&(1<<j)?1:0) ^ ((crc&0x80)?1:0))
                crc = (crc<<1)^CRC_POLYR;
            else
                crc <<= 1;
        }
        m_crc_tab[i] = crc;
    }
}

u8 DVB2::calc_crc8( u8 *b, int len )
{
    u8 crc = 0;

    for( int i = 0; i < len; i++ )
    {
        crc = m_crc_tab[b[i]^crc];
    }
    return crc;
}

//
// MSB is sent first
//
// The polynomial has been reversed, this is only used for the BB header
//

int DVB2::add_crc8_bits( Bit *in, int length )
{
    int crc = 0;
    int b;
    int i = 0;

    for( int n = 0; n < length; n++ )
    {
        b = in[i++] ^ (crc&0x01);
        crc >>= 1;
        if( b ) crc ^= CRC_POLY;
    }

    for( int n = 0; n < 8; n++ )
    {
        in[i++] = (crc&(1<<n))? 1 : 0;
    }
    return 8;// Length of CRC
}

//
// Formatted into a binary array
//
void DVB2::add_bbheader( void )
{
    int temp;
    BBHeader *h = &m_format[0].bb_header;

    m_frame[0] = h->ts_gs>>1;
    m_frame[1] = h->ts_gs&1;
    m_frame[2] = h->sis_mis;
    m_frame[3] = h->ccm_acm;
    m_frame[4] = h->issyi&1;
    m_frame[5] = h->npd&1;
    m_frame[6] = h->ro>>1;
    m_frame[7] = h->ro&1;
    m_frame_offset_bits = 8;
    if( h->sis_mis == SIS_MIS_MULTIPLE )
    {
        temp = h->isi;
        for( int n = 7; n >= 0; n-- )
        {
            m_frame[m_frame_offset_bits++] = temp&(1<<n)? 1 : 0;
        }
    }
    else
    {
        for( int n = 7; n >= 0 ; n-- )
        {
            m_frame[m_frame_offset_bits++] = 0;
        }
    }
    temp = h->upl;
    for( int n = 15; n >= 0; n-- )
    {
        m_frame[m_frame_offset_bits++] = temp&(1<<n)?1:0;
    }
    temp = h->dfl;
    for( int n = 15; n >= 0; n-- )
    {
        m_frame[m_frame_offset_bits++] = temp&(1<<n)?1:0;
    }
    temp = h->sync;
    for( int n = 7; n >= 0; n-- )
    {
        m_frame[m_frame_offset_bits++] = temp&(1<<n)?1:0;
    }
    // Calculate syncd, this should point to the MSB of the CRC
    temp = m_tp_q.size();
    if( temp == 0 )
        temp = 187*8;
    else
        temp = (temp-1)*8;
    //temp = h->syncd;// Syncd
    for( int n = 15; n >= 0; n-- )
    {
        m_frame[m_frame_offset_bits++] = temp&(1<<n)?1:0;
    }
    // Add CRC to BB header, at end
    int len = BB_HEADER_LENGTH_BITS;
    m_frame_offset_bits += add_crc8_bits( m_frame, len );
}

void DVB2::unpack_transport_packet_add_crc( u8 *ts )
{
    // CRC is added in place of sync byte at end of packet
    // skip the header 0x47
    u8 crc = calc_crc8( &ts[1], 188-1  );
    // Add the transport packet to the transport queue
    for( int i = 1; i < 188; i++ ) m_tp_q.push( ts[i] );
    // Add the crc
    m_tp_q.push(crc);
    // now pull off the transport queue and fill the frame
    while((m_tp_q.size()!= 0)&&(m_frame_offset_bits!=m_format[0].kbch))
    {
        // Add to the frame bit array
        u8 b = m_tp_q.front();
        m_tp_q.pop();
        for( int n = 7; n >= 0; n-- )
        {
            m_frame[m_frame_offset_bits++] = b&(1<<n)? 1 : 0;
        }
    }
}
