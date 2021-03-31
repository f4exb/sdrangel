#include "DVB2.h"

#ifndef __SCMPLX__
typedef struct{
    short re;
    short im;
}scmplx;

#endif

#ifndef DVBS2_H
#define DVBS2_H

#ifndef M_PI
#define M_PI 3.14159f
#endif

#define loggerf printf

class DVBS2 : public DVB2{
private:
        const static unsigned long g[6];
        const static int ph_scram_tab[64];
        const static int ph_sync_seq[26];
        scmplx m_bpsk[2][2];
        scmplx m_qpsk[4];
        scmplx m_8psk[8];
        scmplx m_16apsk[16];
        scmplx m_32apsk[32];
        scmplx m_pl[FRAME_SIZE_NORMAL];
        scmplx m_pl_dummy[FRAME_SIZE_NORMAL];
        int m_cscram[FRAME_SIZE_NORMAL];
        int m_iframe[FRAME_SIZE_NORMAL];
        int m_payload_symbols;
        int m_dummy_frame_length;
		int m_configured;
        double m_efficiency;
        int m_s2_config_updated;
        void b_64_7_code( unsigned char in, int *out );
        void s2_pl_header_encode( u8 modcod, u8 type, int *out);
        void modulator_configuration(void);
        void s2_interleave( void );
        void s2_pl_header_create(void);
        int  s2_pl_data_pack( void );
        void pl_scramble_symbols( scmplx *fs, int len );
        void pl_scramble_dummy_symbols( int len );
        void pl_build_dummy( void );
        int parity_chk( long a, long b);
        void build_symbol_scrambler_table( void );
        void calc_efficiency( void );
        void end_of_frame_actions(void);

public:
		int is_valid(int mod, int coderate);		
		double s2_get_efficiency( void );
        void physical(void);
        int s2_set_configure( DVB2FrameFormat *f );
        void s2_get_configure( DVB2FrameFormat *f );
        scmplx *pl_get_frame(void);
        scmplx *pl_get_dummy( int &len );
        int s2_add_ts_frame( u8 *ts );
	DVBS2();
};

#endif
