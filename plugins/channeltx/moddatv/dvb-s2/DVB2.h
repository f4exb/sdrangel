#ifndef DVB2_H
#define DVB2_H
#include <queue>
#include <deque>
#include <list>
#include <stdio.h>

typedef unsigned int u32;
typedef unsigned char u8;


using namespace std;

// BB HEADER fileds
#define TS_GS_TRANSPORT 3
#define TS_GS_GENERIC_PACKETIZED 0
#define TS_GS_GENERIC_CONTINUOUS 1
#define TS_GS_RESERVED 2

#define SIS_MIS_SINGLE 1
#define SIS_MIS_MULTIPLE 0

#define CCM 1
#define ACM 0

#define ISSYI_ACTIVE 1
#define ISSYI_NOT_ACTIVE 0

#define NPD_ACTIVE 1
#define NPD_NOT_ACTIVE 0
// Rolloff
#define RO_0_35 0
#define RO_0_25 1
#define RO_0_20 2
#define RO_RESERVED 3

// Pilots
#define PILOTS_OFF 0
#define PILOTS_ON 1

typedef struct{
    int ts_gs;
    int sis_mis;
    int ccm_acm;
    int issyi;
    int npd;
    int ro;
    int isi;
    int upl;
    int dfl;
    int sync;
        int syncd;
}BBHeader;

typedef int Bit;

// The number of useable and stuff bits in a frame
typedef struct{
    int data_bits;
    int stuff_bits;
}FrameBits;

#define FRAME_SIZE_NORMAL 64800
#define FRAME_SIZE_SHORT  16200
#define LDPC_ENCODE_TABLE_LENGTH (FRAME_SIZE_NORMAL*10)

typedef struct{
    int table_length;
    Bit d[LDPC_ENCODE_TABLE_LENGTH];
    Bit p[LDPC_ENCODE_TABLE_LENGTH];
}Ldpc_encode_table;

#define FRAME_NORMAL 0x00
#define FRAME_SHORT  0x10
#define BB_HEADER_LENGTH_BITS 72
#define CRC8_LENGTH_BITS 8

#define PADDING_LENGTH 200

// Code rates
#define CR_1_4 0
#define CR_1_3 1
#define CR_2_5 2
#define CR_1_2 3
#define CR_3_5 4
#define CR_2_3 5
#define CR_3_4 6
#define CR_4_5 7
#define CR_5_6 8
#define CR_8_9 9
#define CR_9_10 10
// BCH Code
#define BCH_CODE_N8  0
#define BCH_CODE_N10 1
#define BCH_CODE_N12 2
#define BCH_CODE_S12 3
// Constellation
#define M_QPSK   0
#define M_8PSK   1
#define M_16APSK 2
#define M_32APSK 3
//Interface
#define M_ACM 0
#define M_CCM 1
#define M_VCM 2


typedef struct{
   int frame_type;
   int code_rate;
   int roll_off;
   int constellation;
   int pilots;
   int dummy_frame;
   int null_deletion;
   int intface;
   int broadcasting;
   // Calculated information, not used by caller
   BBHeader bb_header;
   int kldpc;
   int kbch;
   int nldpc;
   int q_val;
   int bch_code;
   int useable_data_bits;
   int padding_bits;
   int total_bits;
   int nr_tps;
   int nr_tps_bits;
}DVB2FrameFormat;


class DVB2{

public:

protected:
    Bit m_frame[FRAME_SIZE_NORMAL];
    DVB2FrameFormat m_format[2];
    Bit m_padding[PADDING_LENGTH];
        int m_frame_offset_bits;
        int m_params_changed;

private:
    int m_bbheader[BB_HEADER_LENGTH_BITS+CRC8_LENGTH_BITS];
    int m_bb_randomise[FRAME_SIZE_NORMAL];
    u32 m_poly_n_8[4];
    u32 m_poly_n_10[5];
    u32 m_poly_n_12[6];
    u32 m_poly_s_12[6];
    u8  m_crc_tab[256];
    u8  m_dnp; // Deleted null packet
    // Transport packet queue
    queue <u8> m_tp_q;

    // LDPC tables
    const static int ldpc_tab_1_4N[45][13];
    const static int ldpc_tab_1_3N[60][13];
    const static int ldpc_tab_2_5N[72][13];
    const static int ldpc_tab_1_2N[90][9];
    const static int ldpc_tab_3_5N[108][13];
    const static int ldpc_tab_2_3N[120][14];
    const static int ldpc_tab_3_4N[135][13];
    const static int ldpc_tab_4_5N[144][12];
    const static int ldpc_tab_5_6N[150][14];
    const static int ldpc_tab_8_9N[160][5];
    const static int ldpc_tab_9_10N[162][5];
    const static int ldpc_tab_1_4S[9][13];
    const static int ldpc_tab_1_3S[15][13];
    const static int ldpc_tab_2_5S[18][13];
    const static int ldpc_tab_1_2S[20][9];
    const static int ldpc_tab_3_5S[27][13];
    const static int ldpc_tab_2_3S[30][14];
    const static int ldpc_tab_3_4S[33][13];
    const static int ldpc_tab_4_5S[35][4];
    const static int ldpc_tab_5_6S[37][14];
    const static int ldpc_tab_8_9S[40][5];


    Ldpc_encode_table m_ldpc_encode;
    void bb_randomise(void);
    void init_scrambler(void);
    void init_bb_randomiser(void);
    void ldpc_lookup_generate(void);
    void ldpc_encode( void );
    int add_transport_packet( u8 *pkt, Bit *b );
    void build_crc8_table( void );
    u8   calc_crc8( u8 *b, int len );
    int add_crc8_bits( Bit *in, int length );
    void unpack_transport_packet_add_crc( u8 *ts );
    void add_bbheader( void );
    void poly_reverse( int *pin, int *pout, int len );
    void poly_pack( const int *pin, u32* pout, int len );
    int poly_mult( const int *ina, int lena, const int *inb, int lenb, int *out );
    void bch_poly_build_tables( void );
    void reg_4_shift( u32 *sr );
    void reg_5_shift( u32 *sr );
    void reg_6_shift( u32 *sr );
    Bit bch_n_8_encode(  Bit *in, int len );
    Bit bch_n_10_encode( Bit *in, int len );
    Bit bch_n_12_encode( Bit *in, int len );
    Bit bch_s_12_encode( Bit *in, int len );
    int bch_encode( void );
    int add_ts_frame_base( u8 *ts );
    void ldpc_encode_test();
    void base_end_of_frame_actions(void);
protected:
    int  set_configure( DVB2FrameFormat *f );
    void get_configure( DVB2FrameFormat *f );
    int  next_ts_frame_base( u8 *ts );
public:
    // encode a new transport packet
//    virtual void physical(void);
    DVB2();
    ~DVB2();
};

#endif
