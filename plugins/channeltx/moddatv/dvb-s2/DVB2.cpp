#include "memory.h"
#include "DVB2.h"


//
// Update working parameters for the next frame
// This prevents parameters changing during a frame
//
void DVB2::base_end_of_frame_actions(void)
{
    if( m_params_changed )
    {
        m_format[0] = m_format[1];
        ldpc_lookup_generate();
        m_params_changed = 0;
    }
    // reset the pointer
    m_frame_offset_bits = 0;
}

//
// This configures the system and calculates
// any required intermediate values
//
int DVB2::set_configure( DVB2FrameFormat *f )
{
    int bch_bits = 0;
    int error = 0;

    if( f->broadcasting )
    {
        // Set standard parametrs for broadcasting
        f->frame_type        = FRAME_NORMAL;
        f->bb_header.ts_gs   = TS_GS_TRANSPORT;
        f->bb_header.sis_mis = SIS_MIS_SINGLE;
        f->bb_header.ccm_acm = CCM;
        f->bb_header.issyi   = 0;
        f->bb_header.npd     = 0;
        f->bb_header.upl     = 188*8;
        f->bb_header.sync    = 0x47;
    }
    f->bb_header.ro = f->roll_off;
    // Fill in the mode specific values and bit lengths
    if( f->frame_type == FRAME_NORMAL )
    {
        f->nldpc = 64800;
        bch_bits = 192;
        f->bch_code = BCH_CODE_N12;
        // Apply code rate
        switch(f->code_rate )
        {
            case CR_1_4:
                f->q_val = 135;
                f->kbch  = 16008;
                f->bch_code = BCH_CODE_N12;
                break;
            case CR_1_3:
                f->q_val = 120;
                f->kbch  = 21408;
                f->bch_code = BCH_CODE_N12;
                break;
            case CR_2_5:
                f->q_val = 108;
                f->kbch  = 25728;
                f->bch_code = BCH_CODE_N12;
                break;
            case CR_1_2:
                f->q_val = 90;
                f->kbch  = 32208;
                f->bch_code = BCH_CODE_N12;
                break;
            case CR_3_5:
                f->q_val = 72;
                f->kbch  = 38688;
                f->bch_code = BCH_CODE_N12;
                break;
            case CR_2_3:
                bch_bits = 160;
                f->q_val = 60;
                f->kbch  = 43040;
                f->bch_code = BCH_CODE_N10;
                break;
            case CR_3_4:
                f->q_val = 45;
                f->kbch  = 48408;
                f->bch_code = BCH_CODE_N12;
                break;
            case CR_4_5:
                f->q_val = 36;
                f->kbch  = 51648;
                f->bch_code = BCH_CODE_N12;
                break;
            case CR_5_6:
                bch_bits = 160;
                f->q_val = 30;
                f->kbch  = 53840;
                f->bch_code = BCH_CODE_N10;
                break;
            case CR_8_9:
                bch_bits = 128;
                f->q_val = 20;
                f->kbch  = 57472;
                f->bch_code = BCH_CODE_N8;
                break;
            case CR_9_10:
                bch_bits = 128;
                f->q_val = 18;
                f->kbch  = 58192;
                f->bch_code = BCH_CODE_N8;
                break;
            default:
//                loggerf("Configuration error DVB2\n");
                error = -1;
                break;
        }
    }

    if( f->frame_type == FRAME_SHORT )
    {
        f->nldpc = 16200;
        bch_bits = 168;
        f->bch_code = BCH_CODE_S12;
        // Apply code rate
        switch(f->code_rate )
        {
            case CR_1_4:
                f->q_val = 36;
                f->kbch  = 3072;
                break;
            case CR_1_3:
                f->q_val = 30;
                f->kbch  = 5232;
                break;
            case CR_2_5:
                f->q_val = 27;
                f->kbch  = 6312;
                break;
            case CR_1_2:
                f->q_val = 25;
                f->kbch  = 7032;
                break;
            case CR_3_5:
                f->q_val = 18;
                f->kbch  = 9552;
                break;
            case CR_2_3:
                f->q_val = 15;
                f->kbch  = 10632;
                break;
            case CR_3_4:
                f->q_val = 12;
                f->kbch  = 11712;
                break;
            case CR_4_5:
                f->q_val = 10;
                f->kbch  = 12432;
                break;
            case CR_5_6:
                f->q_val = 8;
                f->kbch  = 13152;
                break;
            case CR_8_9:
                f->q_val = 5;
                f->kbch  = 14232;
                break;
            case CR_9_10:
                error = 1;
                f->kbch  = 0;
                break;
            default:
//                loggerf("Configuration error DVB2\n");
                error = -1;
                break;
        }
    }
    if( error == 0 )
    {
        // Length of the user packets
        f->bb_header.upl  = 188*8;
        // Payload length
        f->bb_header.dfl = f->kbch - 80;
        // Transport packet sync
        f->bb_header.sync = 0x47;
        // Start of LDPC bits
        f->kldpc = f->kbch + bch_bits;
        // Number of padding bits required (not used)
        f->padding_bits = 0;
        // Number of useable data bits (not used)
        f->useable_data_bits = f->kbch - 80;
        // Save the configuration, will be updated on next frame
        m_format[1] = *f;
        // reset various pointers
        m_dnp   = 0;// No deleted null packets
        // Signal we need to update on the next frame.
        if( m_params_changed )
            base_end_of_frame_actions();
        else
            m_params_changed = 1;
    }
    return error;
}
void DVB2::get_configure( DVB2FrameFormat *f )
{
    *f = m_format[1];
}

// Points to first byte in transport packet

int DVB2::add_ts_frame_base( u8 *ts )
{
    if( m_frame_offset_bits == 0 )
    {
         // New frame needs to be sent
        add_bbheader(); // Add the header
    }
    // Add a new transport packet
    unpack_transport_packet_add_crc( ts );
    // Have we reached the end?
    if( m_frame_offset_bits == m_format[0].kbch )
    {
        // Yes so now Scramble the BB frame
        bb_randomise();
        // BCH encode the BB Frame
        bch_encode();
        // LDPC encode the BB frame and BCHFEC bits
        ldpc_encode();
        // Signal to the modulation specific class we have something to send
        base_end_of_frame_actions();
        return 1;
    }
    return 0;
}
//
// Dump NULL packets appends a counter to the end of each UP
// it is not implemented at the moment.
//
int DVB2::next_ts_frame_base( u8 *ts )
{
    int res = 0;
    // See if we need to dump null packets
    if( m_format[0].null_deletion == 1 )
    {
        if(((ts[0]&0x1F) == 0x1F)&&(ts[1] == 0xFF ))
        {
            // Null packet detected
            if( m_dnp < 0xFF )
            {
                m_dnp++;// Increment the number of null packets
                return 0;
            }
        }
    }
    // Need to send a new transport packet 
    res = add_ts_frame_base( ts );
    if( res ) m_dnp = 0;// Clear the DNP counter
    // return whether it is time to transmit a new frame
    return res;
}
DVB2::DVB2(void)
{
    // Clear the transport queue
    m_tp_q.empty();
    init_bb_randomiser();
    bch_poly_build_tables();
    build_crc8_table();
    m_dnp   = 0;// No delted null packets
    m_frame_offset_bits = 0;
    m_params_changed = 1;
}
DVB2::~DVB2(void)
{
}
