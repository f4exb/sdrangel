#include "memory.h"
#include "DVB2.h"

/*
for( int row = 0; row < rows; row++ ) 
{ 
	for( int n = 0; n < 360; n++ ) 
	{ 
		for( int col = 1; col <= table_name[row][0]; col++ ) 
		{ 
			// parity bit location, position from start of block  
			m_ldpc_encode.p[index] = m_format.kldpc + (table_name[row][col] + (n*m_format.q_val))%pbits; 
			// data bit to be accumulated 
			m_ldpc_encode.d[index] = im; 
			index++; 
		} 
		im++; 
	} 
} 
*/

#define LDPC_BF( TABLE_NAME, ROWS ) \
for( int row = 0; row < ROWS; row++ ) \
{ \
	for( int n = 0; n < 360; n++ ) \
	{ \
		for( int col = 1; col <= TABLE_NAME[row][0]; col++ ) \
		{ \
                        m_ldpc_encode.p[index] =  (TABLE_NAME[row][col] + (n*q))%pbits; \
			m_ldpc_encode.d[index] = im; \
			index++; \
		} \
		im++; \
	} \
} 

/*
//
// Generate the lookup tables for the ldpc mode
//
void DVBS2::ldpc_lookup_generate( void )
{
	int im;
	int index;
    int pbits;
    int uc;
	int rows;

	index = 0;
	im = 0;

	if(m_format.frame_type == FRAME_NORMAL)
	{
		if(m_format.code_rate == CR_2_3)
		{
			uc    = m_format.kldpc;
			pbits = m_format.nldpc - m_format.kldpc; //number of parity bits
			rows  = 120;

			for( int row = 0; row < rows; row++ )// for each row
			{
				for( int n = 0; n < 360; n++ )
				{
					for( int col = 1; col <= ldpc_tab_2_3N[row][0]; col++ )
					{
						// parity bit location, position from start of block
						m_ldpc_encode.p[index] = m_format.kldpc + (ldpc_tab_2_3N[row][col] + (n*m_format.q_val))%pbits;
						// data bit to be accumulated
						m_ldpc_encode.d[index] = im;
						index++;
					}
					im++;
				}
			}
		}
	}
	m_ldpc_encode.length = index;
}
*/
void DVB2::ldpc_lookup_generate( void )
{
    int im;
    int index;
    int pbits;
    int q;
    index = 0;
    im = 0;

    pbits = m_format[0].nldpc - m_format[0].kldpc; //number of parity bits
    q = m_format[0].q_val;

    if(m_format[0].frame_type == FRAME_NORMAL)
    {
        if( m_format[0].code_rate == CR_1_4 ) LDPC_BF( ldpc_tab_1_4N,  45  );
        if( m_format[0].code_rate == CR_1_3 ) LDPC_BF( ldpc_tab_1_3N,  60  );
        if( m_format[0].code_rate == CR_2_5 ) LDPC_BF( ldpc_tab_2_5N,  72  );
        if( m_format[0].code_rate == CR_1_2 ) LDPC_BF( ldpc_tab_1_2N,  90  );
        if( m_format[0].code_rate == CR_3_5 ) LDPC_BF( ldpc_tab_3_5N,  108 );
        if( m_format[0].code_rate == CR_2_3 ) LDPC_BF( ldpc_tab_2_3N,  120 );
        if( m_format[0].code_rate == CR_3_4 ) LDPC_BF( ldpc_tab_3_4N,  135 );
        if( m_format[0].code_rate == CR_4_5 ) LDPC_BF( ldpc_tab_4_5N,  144 );
        if( m_format[0].code_rate == CR_5_6 ) LDPC_BF( ldpc_tab_5_6N,  150 );
        if( m_format[0].code_rate == CR_8_9 ) LDPC_BF( ldpc_tab_8_9N,  160 );
        if( m_format[0].code_rate == CR_9_10) LDPC_BF( ldpc_tab_9_10N, 162 );
    }
    if(m_format[0].frame_type == FRAME_SHORT)
    {
        if( m_format[0].code_rate == CR_1_4 ) LDPC_BF( ldpc_tab_1_4S, 9  );
        if( m_format[0].code_rate == CR_1_3 ) LDPC_BF( ldpc_tab_1_3S, 15 );
        if( m_format[0].code_rate == CR_2_5 ) LDPC_BF( ldpc_tab_2_5S, 18 );
        if( m_format[0].code_rate == CR_1_2 ) LDPC_BF( ldpc_tab_1_2S, 20 );
        if( m_format[0].code_rate == CR_3_5 ) LDPC_BF( ldpc_tab_3_5S, 27 );
        if( m_format[0].code_rate == CR_2_3 ) LDPC_BF( ldpc_tab_2_3S, 30 );
        if( m_format[0].code_rate == CR_3_4 ) LDPC_BF( ldpc_tab_3_4S, 33 );
        if( m_format[0].code_rate == CR_4_5 ) LDPC_BF( ldpc_tab_4_5S, 35 );
        if( m_format[0].code_rate == CR_5_6 ) LDPC_BF( ldpc_tab_5_6S, 37 );
        if( m_format[0].code_rate == CR_8_9 ) LDPC_BF( ldpc_tab_8_9S, 40 );
    }
    m_ldpc_encode.table_length = index;
}

void DVB2::ldpc_encode( void )
{
    Bit *d,*p;
    // Calculate the number of parity bits
    int plen = m_format[0].nldpc - m_format[0].kldpc;
    d = m_frame;
    p = &m_frame[m_format[0].kldpc];
    // First zero all the parity bits
    memset( p, 0, sizeof(Bit)*plen);

    // now do the parity checking
    for( int i = 0; i < m_ldpc_encode.table_length; i++ )
    {
        p[m_ldpc_encode.p[i]] ^= d[m_ldpc_encode.d[i]];
    }
	for (int i = 1; i < plen; i++)
	{
		p[i] ^= p[i - 1];
	}
}
void DVB2::ldpc_encode_test( void )
{
    if(1)// m_format.code_rate == CR_1_2 )
    {
        printf("\n\nEncode length %d\n",m_ldpc_encode.table_length);
        printf("Parity start  %d\n",m_format[0].kldpc);
        for( int i = 0; i < m_ldpc_encode.table_length; i++ )
        {
            if(m_ldpc_encode.d[i] == 0 )
            {
                printf("%d+%d\n",m_ldpc_encode.p[i],m_ldpc_encode.d[i]);
            }
        }
        printf("Encode test end\n\n");
    }
}
