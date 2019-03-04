/*---------------------------------------------------------------------------*\

  FILE........: freedv_data_channel.h
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

#ifndef _FREEDV_DATA_CHANNEL_H
#define _FREEDV_DATA_CHANNEL_H

#include <stdlib.h>

#define FREEDV_DATA_CHANNEL_PACKET_MAX 2048

namespace FreeDV
{

typedef void (*freedv_data_callback_rx)(void *, unsigned char *packet, size_t size);
typedef void (*freedv_data_callback_tx)(void *, unsigned char *packet, size_t *size);

struct freedv_data_channel {
    freedv_data_callback_rx cb_rx;
    void *cb_rx_state;
     freedv_data_callback_tx cb_tx;
    void *cb_tx_state;

    unsigned char rx_header[8];
    unsigned char packet_rx[FREEDV_DATA_CHANNEL_PACKET_MAX + 2];
    int packet_rx_cnt;

    unsigned char tx_header[8];
    unsigned char packet_tx[FREEDV_DATA_CHANNEL_PACKET_MAX + 2];
    int packet_tx_cnt;
    size_t packet_tx_size;
};


struct freedv_data_channel *freedv_data_channel_create(void);
void freedv_data_channel_destroy(struct freedv_data_channel *fdc);

void freedv_data_set_cb_rx(struct freedv_data_channel *fdc, freedv_data_callback_rx cb, void *state);
void freedv_data_set_cb_tx(struct freedv_data_channel *fdc, freedv_data_callback_tx cb, void *state);

void freedv_data_channel_rx_frame(struct freedv_data_channel *fdc, unsigned char *data, size_t size, int from_bit, int bcast_bit, int crc_bit, int end_bits);
void freedv_data_channel_tx_frame(struct freedv_data_channel *fdc, unsigned char *data, size_t size, int *from_bit, int *bcast_bit, int *crc_bit, int *end_bits);

void freedv_data_set_header(struct freedv_data_channel *fdc, unsigned char *header);
int freedv_data_get_n_tx_frames(struct freedv_data_channel *fdc, size_t size);

} // FreeDV

#endif /* _FREEDV_DATA_CHANNEL_H */
