/*---------------------------------------------------------------------------*\

  FILE........: fsk.c
  AUTHOR......: Brady O'Brien
  DATE CREATED: 11 February 2016

  Framer and deframer for VHF FreeDV modes 'A' and 'B'
  Currently designed for-
  * 40ms ota modem frames
  * 40ms Codec2 1300 frames
  * 52 bits of Codec2 per frame
  * 16 bits of unique word per frame
  * 28 'spare' bits per frame
  *  - 4 spare bits at front and end of frame (8 total) for padding
  *  - 20 'protocol' bits, either for higher layers of 'protocol' or
  *  - 18 'protocol' bits and 2 vericode sidechannel bits

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2016 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "freedv_vhf_framing.h"

namespace FreeDV
{

/* The voice UW of the VHF type A frame */
static const uint8_t A_uw_v[] =    {0,1,1,0,0,1,1,1,
                                    1,0,1,0,1,1,0,1};

/* The data UW of the VHF type A frame */
static const uint8_t A_uw_d[] =    {1,1,1,1,0,0,0,1,
                                    1,1,1,1,1,1,0,0};

/* Blank VHF type A frame */
static const uint8_t A_blank[] =   {1,0,1,0,0,1,1,1, /* Padding[0:3] Proto[0:3]   */
                                    1,0,1,0,0,1,1,1, /* Proto[4:11]               */
                                    0,0,0,0,0,0,0,0, /* Voice[0:7]                */
                                    0,0,0,0,0,0,0,0, /* Voice[8:15]               */
                                    0,0,0,0,0,0,0,0, /* Voice[16:23]              */
                                    0,1,1,0,0,1,1,1, /* UW[0:7]                   */
                                    1,0,1,0,1,1,0,1, /* UW[8:15]                  */
                                    0,0,0,0,0,0,0,0, /* Voice[24:31]              */
                                    0,0,0,0,0,0,0,0, /* Voice[32:39]              */
                                    0,0,0,0,0,0,0,0, /* Voice[40:47]              */
                                    0,0,0,0,0,0,1,0, /* Voice[48:51] Proto[12:15] */
                                    0,1,1,1,0,0,1,0};/* Proto[16:19] Padding[4:7] */

/* Blank VHF type AT (A for TDMA; padding bits not transmitted) frame */
static const uint8_t AT_blank[] =  {        0,1,1,1, /*              Proto[0:3]   */
                                    1,0,1,0,0,1,1,1, /* Proto[4:11]               */
                                    0,0,0,0,0,0,0,0, /* Voice[0:7]                */
                                    0,0,0,0,0,0,0,0, /* Voice[8:15]               */
                                    0,0,0,0,0,0,0,0, /* Voice[16:23]              */
                                    0,1,1,0,0,1,1,1, /* UW[0:7]                   */
                                    1,0,1,0,1,1,0,1, /* UW[8:15]                  */
                                    0,0,0,0,0,0,0,0, /* Voice[24:31]              */
                                    0,0,0,0,0,0,0,0, /* Voice[32:39]              */
                                    0,0,0,0,0,0,0,0, /* Voice[40:47]              */
                                    0,0,0,0,0,0,1,0, /* Voice[48:51] Proto[12:15] */
                                    0,1,1,1        };/* Proto[16:19]              */

/* HF Type B voice UW */
static const uint8_t B_uw_v[] =    {0,1,1,0,0,1,1,1};

/* HF Type B data UW */
static const uint8_t B_uw_d[] =    {1,1,1,1,0,0,1,0};

/* Blank HF type B frame */
static const uint8_t B_blank[] =   {0,1,1,0,0,1,1,1, /* UW[0:7]					  */
									0,0,0,0,0,0,0,0, /* Voice1[0:7]				  */
									0,0,0,0,0,0,0,0, /* Voice1[8:15]			  */
									0,0,0,0,0,0,0,0, /* Voice1[16:23]			  */
									0,0,0,0,0,0,0,0, /* Voice1[24:28] Voice2[0:3] */
									0,0,0,0,0,0,0,0, /* Voice2[4:11]			  */
									0,0,0,0,0,0,0,0, /* Voice2[12:19]			  */
									0,0,0,0,0,0,0,0};/* Voice2[20:28]			  */

/* States */
#define ST_NOSYNC 0 /* Not synchronized */
#define ST_SYNC 1   /* Synchronized */

/* Get a single bit out of an MSB-first packed byte array */
#define UNPACK_BIT_MSBFIRST(bytes,bitidx) ((bytes)[(bitidx)>>3]>>(7-((bitidx)&0x7)))&0x1

enum frame_payload_type {
    FRAME_PAYLOAD_TYPE_VOICE,
    FRAME_PAYLOAD_TYPE_DATA,
};

/* Place codec and other bits into a frame */
void fvhff_frame_bits(  int frame_type,
                        uint8_t bits_out[],
                        uint8_t codec2_in[],
                        uint8_t proto_in[],
                        uint8_t vc_in[]){
    int i,ibit;
    if(frame_type == FREEDV_VHF_FRAME_A){
        /* Fill out frame with blank frame prototype */
        for(i=0; i<96; i++)
            bits_out[i] = A_blank[i];

        /* Fill in protocol bits, if present */
        if(proto_in!=NULL){
            ibit = 0;
            /* First half of protocol bits */
            /* Extract and place in frame, MSB first */
            for(i=4 ; i<16; i++){
                bits_out[i] = UNPACK_BIT_MSBFIRST(proto_in,ibit);
                ibit++;
            }
            /* Last set of protocol bits */
            for(i=84; i<92; i++){
                bits_out[i] = UNPACK_BIT_MSBFIRST(proto_in,ibit);
                ibit++;
            }
        }

        /* Fill in varicode bits, if present */
        if(vc_in!=NULL){
            bits_out[90] = vc_in[0];
            bits_out[91] = vc_in[1];
        }

        /* Fill in codec2 bits, present or not */
        ibit = 0;
        for(i=16; i<40; i++){   /* First half */
            bits_out[i] = UNPACK_BIT_MSBFIRST(codec2_in,ibit);
            ibit++;
        }
        for(i=56; i<84; i++){   /* Second half */
            bits_out[i] = UNPACK_BIT_MSBFIRST(codec2_in,ibit);
            ibit++;
        }
    }else if(frame_type == FREEDV_HF_FRAME_B){
        /* Pointers to both c2 frames so the bit unpack macro works */
        uint8_t * codec2_in1 = &codec2_in[0];
        uint8_t * codec2_in2 = &codec2_in[4];
        /* Fill out frame with blank prototype */
        for(i=0; i<64; i++)
            bits_out[i] = B_blank[i];

        /* Fill out first codec2 block */
        ibit=0;
        for(i=8; i<36; i++){
            bits_out[i] = UNPACK_BIT_MSBFIRST(codec2_in1,ibit);
            ibit++;
        }
        /* Fill out second codec2 block */
        ibit=0;
        for(i=36; i<64; i++){
            bits_out[i] = UNPACK_BIT_MSBFIRST(codec2_in2,ibit);
            ibit++;
        }
    }else if(frame_type == FREEDV_VHF_FRAME_AT){
        /* Fill out frame with blank frame prototype */
        for(i=0; i<88; i++)
            bits_out[i] = AT_blank[i];

        /* Fill in protocol bits, if present */
        if(proto_in!=NULL){
            ibit = 0;
            /* First half of protocol bits */
            /* Extract and place in frame, MSB first */
            for(i=0 ; i<12; i++){
                bits_out[i] = UNPACK_BIT_MSBFIRST(proto_in,ibit);
                ibit++;
            }
            /* Last set of protocol bits */
            for(i=80; i<88; i++){
                bits_out[i] = UNPACK_BIT_MSBFIRST(proto_in,ibit);
                ibit++;
            }
        }

        /* Fill in varicode bits, if present */
        if(vc_in!=NULL){
            bits_out[86] = vc_in[0];
            bits_out[87] = vc_in[1];
        }

        /* Fill in codec2 bits, present or not */
        ibit = 0;
        for(i=12; i<36; i++){   /* First half */
            bits_out[i] = UNPACK_BIT_MSBFIRST(codec2_in,ibit);
            ibit++;
        }
        for(i=52; i<80; i++){   /* Second half */
            bits_out[i] = UNPACK_BIT_MSBFIRST(codec2_in,ibit);
            ibit++;
        }
    }
}

/* Place data and other bits into a frame */
void fvhff_frame_data_bits(struct freedv_vhf_deframer * def, int frame_type,
                        uint8_t bits_out[]){
    int i,ibit;
    if(frame_type == FREEDV_VHF_FRAME_A){
        uint8_t data[8];
        int end_bits;
        int from_bit;
        int bcast_bit;
        int crc_bit;

        /* Fill out frame with blank frame prototype */
        for(i=0; i<4; i++)
            bits_out[i] = A_blank[i];
        for(i=92; i<96; i++)
            bits_out[i] = A_blank[i];

        /* UW data */
        for (i=0; i < 16; i++)
            bits_out[40 + i] = A_uw_d[i];

        if (def->fdc)
                freedv_data_channel_tx_frame(def->fdc, data, 8, &from_bit, &bcast_bit, &crc_bit, &end_bits);
        else
            return;

        bits_out[4] = from_bit;
        bits_out[5] = bcast_bit;
        bits_out[6] = 0; /* unused */
        bits_out[7] = 0; /* unused */

        /* Fill in data bits */
        ibit = 0;
        for(i=8; i<40; i++){   /* First half */
            bits_out[i] = UNPACK_BIT_MSBFIRST(data,ibit);
            ibit++;
        }
        for(i=56; i<88; i++){  /* Second half */
            bits_out[i] = UNPACK_BIT_MSBFIRST(data,ibit);
            ibit++;
        }

        for (i = 0; i < 4; i++)
            bits_out[88 + i] = (end_bits >> (3-i)) & 0x1;
    } else if (frame_type == FREEDV_HF_FRAME_B){
        uint8_t data[6];
        int end_bits;
        int from_bit;
        int bcast_bit;
        int crc_bit;

        /* Fill out frame with blank prototype */
        for(i=0; i<64; i++)
            bits_out[i] = B_blank[i];

        /* UW data */
        for (i=0; i < 8; i++)
            bits_out[0 + i] = B_uw_d[i];

        if (def->fdc)
            freedv_data_channel_tx_frame(def->fdc, data, 6, &from_bit, &bcast_bit, &crc_bit, &end_bits);
        else
            return;

        bits_out[56] = from_bit;
        bits_out[57] = bcast_bit;
        bits_out[58] = crc_bit;
        bits_out[59] = 0; /* unused */

        /* Fill in data bits */
        ibit = 0;
        for(i=8; i<56; i++){   /* First half */
            bits_out[i] = UNPACK_BIT_MSBFIRST(data,ibit);
            ibit++;
        }
        for (i = 0; i < 4; i++)
            bits_out[60 + i] = (end_bits >> (3-i)) & 0x1;
    }
}

/* Init and allocate memory for a freedv-vhf framer/deframer */
struct freedv_vhf_deframer * fvhff_create_deframer(uint8_t frame_type, int enable_bit_flip){
    struct freedv_vhf_deframer * deframer;
    uint8_t *bits,*invbits;
    int frame_size;
    int uw_size;

    assert( (frame_type == FREEDV_VHF_FRAME_A) || (frame_type == FREEDV_HF_FRAME_B) );

    /* It's a Type A frame */
    if(frame_type == FREEDV_VHF_FRAME_A){
        frame_size = 96;
        uw_size = 16;
    }else if(frame_type == FREEDV_HF_FRAME_B){
        frame_size = 64;
        uw_size = 8;
    }else{
        return NULL;
    }

    /* Allocate memory for the thing */
    deframer = (freedv_vhf_deframer*) malloc(sizeof(struct freedv_vhf_deframer));
    if(deframer == NULL)
        return NULL;

    /* Allocate the not-bit buffer */
    if(enable_bit_flip){
        invbits = (uint8_t*) malloc(sizeof(uint8_t)*frame_size);
        if(invbits == NULL) {
            free(deframer);
            return NULL;
        }
    }else{
        invbits = NULL;
    }

    /* Allocate the bit buffer */
    bits = (uint8_t*) malloc(sizeof(uint8_t)*frame_size);
    if(bits == NULL) {
        free(deframer);
        return NULL;
    }

    deframer->bits = bits;
    deframer->invbits = invbits;
    deframer->ftype = frame_type;
    deframer->state = ST_NOSYNC;
    deframer->bitptr = 0;
    deframer->last_uw = 0;
    deframer->miss_cnt = 0;
    deframer->frame_size = frame_size;
    deframer->uw_size = uw_size;
    deframer->on_inv_bits = 0;
    deframer->sym_size = 1;

    deframer->ber_est = 0;
    deframer->total_uw_bits = 0;
    deframer->total_uw_err = 0;

    deframer->fdc = NULL;

    return deframer;
}

/* Get size of frame in bits */
int fvhff_get_frame_size(struct freedv_vhf_deframer * def){
    return def->frame_size;
}

/* Codec2 size in bytes */
int fvhff_get_codec2_size(struct freedv_vhf_deframer * def){
    if(def->ftype == FREEDV_VHF_FRAME_A){
        return 7;
    } else if(def->ftype == FREEDV_HF_FRAME_B){
        return 8;
    } else{
        return 0;
    }
}

/* Protocol bits in bits */
int fvhff_get_proto_size(struct freedv_vhf_deframer * def){
    if(def->ftype == FREEDV_VHF_FRAME_A){
        return 20;
    } else if(def->ftype == FREEDV_HF_FRAME_B){
        return 0;
    } else{
        return 0;
    }
}

/* Varicode bits in bits */
int fvhff_get_varicode_size(struct freedv_vhf_deframer * def){
    if(def->ftype == FREEDV_VHF_FRAME_A){
        return 2;
    } else if(def->ftype == FREEDV_HF_FRAME_B){
        return 0;
    } else{
        return 0;
    }
}

void fvhff_destroy_deframer(struct freedv_vhf_deframer * def){
    freedv_data_channel_destroy(def->fdc);
    free(def->bits);
    free(def);
}

int fvhff_synchronized(struct freedv_vhf_deframer * def){
    return (def->state) == ST_SYNC;
}

/* Search for a complete UW in a buffer of bits */
std::size_t fvhff_search_uw(const uint8_t bits[], std::size_t nbits,
                     const uint8_t uw[],    std::size_t uw_len,
                     std::size_t * delta_out,    std::size_t bits_per_sym){

    std::size_t ibits,iuw;
    std::size_t delta_min = uw_len;
    std::size_t delta;
    std::size_t offset_min = 0;
    /* Walk through buffer bits */
    for(ibits = 0; ibits < nbits-uw_len; ibits+=bits_per_sym){
        delta = 0;
        for(iuw = 0; iuw < uw_len; iuw++){
            if(bits[ibits+iuw] != uw[iuw]) delta++;
        }
        if( delta < delta_min ){
            delta_min = delta;
            offset_min = ibits;
        }
    }
    if(delta_out != NULL) *delta_out = delta_min;
    return offset_min;
}

/* See if the UW is where it should be, to within a tolerance, in a bit buffer */
static int fvhff_match_uw(struct freedv_vhf_deframer * def,uint8_t bits[],int tol,int *rdiff, enum frame_payload_type *pt){
    int frame_type  = def->ftype;
    int bitptr      = def->bitptr;
    int frame_size  = def->frame_size;
    int uw_len      = def->uw_size;
    int iuw,ibit;
    const uint8_t * uw[2];
    int uw_offset;
    int diff[2] = { 0, 0 };
    int i;
    int match[2];
    int r;

    /* defaults to make compiler happy on -O3 */

    *pt = FRAME_PAYLOAD_TYPE_VOICE;
    *rdiff = 0;

    /* Set up parameters for the standard type of frame */
    if(frame_type == FREEDV_VHF_FRAME_A){
        uw[0] = A_uw_v;
        uw[1] = A_uw_d;
        uw_len = 16;
        uw_offset = 40;
    } else if(frame_type == FREEDV_HF_FRAME_B){
        uw[0] = B_uw_v;
        uw[1] = B_uw_d;
        uw_len = 8;
        uw_offset = 0;
    } else {
        return 0;
    }

    /* Check both the voice and data UWs */
    for (i = 0; i < 2; i++) {
        /* Start bit pointer where UW should be */
        ibit = bitptr + uw_offset;
        if(ibit >= frame_size) ibit -= frame_size;
        /* Walk through and match bits in frame with bits of UW */
        for(iuw=0; iuw<uw_len; iuw++){
            if(bits[ibit] != uw[i][iuw]) diff[i]++;
            ibit++;
            if(ibit >= frame_size) ibit = 0;
        }
        match[i] = diff[i] <= tol;
    }
    /* Pick the best matching UW */

    if (diff[0] < diff[1]) {
        r = match[0];
        *rdiff = diff[0];
        *pt = FRAME_PAYLOAD_TYPE_VOICE;
    } else {
        r = match[1];
        *rdiff = diff[1];
        *pt = FRAME_PAYLOAD_TYPE_DATA;
    }

    return r;
}

static void fvhff_extract_frame_voice(struct freedv_vhf_deframer * def,uint8_t bits[],
    uint8_t codec2_out[],uint8_t proto_out[],uint8_t vc_out[]){
    int frame_type  = def->ftype;
    int bitptr      = def->bitptr;
    int frame_size  = def->frame_size;
    int iframe,ibit;

    if(frame_type == FREEDV_VHF_FRAME_A){
        /* Extract codec2 bits */
        memset(codec2_out,0,7);
        ibit = 0;
        /* Extract and pack first half, MSB first */
        iframe = bitptr+16;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<24;ibit++){
            codec2_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        /* Extract and pack last half, MSB first */
        iframe = bitptr+56;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<52;ibit++){
            codec2_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }
        /* Extract varicode bits, if wanted */
        if(vc_out!=NULL){
            iframe = bitptr+90;
            if(iframe >= frame_size) iframe-=frame_size;
            vc_out[0] = bits[iframe];
            iframe++;
            vc_out[1] = bits[iframe];
        }
        /* Extract protocol bits, if proto is passed through */
        if(proto_out!=NULL){
            /* Clear protocol bit array */
            memset(proto_out,0,3);
            ibit = 0;
            /* Extract and pack first half, MSB first */
            iframe = bitptr+4;
            if(iframe >= frame_size) iframe-=frame_size;
            for(;ibit<12;ibit++){
                proto_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
                iframe++;
                if(iframe >= frame_size) iframe=0;
            }

            /* Extract and pack last half, MSB first */
            iframe = bitptr+84;
            if(iframe >= frame_size) iframe-=frame_size;
            for(;ibit<20;ibit++){
                proto_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
                iframe++;
                if(iframe >= frame_size) iframe=0;
            }
        }

    }else if(frame_type == FREEDV_HF_FRAME_B){
        /* Pointers to both c2 frames */
        uint8_t * codec2_out1 = &codec2_out[0];
        uint8_t * codec2_out2 = &codec2_out[4];

        /* Extract codec2 bits */
        memset(codec2_out,0,8);
        ibit = 0;

        /* Extract and pack first c2 frame, MSB first */
        iframe = bitptr+8;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<28;ibit++){
            codec2_out1[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        /* Extract and pack second c2 frame, MSB first */
        iframe = bitptr+36;
        ibit = 0;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<28;ibit++){
            codec2_out2[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }
    }else if(frame_type == FREEDV_VHF_FRAME_AT){
        /* Extract codec2 bits */
        memset(codec2_out,0,7);
        ibit = 0;
        /* Extract and pack first half, MSB first */
        iframe = bitptr+12;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<24;ibit++){
            codec2_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        /* Extract and pack last half, MSB first */
        iframe = bitptr+52;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<52;ibit++){
            codec2_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }
        /* Extract varicode bits, if wanted */
        if(vc_out!=NULL){
            iframe = bitptr+86;
            if(iframe >= frame_size) iframe-=frame_size;
            vc_out[0] = bits[iframe];
            iframe++;
            vc_out[1] = bits[iframe];
        }
        /* Extract protocol bits, if proto is passed through */
        if(proto_out!=NULL){
            /* Clear protocol bit array */
            memset(proto_out,0,3);
            ibit = 0;
            /* Extract and pack first half, MSB first */
            iframe = bitptr+4;
            if(iframe >= frame_size) iframe-=frame_size;
            for(;ibit<12;ibit++){
                proto_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
                iframe++;
                if(iframe >= frame_size) iframe=0;
            }

            /* Extract and pack last half, MSB first */
            iframe = bitptr+84;
            if(iframe >= frame_size) iframe-=frame_size;
            for(;ibit<20;ibit++){
                proto_out[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
                iframe++;
                if(iframe >= frame_size) iframe=0;
            }
        }

    }
}

static void fvhff_extract_frame_data(struct freedv_vhf_deframer * def,uint8_t bits[]){
    int frame_type  = def->ftype;
    int bitptr      = def->bitptr;
    int frame_size  = def->frame_size;
    int iframe,ibit;

    if(frame_type == FREEDV_VHF_FRAME_A){
        uint8_t data[8];
        int end_bits = 0;
        int from_bit;
        int bcast_bit;

        iframe = bitptr+4;
        if(iframe >= frame_size) iframe-=frame_size;
        from_bit = bits[iframe];
        iframe++;
        if(iframe >= frame_size) iframe-=frame_size;
        bcast_bit = bits[iframe];

        /* Extract data bits */
        memset(data,0,8);
        ibit = 0;
        /* Extract and pack first half, MSB first */
        iframe = bitptr+8;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<32;ibit++){
            data[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        /* Extract and pack last half, MSB first */
        iframe = bitptr+56;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<64;ibit++){
            data[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        /* Extract endbits value, MSB first*/
        iframe = bitptr+88;
        ibit = 0;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<4;ibit++){
            end_bits |= (bits[iframe]&0x1)<<(3-(ibit));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        if (def->fdc) {
            freedv_data_channel_rx_frame(def->fdc, data, 8, from_bit, bcast_bit, 0, end_bits);
        }
    } else if(frame_type == FREEDV_HF_FRAME_B){
        uint8_t data[6];
        int end_bits = 0;
        int from_bit;
        int bcast_bit;
	int crc_bit;

        ibit = 0;
        memset(data,0,6);

        /* Extract and pack first c2 frame, MSB first */
        iframe = bitptr+8;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<48;ibit++){
            data[ibit>>3] |= (bits[iframe]&0x1)<<(7-(ibit&0x7));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        iframe = bitptr+56;
        if(iframe >= frame_size) iframe-=frame_size;
        from_bit = bits[iframe];
        iframe++;
        if(iframe >= frame_size) iframe-=frame_size;
        bcast_bit = bits[iframe];
        iframe++;
        if(iframe >= frame_size) iframe-=frame_size;
        crc_bit = bits[iframe];

        /* Extract endbits value, MSB first*/
        iframe = bitptr+60;
        ibit = 0;
        if(iframe >= frame_size) iframe-=frame_size;
        for(;ibit<4;ibit++){
            end_bits |= (bits[iframe]&0x1)<<(3-(ibit));
            iframe++;
            if(iframe >= frame_size) iframe=0;
        }

        if (def->fdc) {
            freedv_data_channel_rx_frame(def->fdc, data, 6, from_bit, bcast_bit, crc_bit, end_bits);
        }
    }
}

static void fvhff_extract_frame(struct freedv_vhf_deframer * def,uint8_t bits[],uint8_t codec2_out[],
    uint8_t proto_out[],uint8_t vc_out[],enum frame_payload_type pt){
    switch (pt) {
        case FRAME_PAYLOAD_TYPE_VOICE:
        fvhff_extract_frame_voice(def, bits, codec2_out, proto_out, vc_out);
        break;
    case FRAME_PAYLOAD_TYPE_DATA:
        fvhff_extract_frame_data(def, bits);
        break;
    }
}

/*
 * Try to find the UW and extract codec/proto/vc bits in def->frame_size bits
 */
int fvhff_deframe_bits(struct freedv_vhf_deframer * def,uint8_t codec2_out[],uint8_t proto_out[],
    uint8_t vc_out[],uint8_t bits_in[]){
    uint8_t * strbits  = def->bits;
    uint8_t * invbits  = def->invbits;
    uint8_t * bits;
    int on_inv_bits = def->on_inv_bits;
    int frame_type  = def->ftype;
    int state       = def->state;
    int bitptr      = def->bitptr;
    int last_uw     = def->last_uw;
    int miss_cnt    = def->miss_cnt;
    int frame_size  = def->frame_size;
    int uw_size     = def->uw_size;
    int uw_diff;
    int i;
    int uw_first_tol;
    int uw_sync_tol;
    int miss_tol;
    int extracted_frame = 0;
    enum frame_payload_type pt = FRAME_PAYLOAD_TYPE_VOICE;

    /* Possibly set up frame-specific params here */
    if(frame_type == FREEDV_VHF_FRAME_A){
        uw_first_tol = 1;   /* The UW bit-error tolerance for the first frame */
        uw_sync_tol = 3;    /* The UW bit error tolerance for frames after sync */
        miss_tol = 4;       /* How many UWs may be missed before going into the de-synced state */
    }else if(frame_type == FREEDV_HF_FRAME_B){
        uw_first_tol = 0;   /* The UW bit-error tolerance for the first frame */
        uw_sync_tol = 1;    /* The UW bit error tolerance for frames after sync */
        miss_tol = 3;       /* How many UWs may be missed before going into the de-synced state */
    }else{
        return 0;
    }
    /* Skip N bits for multi-bit symbol modems */
    for(i=0; i<frame_size; i++){
        /* Put a bit in the buffer */
        strbits[bitptr] = bits_in[i];
        /* If we're checking the inverted bitstream, put a bit in it */
        if(invbits!=NULL)
            invbits[bitptr] = bits_in[i]?0:1;

        bitptr++;
        if(bitptr >= frame_size) bitptr -= frame_size;
        def->bitptr = bitptr;
        /* Enter state machine */
        if(state==ST_SYNC){
            /* Already synchronized, just wait till UW is back where it should be */
            last_uw++;
            if(invbits!=NULL){
                if(on_inv_bits)
                    bits = invbits;
                else
                    bits = strbits;
            }else{
                bits=strbits;
            }
            /* UW should be here. We're sunk, so deframe anyway */
            if(last_uw == frame_size){
                last_uw = 0;

                if(!fvhff_match_uw(def,bits,uw_sync_tol,&uw_diff, &pt))
                    miss_cnt++;
                else
                    miss_cnt=0;

                /* If we go over the miss tolerance, go into no-sync */
                if(miss_cnt>miss_tol){
                    state = ST_NOSYNC;
                }
                /* Extract the bits */
                extracted_frame = 1;
                fvhff_extract_frame(def,bits,codec2_out,proto_out,vc_out,pt);

                /* Update BER estimate */
                def->ber_est = (.995*def->ber_est) + (.005*((float)uw_diff)/((float)uw_size));
                def->total_uw_bits += uw_size;
                def->total_uw_err += uw_diff;
            }
        /* Not yet sunk */
        }else{
            /* It's a sync!*/
            if(invbits!=NULL){
                if(fvhff_match_uw(def,invbits,uw_first_tol, &uw_diff, &pt)){
                    state = ST_SYNC;
                    last_uw = 0;
                    miss_cnt = 0;
                    extracted_frame = 1;
                    on_inv_bits = 1;
                    fvhff_extract_frame(def,invbits,codec2_out,proto_out,vc_out,pt);
                    /* Update BER estimate */
                    def->ber_est = (.995*def->ber_est) + (.005*((float)uw_diff)/((float)uw_size));
                    def->total_uw_bits += uw_size;
                    def->total_uw_err += uw_diff;
                }
            }
            if(fvhff_match_uw(def,strbits,uw_first_tol, &uw_diff, &pt)){
                state = ST_SYNC;
                last_uw = 0;
                miss_cnt = 0;
                extracted_frame = 1;
                on_inv_bits = 0;
                fvhff_extract_frame(def,strbits,codec2_out,proto_out,vc_out,pt);
                /* Update BER estimate */
                def->ber_est = (.995*def->ber_est) + (.005*((float)uw_diff)/((float)uw_size));
                def->total_uw_bits += uw_size;
                def->total_uw_err += uw_diff;
            }
        }
    }
    def->state = state;
    def->last_uw = last_uw;
    def->miss_cnt = miss_cnt;
    def->on_inv_bits = on_inv_bits;
    /* return zero for data frames, they are already handled by callback */
    return extracted_frame && pt == FRAME_PAYLOAD_TYPE_VOICE;
}

} // FreeDV
