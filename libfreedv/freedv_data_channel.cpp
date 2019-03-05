/*---------------------------------------------------------------------------*\

  FILE........: freedv_data_channel.c
  AUTHOR......: Jeroen Vreeken
  DATE CREATED: 03 March 2016

  Data channel for ethernet like packets in freedv VHF frames.
  Currently designed for-
  * 2 control bits per frame
  * 4 byte counter bits per frame
  * 64 bits of data per frame
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2016 Jeroen Vreeken

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

#include "freedv_data_channel.h"

#include <stdlib.h>
#include <string.h>
#include <cstddef>

namespace FreeDV
{

static unsigned char fdc_header_bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* CCIT CRC table (0x1201 polynomal) */
static unsigned short fdc_crc_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static unsigned short fdc_crc(unsigned char *buffer, std::size_t len)
{
    unsigned short crc = 0xffff;
    std::size_t i;

    for (i = 0; i < len; i++, buffer++) {
        crc = (crc >> 8) ^ fdc_crc_table[(crc ^ *buffer) & 0xff];
    }

    return crc ^ 0xffff;
}

/* CRC4 0x03 polynomal */
static unsigned char fdc_crc4(unsigned char *buffer, std::size_t len)
{
    unsigned char crc = 0x0f;
    std::size_t i;

    for (i = 0; i < len; i++, buffer++) {
        int shift;

        for (shift = 7; shift <= 0; shift--) {
            crc <<= 1;
            if ((*buffer >> shift) & 0x1)
                crc |= 1;
            if (crc & 0x10)
                crc ^= 0x03;
	}
    }

    return crc & 0x0f;
}

struct freedv_data_channel *freedv_data_channel_create(void)
{
    struct freedv_data_channel *fdc;

    fdc = (freedv_data_channel*) malloc(sizeof(struct freedv_data_channel));
    if (!fdc)
        return nullptr;

    fdc->cb_rx = nullptr;
    fdc->cb_tx = nullptr;
    fdc->packet_tx_size = 0;

    freedv_data_set_header(fdc, fdc_header_bcast);

    memcpy(fdc->rx_header, fdc->tx_header, 8);

    return fdc;
}

void freedv_data_channel_destroy(struct freedv_data_channel *fdc)
{
    free(fdc);
}


void freedv_data_set_cb_rx(struct freedv_data_channel *fdc, freedv_data_callback_rx cb, void *state)
{
    fdc->cb_rx = cb;
    fdc->cb_rx_state = state;
}

void freedv_data_set_cb_tx(struct freedv_data_channel *fdc, freedv_data_callback_tx cb, void *state)
{
    fdc->cb_tx = cb;
    fdc->cb_tx_state = state;
}

void freedv_data_channel_rx_frame(struct freedv_data_channel *fdc, unsigned char *data, std::size_t size, int from_bit, int bcast_bit, int crc_bit, int end_bits)
{
    int copy_bits;

    if (end_bits) {
        copy_bits = end_bits;
    } else {
        copy_bits = size;
    }

    /* New packet? */
    if (fdc->packet_rx_cnt == 0)
    {
        /* Does the packet have a compressed from field? */
        if (from_bit) {
            /* Compressed from: take the previously received header */
            memcpy(fdc->packet_rx + fdc->packet_rx_cnt, fdc->rx_header, 6);
            fdc->packet_rx_cnt += 6;
        }

        if (bcast_bit)
        {
            if (!from_bit)
            {
                /* Copy from header and modify size and end_bits accordingly */
                memcpy(fdc->packet_rx + fdc->packet_rx_cnt, data, 6);
                fdc->packet_rx_cnt += 6;
                copy_bits -= 6;

                if (copy_bits < 0) {
                    copy_bits = 0;
                }

                data += 6;
            }
            /* Compressed to: fill in broadcast address */
            memcpy(fdc->packet_rx + fdc->packet_rx_cnt, fdc_header_bcast, sizeof(fdc_header_bcast));
            fdc->packet_rx_cnt += 6;
        }

        if (crc_bit)
        {
            unsigned char calc_crc = fdc_crc4(data, size);

            if (calc_crc == end_bits)
            {
                /* It is a single header field, remember it for later */
                memcpy(fdc->packet_rx + 6, data, 6);
                memcpy(fdc->packet_rx, fdc_header_bcast, 6);

                if (fdc->cb_rx) {
                    fdc->cb_rx(fdc->cb_rx_state, fdc->packet_rx, 12);
                }
            }

            fdc->packet_rx_cnt = 0;
            return;
        }
    }

    if (fdc->packet_rx_cnt + copy_bits >= FREEDV_DATA_CHANNEL_PACKET_MAX)
    {
        // Something went wrong... this can not be a real packet
        fdc->packet_rx_cnt = 0;
        return;
    }
    else if (fdc->packet_rx_cnt < 0)
    {
        // This is wrong too...
        fdc->packet_rx_cnt = 0;
        return;
    }

    memcpy(fdc->packet_rx + fdc->packet_rx_cnt, data, copy_bits);
    fdc->packet_rx_cnt += copy_bits;

    if (end_bits != 0 && fdc->packet_rx_cnt >= 2)
    {
        unsigned short calc_crc = fdc_crc(fdc->packet_rx, fdc->packet_rx_cnt - 2);
        unsigned short rx_crc;
        rx_crc = fdc->packet_rx[fdc->packet_rx_cnt - 1] << 8;
        rx_crc |= fdc->packet_rx[fdc->packet_rx_cnt - 2];

        if (rx_crc == calc_crc)
        {
            if ((std::size_t) fdc->packet_rx_cnt == size) {
                /* It is a single header field, remember it for later */
                memcpy(fdc->rx_header, fdc->packet_rx, 6);
            }

            /* callback */
            if (fdc->cb_rx)
            {
                unsigned char tmp[6];
                memcpy(tmp, fdc->packet_rx, 6);
                memcpy(fdc->packet_rx, fdc->packet_rx + 6, 6);
                memcpy(fdc->packet_rx + 6, tmp, 6);
                std::size_t size = fdc->packet_rx_cnt - 2;

                if (size < 12) {
                    size = 12;
                }

                fdc->cb_rx(fdc->cb_rx_state, fdc->packet_rx, size);
            }
        }

        fdc->packet_rx_cnt = 0;
    }
}

void freedv_data_channel_tx_frame(struct freedv_data_channel *fdc, unsigned char *data, std::size_t size, int *from_bit, int *bcast_bit, int *crc_bit, int *end_bits)
{
    *from_bit = 0;
    *bcast_bit = 0;
    *crc_bit = 0;

    if (!fdc->packet_tx_size) {
        fdc->packet_tx_cnt = 0;

        if (fdc->cb_tx) {
            fdc->packet_tx_size = FREEDV_DATA_CHANNEL_PACKET_MAX;
            fdc->cb_tx(fdc->cb_tx_state, fdc->packet_tx, &fdc->packet_tx_size);
        }
	if (!fdc->packet_tx_size) {
	    /* Nothing to send, insert a header frame */
	    memcpy(fdc->packet_tx, fdc->tx_header, size);
            if (size < 8) {
                *end_bits = fdc_crc4(fdc->tx_header, size);
                *crc_bit = 1;
                memcpy(data, fdc->tx_header, size);

                return;
            } else {
                fdc->packet_tx_size = size;
            }
	} else {
	    /* new packet */
	    unsigned short crc;
            unsigned char tmp[6];

            *from_bit = !memcmp(fdc->packet_tx + 6, fdc->tx_header, 6);
            *bcast_bit = !memcmp(fdc->packet_tx, fdc_header_bcast, 6);

            memcpy(tmp, fdc->packet_tx, 6);
	    memcpy(fdc->packet_tx, fdc->packet_tx + 6, 6);
	    memcpy(fdc->packet_tx + 6, tmp, 6);

            crc = fdc_crc(fdc->packet_tx, fdc->packet_tx_size);

	    fdc->packet_tx[fdc->packet_tx_size] = crc & 0xff;
	    fdc->packet_tx_size++;
	    fdc->packet_tx[fdc->packet_tx_size] = (crc >> 8) & 0xff;
	    fdc->packet_tx_size++;

	    if (*from_bit) {
		fdc->packet_tx_cnt = 6;
            } else {
                if (*bcast_bit) {
                    memcpy(fdc->packet_tx + 6, fdc->packet_tx, 6);
                }
            }
            if (*bcast_bit) {
                fdc->packet_tx_cnt += 6;
            }
	}
    }
    if (fdc->packet_tx_size) {
        std::size_t copy = fdc->packet_tx_size - fdc->packet_tx_cnt;

        if (copy > size) {
            copy = size;
	    *end_bits = 0;
        } else {
            *end_bits = copy;
            fdc->packet_tx_size = 0;
        }
        memcpy(data, fdc->packet_tx + fdc->packet_tx_cnt, copy);
        fdc->packet_tx_cnt += copy;
    }
}

void freedv_data_set_header(struct freedv_data_channel *fdc, unsigned char *header)
{
    unsigned short crc = fdc_crc(header, 6);

    memcpy(fdc->tx_header, header, 6);
    fdc->tx_header[6] = crc & 0xff;
    fdc->tx_header[7] = (crc >> 8) & 0xff;
}

int freedv_data_get_n_tx_frames(struct freedv_data_channel *fdc, std::size_t size)
{
    if (fdc->packet_tx_size == 0)
        return 0;
    /* packet will be send in 'size' byte frames */
    return (fdc->packet_tx_size - fdc->packet_tx_cnt + size-1) / size;
}

} // FreeDV
