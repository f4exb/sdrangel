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
#include <stdlib.h>
#include <libacars/list.h>          // la_list
#include <libacars/vstring.h>       // la_vstring
#include <libacars/dict.h>          // la_dict
#include <libacars/json.h>
#include "dumpvdl2.h"               // XCALLOC, XFREE
#include "tlv.h"

// Forward declarations
tlv_type_descriptor_t tlv_DEF_unknown_tag;
tlv_type_descriptor_t tlv_DEF_unparseable_tag;

static void tlv_tag_destroy(tlv_tag_t *t) {
	if(t == NULL) {
		return;
	}
	if(t->data != TLV_NO_VALUE_PTR && t->td != NULL) {
		if(t->td->destroy != NULL) {
			t->td->destroy(t->data);
		} else {
			XFREE(t->data);
		}
	}
	XFREE(t);
}

void tlv_list_destroy(la_list *p) {
	la_list_free_full(p, tlv_tag_destroy);
	p = NULL;
}

la_list *tlv_list_append(la_list *head, uint8_t typecode, tlv_type_descriptor_t *td, void *data) {
	NEW(tlv_tag_t, tag);
	tag->typecode = typecode;
	tag->td = td;
	tag->data = data;
	return la_list_append(head, tag);
}

tlv_tag_t *tlv_list_search(la_list const *ptr, uint8_t typecode) {
	for(; ptr != NULL; ptr = ptr->next) {
		if(ptr->data != NULL) {
			tlv_tag_t *tag = ptr->data;
			if(tag->typecode == typecode) {
				return tag;
			}
		}
	}
	return NULL;
}

la_list *tlv_single_tag_parse(uint8_t typecode, uint8_t *buf, size_t tag_len, la_dict const *tag_table, la_list *list) {
	ASSERT(buf != NULL);
	ASSERT(tag_table != NULL);

	tlv_type_descriptor_t *td = la_dict_search(tag_table, (int)typecode);
	if(td == NULL) {
		debug_print(D_PROTO, "Unknown type code %u\n", typecode);
		td = &tlv_DEF_unknown_tag;
	}
	ASSERT(td->parse != NULL);
	void *parsed = NULL;
reparse:
	parsed = td->parse(typecode, buf, tag_len);
	if(parsed == NULL) {
		td = &tlv_DEF_unparseable_tag;
		// tlv_unparseable_tag_parse() does not return NULL, so we don't expect a loop here
		goto reparse;
	}
	return tlv_list_append(list, typecode, td, parsed);
}

la_list *tlv_parse(uint8_t *buf, size_t len, la_dict const *tag_table, size_t len_octets) {
	ASSERT(buf != NULL);
	ASSERT(tag_table != NULL);
	la_list *head = NULL;
	uint8_t *ptr = buf;
	size_t tlv_min_tag_len = 1 + len_octets;    // type code + <len_octets> length field + empty data field
	size_t tag_len;
	while(len >= tlv_min_tag_len) {
		uint8_t typecode = *ptr;
		ptr++; len--;

		tag_len = (size_t)(*ptr);
		if(len_octets == 2) {
			tag_len = (tag_len << 8) | (size_t)ptr[1];
		}

		ptr += len_octets; len -= len_octets;
		if(tag_len > len) {
			debug_print(D_PROTO, "TLV param %02x truncated: tag_len=%zu buflen=%zu\n", typecode, tag_len, len);
			return NULL;
		} else if(UNLIKELY(tag_len == 0)) {
			debug_print(D_PROTO, "TLV param %02x: bad length 0\n", typecode);
			return NULL;
		}
		head = tlv_single_tag_parse(typecode, ptr, tag_len, tag_table, head);
		ptr += tag_len; len -= tag_len;
	}
	if(len > 0) {
		debug_print(D_PROTO, "Warning: %zu unparsed octets left at end of TLV list\n", len);
	}
	return head;
}

static void tlv_tag_output_text(tlv_tag_t const *t, void *ctx) {
	ASSERT(t);
	ASSERT(ctx);

	tlv_formatter_ctx_t *c = ctx;
	ASSERT(t->td != NULL);
	if(t->td->format_text != NULL) {
		if(t->data == TLV_NO_VALUE_PTR) {
			LA_ISPRINTF(c->vstr, c->indent, "%s\n", t->td->label);
		} else {
			t->td->format_text(ctx, t->td->label, t->data);
		}
	}
}

static void tlv_tag_output_json(tlv_tag_t const *t, void *ctx) {
	ASSERT(t);
	ASSERT(ctx);

	tlv_formatter_ctx_t *c = ctx;
	ASSERT(t->td != NULL);
	if(t->td->format_json != NULL) {
		la_json_object_start(c->vstr, NULL);
		la_json_append_string(c->vstr, "name", t->td->json_key);
		if(t->data == TLV_NO_VALUE_PTR) {
			la_json_object_start(c->vstr, "value");
			la_json_object_end(c->vstr);
		} else {
			t->td->format_json(c, "value", t->data);
		}
		la_json_object_end(c->vstr);
	}
}

void tlv_list_format_text(la_vstring *vstr, la_list *tlv_list, int indent) {
	ASSERT(vstr);
	ASSERT(indent >= 0);
	if(tlv_list == NULL) {
		return;
	}
	tlv_formatter_ctx_t ctx = {
		.vstr = vstr,
		.indent = indent
	};
	la_list_foreach(tlv_list, tlv_tag_output_text, &ctx);
}

void tlv_list_format_json(la_vstring *vstr, char const *key, la_list *tlv_list) {
	ASSERT(vstr);
	if(tlv_list == NULL) {
		return;
	}
	tlv_formatter_ctx_t ctx = {
		.vstr = vstr,
		.indent = 0
	};
	la_json_array_start(vstr, key);
	la_list_foreach(tlv_list, tlv_tag_output_json, &ctx);
	la_json_array_end(vstr);
}

// Parsers and formatters for common data types

TLV_PARSER(tlv_octet_string_parse) {
	UNUSED(typecode);
	return octet_string_new(buf, len);
}

TLV_FORMATTER(tlv_octet_string_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	octet_string_format_text(ctx->vstr, data, 0);
	EOL(ctx->vstr);
}

TLV_FORMATTER(tlv_octet_string_format_json) {
	octet_string_t const *ostring = data;
	la_json_append_octet_string(ctx->vstr, label, ostring->buf, ostring->len);
}

TLV_FORMATTER(tlv_octet_string_with_ascii_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	octet_string_with_ascii_format_text(ctx->vstr, data, 0);
	EOL(ctx->vstr);
}

TLV_FORMATTER(tlv_octet_string_as_ascii_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	octet_string_as_ascii_format_text(ctx->vstr, data, 0);
	EOL(ctx->vstr);
}

TLV_FORMATTER(tlv_octet_string_as_ascii_format_json) {
	octet_string_as_ascii_format_json(ctx->vstr, label, data);
}

TLV_FORMATTER(tlv_single_octet_format_text) {
	octet_string_t const *octet = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	// We expect this octet string to have a length of 1 - if this is the case,
	// print it in hex with 0x prefix. Otherwise print is as octet string
	// without the prefix, for brevity.
	if(LIKELY(octet->len == 1)) {
		la_vstring_append_sprintf(ctx->vstr, "0x");
	}
	octet_string_format_text(ctx->vstr, octet, 0);
	EOL(ctx->vstr);
}

TLV_PARSER(tlv_uint8_parse) {
	UNUSED(typecode);
	if(len < sizeof(uint8_t)) {
		return NULL;
	}
	NEW(uint32_t, ret);
	*ret = (uint32_t)buf[0];
	return ret;
}

TLV_PARSER(tlv_uint16_msbfirst_parse) {
	UNUSED(typecode);
	if(len < sizeof(uint16_t)) {
		return NULL;
	}
	NEW(uint32_t, ret);
	*ret = (uint32_t)(extract_uint16_msbfirst(buf));
	return ret;
}

TLV_PARSER(tlv_uint32_msbfirst_parse) {
	UNUSED(typecode);
	if(len < sizeof(uint32_t)) {
		return NULL;
	}
	NEW(uint32_t, ret);
	*ret = extract_uint32_msbfirst(buf);
	return ret;
}

TLV_FORMATTER(tlv_uint_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: %u\n",
			label, *(uint32_t *)data);
}

TLV_FORMATTER(tlv_uint_format_json) {
	la_json_append_int64(ctx->vstr, label, *(uint32_t *)data);
}

// No-op parser
// Can be used to skip over a TLV without outputting it
TLV_PARSER(tlv_parser_noop) {
	UNUSED(typecode);
	UNUSED(buf);
	UNUSED(len);
	// Have to return something free()'able to indicate a success
	return XCALLOC(1, 1);
}

typedef struct {
	uint8_t typecode;
	octet_string_t *data;
} tlv_unparsed_tag_t;

TLV_PARSER(tlv_unknown_tag_parse) {
	NEW(tlv_unparsed_tag_t, t);
	t->typecode = typecode;
	t->data = octet_string_new(buf, len);
	return t;
}

TLV_FORMATTER(tlv_unknown_tag_format_text) {
	UNUSED(label);
	tlv_unparsed_tag_t const *t = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "-- Unknown TLV (code: 0x%02x): ", t->typecode);
	octet_string_format_text(ctx->vstr, t->data, 0);
	EOL(ctx->vstr);
}

TLV_DESTRUCTOR(tlv_unknown_tag_destroy) {
	if(data == NULL) {
		return;
	}
	tlv_unparsed_tag_t *t = data;
	XFREE(t->data);
	XFREE(t);
}

tlv_type_descriptor_t tlv_DEF_unknown_tag = {
	.label = "Unknown tag",
	.json_key = NULL,
	.parse = tlv_unknown_tag_parse,
	.format_text = tlv_unknown_tag_format_text,
	.format_json = NULL,
	.destroy = tlv_unknown_tag_destroy
};

TLV_FORMATTER(tlv_unparseable_tag_format_text) {
	UNUSED(label);
	tlv_unparsed_tag_t const *t = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "-- Unparseable TLV (code: 0x%02x): ", t->typecode);
	octet_string_format_text(ctx->vstr, t->data, 0);
	EOL(ctx->vstr);
}

TLV_FORMATTER(tlv_unparseable_tag_format_json) {
	UNUSED(label);
	tlv_unparsed_tag_t const *t = data;
	la_json_object_start(ctx->vstr, label);
	la_json_append_int64(ctx->vstr, "typecode",  t->typecode);
	la_json_append_octet_string(ctx->vstr, "data", t->data->buf, t->data->len);
	la_json_object_end(ctx->vstr);
}

tlv_type_descriptor_t tlv_DEF_unparseable_tag = {
	.label = "Unparseable tag",
	.json_key = "__unparseable_tlv_tag",
	.parse = tlv_unknown_tag_parse,
	.format_text = tlv_unparseable_tag_format_text,
	.format_json = tlv_unparseable_tag_format_json,
	.destroy = tlv_unknown_tag_destroy
};
