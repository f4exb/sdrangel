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
#ifndef _TLV_H
#define _TLV_H 1
#include <stdio.h>
#include <stdint.h>
#include <libacars/list.h>       // la_list
#include <libacars/vstring.h>    // la_vstring
#include <libacars/dict.h>       // la_dict

typedef struct {
	la_vstring *vstr;
	int indent;
} tlv_formatter_ctx_t;

typedef void *(tlv_parser_f)(uint8_t typecode, uint8_t *buf, size_t len);
typedef void(tlv_formatter_f)(tlv_formatter_ctx_t *ctx, char const *label, void const *data);
typedef void(tlv_destructor_f)(void *data);

#define TLV_PARSER(x) void *(x)(uint8_t typecode, uint8_t *buf, size_t len)
#define TLV_FORMATTER(x) void (x)(tlv_formatter_ctx_t *ctx, char const *label, void const *data)
#define TLV_DESTRUCTOR(x) void (x)(void *data)

typedef struct {
	char const *label;
	char const *json_key;
	tlv_parser_f *parse;
	tlv_formatter_f *format_text;
	tlv_formatter_f *format_json;
	tlv_destructor_f *destroy;
} tlv_type_descriptor_t;

// generic tag structure
typedef struct {
	uint8_t typecode;
	tlv_type_descriptor_t *td;
	void *data;
} tlv_tag_t;

// Special value for tlv_tag_t.data to indicate a tag without a value
#define TLV_NO_VALUE_PTR (void *)(~0)

// tlv.c
// Generic TLV API
la_list *tlv_parse(uint8_t *buf, size_t len, la_dict const *tag_table, size_t len_octets);
la_list *tlv_single_tag_parse(uint8_t typecode, uint8_t *buf, size_t tag_len, la_dict const *tag_table, la_list *list);
tlv_tag_t *tlv_list_search(la_list const *ptr, uint8_t typecode);
la_list *tlv_list_append(la_list *head, uint8_t typecode, tlv_type_descriptor_t *td, void *data);
void tlv_list_format_text(la_vstring *vstr, la_list *tlv_list, int indent);
void tlv_list_format_json(la_vstring *vstr, char const *key, la_list *tlv_list);
void tlv_list_destroy(la_list *p);

// Parsers and formatters for common data types
TLV_PARSER(tlv_octet_string_parse);
TLV_FORMATTER(tlv_octet_string_format_text);

TLV_PARSER(tlv_uint8_parse);
TLV_PARSER(tlv_uint16_msbfirst_parse);
TLV_PARSER(tlv_uint32_msbfirst_parse);
TLV_PARSER(tlv_parser_noop);
TLV_FORMATTER(tlv_uint_format_text);
TLV_FORMATTER(tlv_uint_format_json);
TLV_FORMATTER(tlv_octet_string_as_ascii_format_text);
TLV_FORMATTER(tlv_octet_string_as_ascii_format_json);
TLV_FORMATTER(tlv_octet_string_with_ascii_format_text);
TLV_FORMATTER(tlv_single_octet_format_text);
// tlv_single_octet_format_json is handled by tlv_octet_string_format_json
TLV_FORMATTER(tlv_octet_string_format_json);
#endif // !_TLV_H
