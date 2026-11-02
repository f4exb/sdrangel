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

#ifndef _ASN1_UTIL_H
#define _ASN1_UTIL_H
#include <stdint.h>                 // uint8_t
#include <libacars/libacars.h>      // la_type_descriptor
#include <libacars/asn1-util.h>     // la_asn1_formatter_params
#include "asn1/constr_TYPE.h"       // asn_TYPE_descriptor_t

// A structure for storing decoded ASN.1 payloads in a la_proto_node
typedef struct {
	asn_TYPE_descriptor_t *type;
	void *data;
	la_asn1_formatter const *formatter_table_text;
	la_asn1_formatter const *formatter_table_json;
	size_t formatter_table_text_len;
	size_t formatter_table_json_len;
} asn1_pdu_t;

// asn1-util.c
int asn1_decode_as(asn_TYPE_descriptor_t *td, void **struct_ptr, uint8_t *buf, int size);
void asn1_pdu_format_text(la_vstring *vstr, void const *data, int indent);
void asn1_pdu_format_json(la_vstring *vstr, void const *data);
void asn1_pdu_destroy(void *data);

#endif // _ASN1_UTIL_H
