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
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <libacars/libacars.h>      // la_proto_node, la_proto_node_new()
#include <libacars/vstring.h>       // la_vstring
#include <libacars/dict.h>          // la_dict
#include <libacars/json.h>
#include "config.h"                 // IS_BIG_ENDIAN
#include "dumpvdl2.h"               // la_dict_search()
#include "tlv.h"
#include "avlc.h"                   // avlc_addr_t
#include "xid.h"

/***************************************************************************
 * Forward declarations
 **************************************************************************/
la_type_descriptor const proto_DEF_XID_msg;

#define XID_FMT_ID              0x82
#define XID_GID_PUBLIC          0x80
#define XID_GID_PRIVATE         0xF0
#define XID_MIN_GROUPLEN        3                   // group_id + group_len (0)
#define XID_MIN_LEN (1 + 2 * XID_MIN_GROUPLEN)      // XID fmt + empty pub group + empty priv group
#define XID_PARAM_CONN_MGMT     1

struct xid_descr {
	char *name;
	char *description;
};

// list indexed with a bitfield consisting of:
// 4. C/R bit value
// 3. P/F bit value
// 2. connection mgmt parameter h bit value
// 1. connection mgmt parameter r bit value
// GSIF, XID_CMD_LPM and XID_RSP_LPM messages do not contain
// connection mgmt parameter - h and r are forced to 1 in this case
// Reference: ICAO 9776, Table 5.12
static struct xid_descr const xid_names[16] = {
	{ "",               "" },
	{ "XID_CMD_LCR",    "Link Connection Refused" },
	{ "XID_CMD_HO",     "Handoff Request / Broadcast Handoff" },
	{ "GSIF",           "Ground Station Information Frame" },
	{ "XID_CMD_LE",     "Link Establishment" },
	{ "",               "" },
	{ "XID_CMD_HO",     "Handoff Initiation" },
	{ "XID_CMD_LPM",    "Link Parameter Modification" },
	{ "",               "" },
	{ "",               "" },
	{ "",               "" },
	{ "",               "" },
	{ "XID_RSP_LE",     "Link Establishment Response" },
	{ "XID_RSP_LCR",    "Link Connection Refused Response" },
	{ "XID_RSP_HO",     "Handoff Response" },
	{ "XID_RSP_LPM",    "Link Parameter Modification Response" }
};

/***************************************************************************
 * Parsers and formatters for XID parameters
 **************************************************************************/

/***************************************************************************
 * Connection management
 **************************************************************************/

typedef union {
	uint8_t val;
	struct {
#ifdef IS_BIG_ENDIAN
		uint8_t pad:4;
		uint8_t v:1;
		uint8_t x:1;
		uint8_t r:1;
		uint8_t h:1;
#else
		uint8_t h:1;
		uint8_t r:1;
		uint8_t x:1;
		uint8_t v:1;
		uint8_t pad:4;
#endif
	} bits;
} conn_mgmt_t;

TLV_PARSER(conn_mgmt_parse) {
	UNUSED(typecode);
	if(len < 1) {
		return NULL;
	}
	NEW(conn_mgmt_t, ret);
	ret->val = buf[0];
	return ret;
}

TLV_FORMATTER(conn_mgmt_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	conn_mgmt_t const *c = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: %02x\n", label, c->val);
}

/***************************************************************************
 * XID sequencing
 **************************************************************************/

TLV_FORMATTER(xid_seq_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	uint8_t const *xidseq = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: seq: %u retry: %u\n",
			label, *xidseq & 0x7, *xidseq >> 4);
}

TLV_FORMATTER(xid_seq_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);

	uint8_t const *xidseq = data;
	la_json_object_start(ctx->vstr, label);
	la_json_append_int64(ctx->vstr, "seq", *xidseq & 0x7);
	la_json_append_int64(ctx->vstr, "retry", *xidseq >> 4);
	la_json_object_end(ctx->vstr);
}

/***************************************************************************
 * Frequency, modulation
 **************************************************************************/

static la_dict const modulations[] = {
	{ .id = 2, .val = "VDL-M2, D8PSK, 31500 bps" },
	{ .id = 4, .val = "VDL-M3, D8PSK, 31500 bps" },
	{ .id = 0, .val = NULL }
};

TLV_FORMATTER(modulation_support_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);
	uint32_t const *val = data;
	uint8_t mods_val = (uint8_t)(*val & 0xff);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	bitfield_format_text(ctx->vstr, &mods_val, 1, modulations);
	EOL(ctx->vstr);
}

TLV_FORMATTER(modulation_support_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);

	uint32_t const *val = data;
	uint8_t mods_val = (uint8_t)(*val & 0xff);
	bitfield_format_json(ctx->vstr, &mods_val, 1, modulations, label);
}

typedef struct {
	uint8_t modulations;
	float frequency;
} vdl2_frequency_t;

vdl2_frequency_t parse_freq(uint8_t const *buf) {
	uint8_t modulations = buf[0] >> 4;
	uint16_t freq = extract_uint16_msbfirst(buf) & 0x0fff;
	uint32_t freq_khz = (freq + 10000) * 10;
	if(freq_khz % 25 != 0) {
		freq_khz = freq_khz + 25 - freq_khz % 25;
	}
	float frequency = (float)freq_khz / 1000.f;
	return (vdl2_frequency_t){
		.modulations = modulations,
			.frequency = frequency
	};
}

TLV_PARSER(vdl2_frequency_parse) {
	UNUSED(typecode);
	if(len < 2) {
		return NULL;
	}
	NEW(vdl2_frequency_t, f);
	*f = parse_freq(buf);
	return f;
}

static void append_frequency_as_text(vdl2_frequency_t const *f, la_vstring *vstr) {
	ASSERT(vstr != NULL);
	ASSERT(f != NULL);
	la_vstring_append_sprintf(vstr, "%.3f MHz (", f->frequency);
	bitfield_format_text(vstr, &f->modulations, 1, modulations);
	la_vstring_append_sprintf(vstr, "%s", ")");
}

TLV_FORMATTER(vdl2_frequency_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	vdl2_frequency_t const *f = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	append_frequency_as_text(f, ctx->vstr);
	EOL(ctx->vstr);
}

TLV_FORMATTER(vdl2_frequency_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);

	vdl2_frequency_t const *f = data;
	la_json_object_start(ctx->vstr, label);
	la_json_append_double(ctx->vstr, "freq_mhz", f->frequency);
	bitfield_format_json(ctx->vstr, &f->modulations, 1, modulations, "modulation_support");
	la_json_object_end(ctx->vstr);
}

/***************************************************************************
 * DLC addresses
 **************************************************************************/

TLV_PARSER(dlc_addr_list_parse) {
	UNUSED(typecode);
	if(len % 4 != 0) {
		return NULL;
	}
	la_list *addr_list = NULL;
	while(len > 0) {
		NEW(avlc_addr_t, addr);
		addr->val = parse_dlc_addr(buf);
		addr_list = la_list_append(addr_list, addr);
		buf += 4; len -= 4;
	}
	return addr_list;
}

// Appends anonymous DLC address to the current line.
// Executed via la_list_foreach().
static void append_dlc_addr_as_text(void const *data, void *ctx) {
	ASSERT(data != NULL);
	ASSERT(ctx != NULL);
	la_vstring *vstr = ctx;
	avlc_addr_t const *a = data;
	la_vstring_append_sprintf(vstr, " %06X", a->a_addr.addr);
}

TLV_FORMATTER(dlc_addr_list_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	la_list const *addr_list = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:", label);
	la_list_foreach((la_list *)addr_list, append_dlc_addr_as_text, ctx->vstr);
	EOL(ctx->vstr);
}

TLV_FORMATTER(dlc_addr_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	avlc_addr_t const *a = data;
	char addr_str[7];
	sprintf(addr_str, "%06X", a->a_addr.addr);
	la_json_append_string(ctx->vstr, label, addr_str);
}

// Appends anonymous DLC address to the JSON list.
// Executed via la_list_foreach().
static void append_dlc_addr_as_json(void const *data, void *ctx) {
	ASSERT(data != NULL);
	ASSERT(ctx != NULL);
	la_vstring *vstr = ctx;
	dlc_addr_format_json(&(tlv_formatter_ctx_t){ .vstr = vstr }, NULL, data);
}

TLV_FORMATTER(dlc_addr_list_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);

	la_list const *addr_list = data;
	la_json_array_start(ctx->vstr, label);
	la_list_foreach((la_list *)addr_list, append_dlc_addr_as_json, ctx->vstr);
	la_json_array_end(ctx->vstr);
}

TLV_DESTRUCTOR(dlc_list_destroy) {
	la_list_free(data);
}

/***************************************************************************
 * Frequency support list
 **************************************************************************/

typedef struct {
	vdl2_frequency_t freq;
	avlc_addr_t gs_addr;
} freq_support_t;

TLV_PARSER(freq_support_list_parse) {
	UNUSED(typecode);
	if(len % 6 != 0) {
		return NULL;
	}

	la_list *fslist = NULL;
	while(len > 0) {
		NEW(freq_support_t, fs);
		fs->freq = parse_freq(buf);
		buf += 2; len -= 2;
		fs->gs_addr.val = parse_dlc_addr(buf);
		buf += 4; len -= 4;
		fslist = la_list_append(fslist, fs);
	}
	return fslist;
}

// Appends frequency support list entry.
// Executed via la_list_foreach().
static void fs_entry_format_text(void const *data, void *ctx_ptr) {
	ASSERT(data != NULL);
	ASSERT(ctx_ptr != NULL);
	tlv_formatter_ctx_t *ctx = ctx_ptr;
	freq_support_t const *fs = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:", "Ground station");
	append_dlc_addr_as_text(&fs->gs_addr, ctx->vstr);
	EOL(ctx->vstr);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "%s: ", "Frequency");
	append_frequency_as_text(&fs->freq, ctx->vstr);
	EOL(ctx->vstr);
}

TLV_FORMATTER(freq_support_list_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	la_list const *fslist = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	ctx->indent++;
	la_list_foreach((la_list *)fslist, fs_entry_format_text, ctx);
	ctx->indent--;
}

static void fs_entry_format_json(void const *data, void *ctx_ptr) {
	ASSERT(data != NULL);
	ASSERT(ctx_ptr != NULL);
	tlv_formatter_ctx_t *ctx = ctx_ptr;
	freq_support_t const *fs = data;
	la_json_object_start(ctx->vstr, NULL);
	dlc_addr_format_json(ctx, "gs_addr", &fs->gs_addr);
	vdl2_frequency_format_json(ctx, "gs_freq", &fs->freq);
	la_json_object_end(ctx->vstr);
}

TLV_FORMATTER(freq_support_list_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);

	la_list const *fslist = data;
	la_json_array_start(ctx->vstr, label);
	la_list_foreach((la_list *)fslist, fs_entry_format_json, ctx);
	la_json_array_end(ctx->vstr);
}

TLV_DESTRUCTOR(freq_support_list_destroy) {
	la_list_free(data);
}

/***************************************************************************
 * LCR cause
 **************************************************************************/

typedef struct {
	octet_string_t additional_data;
	uint16_t delay;
	uint8_t cause;
} lcr_cause_t;

TLV_PARSER(lcr_cause_parse) {
	UNUSED(typecode);
	if(len < 3) {
		return NULL;
	}
	NEW(lcr_cause_t, c);
	c->cause = buf[0];
	c->delay = extract_uint16_msbfirst(buf + 1);
	buf += 3; len -= 3;
	if(len > 0) {
		c->additional_data.buf = buf;
		c->additional_data.len = len;
	}
	return c;
}

static la_dict const lcr_causes[] = {
	{ .id = 0x00, .val = "Bad local parameter" },
	{ .id = 0x01, .val = "Out of link layer resources" },
	{ .id = 0x02, .val = "Out of packet layer resources" },
	{ .id = 0x03, .val = "Terrestrial network not available" },
	{ .id = 0x04, .val = "Terrestrial network congestion" },
	{ .id = 0x05, .val = "Cannot support autotune" },
	{ .id = 0x06, .val = "Station cannot support initiating handoff" },
	{ .id = 0x7f, .val = "Other unspecified local reason" },
	{ .id = 0x80, .val = "Bad global parameter" },
	{ .id = 0x81, .val = "Protocol violation" },
	{ .id = 0x82, .val = "Ground system out of resources" },
	{ .id = 0xff, .val = "Other unspecified system reason" },
	{ .id = 0x00, .val = NULL }
};

TLV_FORMATTER(lcr_cause_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	lcr_cause_t const *c = data;
	char const *cause_descr = la_dict_search(lcr_causes, c->cause);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: 0x%02x (%s)\n",
			label, c->cause, cause_descr ? cause_descr : "unknown");
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Delay: %u\n", c->delay);
	if(c->additional_data.buf != NULL) {
		LA_ISPRINTF(ctx->vstr, ctx->indent+1, "%s: ", "Additional data");
		octet_string_format_text(ctx->vstr, &c->additional_data, 0);
		EOL(ctx->vstr);
	}
}

TLV_FORMATTER(lcr_cause_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);

	lcr_cause_t const *c = data;
	char const *cause_descr = la_dict_search(lcr_causes, c->cause);
	la_json_object_start(ctx->vstr, label);
	la_json_append_int64(ctx->vstr, "cause_code", c->cause);
	if(cause_descr != NULL) {
		la_json_append_string(ctx->vstr, "cause_descr", cause_descr);
	}
	la_json_append_int64(ctx->vstr, "delay", c->delay);
	if(c->additional_data.buf != NULL) {
		la_json_append_octet_string(ctx->vstr, "additional_data", c->additional_data.buf, c->additional_data.len);
	}
	la_json_object_end(ctx->vstr);
}

/***************************************************************************
 * Ground station location
 **************************************************************************/

typedef struct {
	float lat, lon;
} location_t;

static location_t loc_parse(uint8_t *buf) {
	struct { signed int coord:12; } s;
	int lat = s.coord = (int)(extract_uint16_msbfirst(buf) >> 4);
	int lon = s.coord = (int)(extract_uint16_msbfirst(buf + 1) & 0xfff);
	debug_print(D_PROTO_DETAIL, "lat: %d lon: %d\n", lat, lon);
	float latf = (float)lat / 10.0f;
	float lonf = (float)lon / 10.0f;
	return (location_t){ .lat = latf, .lon = lonf };
}

TLV_PARSER(location_parse) {
	UNUSED(typecode);
	if(len < 3) {
		return NULL;
	}
	NEW(location_t, loc);
	*loc = loc_parse(buf);
	return loc;
}

static void append_location_as_text(la_vstring *vstr, location_t loc) {
	ASSERT(vstr != NULL);
	char ns = 'N';
	char we = 'E';
	if(loc.lat < 0.f) {
		loc.lat = -loc.lat;
		ns = 'S';
	}
	if(loc.lon < 0.f) {
		loc.lon = -loc.lon;
		we = 'W';
	}
	la_vstring_append_sprintf(vstr, "%.1f%c %.1f%c", loc.lat, ns, loc.lon, we);
}

TLV_FORMATTER(location_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	location_t const *loc = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	append_location_as_text(ctx->vstr, *loc);
	EOL(ctx->vstr);
}

TLV_FORMATTER(location_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	location_t const *loc = data;
	la_json_object_start(ctx->vstr, label);
	la_json_append_double(ctx->vstr, "lat", loc->lat);
	la_json_append_double(ctx->vstr, "lon", loc->lon);
	la_json_object_end(ctx->vstr);
}

/***************************************************************************
 * Aicraft location
 **************************************************************************/

typedef struct {
	location_t loc;
	int alt;
} loc_alt_t;

TLV_PARSER(loc_alt_parse) {
	UNUSED(typecode);
	if(len < 4) {
		return NULL;
	}
	NEW(loc_alt_t, la);
	la->loc = loc_parse(buf);
	la->alt = (int)buf[3] * 1000;
	return la;
}

TLV_FORMATTER(loc_alt_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	loc_alt_t const *la = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	append_location_as_text(ctx->vstr, la->loc);
	la_vstring_append_sprintf(ctx->vstr, " %d ft\n", la->alt);
}

TLV_FORMATTER(loc_alt_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	loc_alt_t const *la = data;
	la_json_object_start(ctx->vstr, label);
	location_format_json(ctx, "loc", &la->loc);
	la_json_append_int64(ctx->vstr, "alt", la->alt);
	la_json_object_end(ctx->vstr);
}

/***************************************************************************
 * Public XID parameters
 **************************************************************************/

static la_dict const xid_pub_params[] = {
	{
		.id = 0x1,
		.val = &(tlv_type_descriptor_t){
			.label = "Parameter set ID",
			.json_key = "param_set_id",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_as_ascii_format_text,
			.format_json = tlv_octet_string_as_ascii_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x2,
		.val = &(tlv_type_descriptor_t){
			.label = "Procedure classes",
			.json_key = "procedure_classes",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x3,
		.val = &(tlv_type_descriptor_t){
			.label = "HDLC options",
			.json_key = "hdlc_options",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x5,
		.val = &(tlv_type_descriptor_t){
			.label = "N1-downlink",
			.json_key = "n1_downlink",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x6,
		.val = &(tlv_type_descriptor_t){
			.label = "N1-uplink",
			.json_key = "n1_uplink",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x7,
		.val = &(tlv_type_descriptor_t){
			.label = "k-downlink",
			.json_key = "k_downlink",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x8,
		.val = &(tlv_type_descriptor_t){
			.label = "k-uplink",
			.json_key = "k_uplink",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x9,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer T1_downlink",
			.json_key = "timer_t1_downlink",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xA,
		.val = &(tlv_type_descriptor_t){
			.label = "Counter N2",
			.json_key = "counter_n2",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xB,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer T2",
			.json_key = "timer_t2",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xFF,
		.val = NULL
	}
};

/***************************************************************************
 * VDL2-specific XID parameters
 **************************************************************************/

static la_dict const xid_vdl_params[] = {
	{
		.id = 0x00,
		.val = &(tlv_type_descriptor_t){
			.label = "Parameter set ID",
			.json_key = "param_set_id",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_as_ascii_format_text,
			.format_json = tlv_octet_string_as_ascii_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x01,
		.val = &(tlv_type_descriptor_t){
			.label = "Connection management",
			.json_key = "conn_mgmt",
			.parse = conn_mgmt_parse,
			.format_text = conn_mgmt_format_text,
			.format_json = tlv_uint_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x02,
		.val = &(tlv_type_descriptor_t){
			.label = "SQP",
			.json_key = "sqp",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x03,
		.val = &(tlv_type_descriptor_t){
			.label = "XID sequencing",
			.json_key = "xid_sequencing",
			.parse = tlv_uint8_parse,
			.format_text = xid_seq_format_text,
			.format_json = xid_seq_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x04,
		.val = &(tlv_type_descriptor_t){
			.label = "AVLC specific options",
			.json_key = "avlc_specific_options",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x05,
		.val = &(tlv_type_descriptor_t){
			.label = "Expedited SN connection",
			.json_key = "expedited_sn_connection",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x06,
		.val = &(tlv_type_descriptor_t){
			.label = "LCR cause",
			.json_key = "lcr_cause",
			.parse = lcr_cause_parse,
			.format_text = lcr_cause_format_text,
			.format_json = lcr_cause_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x81,
		.val = &(tlv_type_descriptor_t){
			.label = "Modulation support",
			.json_key = "modulation_support",
			.parse = tlv_uint8_parse,
			.format_text = modulation_support_format_text,
			.format_json = modulation_support_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x82,
		.val = &(tlv_type_descriptor_t){
			.label = "Alternate ground stations",
			.json_key = "alternate_ground_stations",
			.parse = dlc_addr_list_parse,
			.format_text = dlc_addr_list_format_text,
			.format_json = dlc_addr_list_format_json,
			.destroy = dlc_list_destroy
		}
	},
	{
		.id = 0x83,
		.val = &(tlv_type_descriptor_t){
			.label = "Destination airport",
			.json_key = "dst_airport",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_as_ascii_format_text,
			.format_json = tlv_octet_string_as_ascii_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x84,
		.val = &(tlv_type_descriptor_t){
			.label = "Aircraft location",
			.json_key = "ac_location",
			.parse = loc_alt_parse,
			.format_text = loc_alt_format_text,
			.format_json = loc_alt_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x40,
		.val = &(tlv_type_descriptor_t){
			.label = "Autotune frequency",
			.json_key = "autotune_freq",
			.parse = vdl2_frequency_parse,
			.format_text = vdl2_frequency_format_text,
			.format_json = vdl2_frequency_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x41,
		.val = &(tlv_type_descriptor_t){
			.label = "Replacement ground stations",
			.json_key = "replacement_ground_stations",
			.parse = dlc_addr_list_parse,
			.format_text = dlc_addr_list_format_text,
			.format_json = dlc_addr_list_format_json,
			.destroy = dlc_list_destroy
		}
	},
	{
		.id = 0x42,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer T4",
			.json_key = "timer_t4",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x43,
		.val = &(tlv_type_descriptor_t){
			.label = "MAC persistence",
			.json_key = "mac_persistence",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x44,
		.val = &(tlv_type_descriptor_t){
			.label = "Counter M1",
			.json_key = "counter_m1",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x45,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer TM2",
			.json_key = "timer_tm2",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x46,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer TG5",
			.json_key = "timer_tg5",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x47,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer T3min",
			.json_key = "timer_t3min",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0x48,
		.val = &(tlv_type_descriptor_t){
			.label = "Ground station address filter",
			.json_key = "gs_addr_filter",
			.parse = dlc_addr_list_parse,
			.format_text = dlc_addr_list_format_text,
			.format_json = dlc_addr_list_format_json,
			.destroy = dlc_list_destroy
		}
	},
	{
		.id = 0x49,
		.val = &(tlv_type_descriptor_t){
			.label = "Broadcast connection",
			.json_key = "broadcast_connection",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xC0,
		.val = &(tlv_type_descriptor_t){
			.label = "Frequency support list",
			.json_key = "freq_support_list",
			.parse = freq_support_list_parse,
			.format_text = freq_support_list_format_text,
			.format_json = freq_support_list_format_json,
			.destroy = freq_support_list_destroy
		}
	},
	{
		.id = 0xC1,
		.val = &(tlv_type_descriptor_t){
			.label = "Airport coverage",
			.json_key = "airport_coverage",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_as_ascii_format_text,
			.format_json = tlv_octet_string_as_ascii_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xC3,
		.val = &(tlv_type_descriptor_t){
			.label = "Nearest airport ID",
			.json_key = "nearest_airport_id",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_as_ascii_format_text,
			.format_json = tlv_octet_string_as_ascii_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xC4,
		.val = &(tlv_type_descriptor_t){
			.label = "ATN router NETs",
			.json_key = "atn_router_nets",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_with_ascii_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xC5,
		.val = &(tlv_type_descriptor_t){
			.label = "System mask",
			.json_key = "system_mask",
			.parse = dlc_addr_list_parse,
			.format_text = dlc_addr_list_format_text,
			.format_json = dlc_addr_list_format_json,
			.destroy = dlc_list_destroy
		}
	},
	{
		.id = 0xC6,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer TG3",
			.json_key = "timer_tg3",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xC7,
		.val = &(tlv_type_descriptor_t){
			.label = "Timer TG4",
			.json_key = "timer_tg4",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xC8,
		.val = &(tlv_type_descriptor_t){
			.label = "Ground station location",
			.json_key = "gs_location",
			.parse = location_parse,
			.format_text = location_format_text,
			.format_json = location_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xFF,
		.val = NULL
	}
};

/***************************************************************************
 * Main XID parsing routine
 **************************************************************************/

la_proto_node *xid_parse(uint8_t cr, uint8_t pf, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	NEW(xid_msg_t, msg);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_XID_msg;
	node->data = msg;
	node->next = NULL;

	msg->err = true;    // fail-safe default
	if(len < XID_MIN_LEN) {
		debug_print(D_PROTO, "XID too short\n");
		goto end;
	}
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	if(ptr[0] != XID_FMT_ID) {
		debug_print(D_PROTO, "Unknown XID format\n");
		goto end;
	}
	ptr++; remaining--;
	while(remaining >= XID_MIN_GROUPLEN) {
		uint8_t gid = *ptr;
		ptr++; remaining--;
		uint16_t grouplen = extract_uint16_msbfirst(ptr);
		ptr += 2; remaining -= 2;
		if(grouplen > len) {
			debug_print(D_PROTO, "XID group %02x truncated: grouplen=%u buflen=%u\n", gid,
					grouplen, remaining);
			goto end;
		}
		switch(gid) {
			case XID_GID_PUBLIC:
				if(msg->pub_params != NULL) {
					debug_print(D_PROTO, "Duplicate XID group 0x%02x\n", XID_GID_PUBLIC);
					goto end;
				}
				msg->pub_params = tlv_parse(ptr, grouplen, xid_pub_params, 1);
				break;
			case XID_GID_PRIVATE:
				if(msg->vdl_params != NULL) {
					debug_print(D_PROTO, "Duplicate XID group 0x%02x\n", XID_GID_PRIVATE);
					goto end;
				}
				msg->vdl_params = tlv_parse(ptr, grouplen, xid_vdl_params, 1);
				break;
			default:
				debug_print(D_PROTO, "Unknown XID Group ID 0x%x, ignored\n", gid);
		}
		ptr += grouplen; remaining -= grouplen;
	}
	// pub_params are optional, vdl_params are mandatory
	if(msg->vdl_params == NULL) {
		debug_print(D_PROTO, "Incomplete XID message\n");
		goto end;
	}
	if(remaining > 0) {
		debug_print(D_PROTO, "Warning: %u unparsed octets left at end of XID message\n", remaining);
		node->next = unknown_proto_pdu_new(ptr, remaining);
	}
	// find connection management parameter to figure out the XID type
	conn_mgmt_t cm = {
		.val = 0xff // default dummy value
	};
	tlv_tag_t *tmp = tlv_list_search(msg->vdl_params, XID_PARAM_CONN_MGMT);
	if(tmp != NULL) {
		cm = *(conn_mgmt_t *)(tmp->data);
	}
	msg->type = ((cr & 0x1) << 3) | ((pf & 0x1) << 2) | (cm.bits.h << 1) | cm.bits.r;
	if(msg->type == GSIF) {
		*msg_type |= MSGFLT_XID_GSIF;
	} else {
		*msg_type |= MSGFLT_XID_NO_GSIF;
	}
	msg->err = false;
	return node;
end:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

/***************************************************************************
 * XID formatters
 **************************************************************************/

void xid_format_text(la_vstring *vstr, void const *data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	xid_msg_t const *msg = data;
	if(msg->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable XID\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "XID: %s\n", xid_names[msg->type].description);
	indent++;
	// pub_params are optional, vdl_params are mandatory
	if(msg->pub_params) {
		LA_ISPRINTF(vstr, indent, "%s", "Public params:\n");
		tlv_list_format_text(vstr, msg->pub_params, indent+1);
	}
	LA_ISPRINTF(vstr, indent, "%s", "VDL params:\n");
	tlv_list_format_text(vstr, msg->vdl_params, indent+1);
}

void xid_format_json(la_vstring *vstr, void const *data) {
	ASSERT(vstr != NULL);
	ASSERT(data);

	xid_msg_t const *msg = data;
	la_json_append_bool(vstr, "err", msg->err);
	if(msg->err == true) {
		return;
	}
	la_json_append_string(vstr, "type", xid_names[msg->type].name);
	la_json_append_string(vstr, "type_descr", xid_names[msg->type].description);
	if(msg->pub_params) {
		tlv_list_format_json(vstr, "pub_params", msg->pub_params);
	}
	tlv_list_format_json(vstr, "vdl_params", msg->vdl_params);
}

/***************************************************************************
 * Destructors
 **************************************************************************/

void xid_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	xid_msg_t *msg = data;
	tlv_list_destroy(msg->pub_params);
	tlv_list_destroy(msg->vdl_params);
	msg->pub_params = msg->vdl_params = NULL;
	XFREE(data);
}

/***************************************************************************
 * Type descriptors
 **************************************************************************/

la_type_descriptor const proto_DEF_XID_msg = {
	.format_text = xid_format_text,
	.format_json = xid_format_json,
	.json_key = "xid",
	.destroy = xid_destroy
};
