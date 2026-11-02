/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2026 Tomasz Lemiech <szpajder@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include <libacars/libacars.h>  // la_proto_node
#include <libacars/list.h>      // la_list

enum xid_types {
	XID_CMD_LCR     =  1,
	XID_CMD_HO_REQ  =  2,
	GSIF            =  3,
	XID_CMD_LE      =  4,
	XID_CMD_HO_INIT =  6,
	XID_CMD_LPM     =  7,
	XID_RSP_LE      = 12,
	XID_RSP_LCR     = 13,
	XID_RSP_HO      = 14,
	XID_RSP_LPM     = 15
};


typedef struct {
	la_list *pub_params, *vdl_params;
	enum xid_types type;
	bool err;
} xid_msg_t;

// xid.c
la_proto_node *xid_parse(uint8_t cr, uint8_t pf, uint8_t *buf, uint32_t len, uint32_t *msg_type);
