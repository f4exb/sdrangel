/*
 * Minimal stand-in for dumpvdl2's dumpvdl2.h, providing just the symbols the
 * vendored ICAO ATN decoder sources (icao.c, asn1-util.c, asn1-format-icao-*.c,
 * from https://github.com/szpajder/dumpvdl2, GPL-3.0) use. The originals live in
 * dumpvdl2/src/dumpvdl2.h and util.c; this cut-down version lets them build inside
 * the SDRangel ACARS demodulator plugin unmodified.
 */
#ifndef DUMPVDL2_COMPAT_H
#define DUMPVDL2_COMPAT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libacars/libacars.h>      // la_proto_node, la_type_descriptor
#include <libacars/vstring.h>       // la_vstring
#include <libacars/json.h>          // la_json_append_string
#include <libacars/dict.h>          // la_dict

#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x) (void)(x)
#define ASSERT(expr) ((void)0)
#define nop() ((void)0)

#ifdef __GNUC__
#define LIKELY(x)   (__builtin_expect(!!(x),1))
#define UNLIKELY(x) (__builtin_expect(!!(x),0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#define debug_print(...) ((void)0)
#define debug_print_buf_hex(...) ((void)0)

// Debug filter classes referenced by the vendored sources (debugging is compiled out)
#define D_PROTO 0
#define D_PROTO_DETAIL 0

#define XCALLOC(nmemb, size) calloc(nmemb, size)
#define XFREE(ptr) free(ptr)
#define NEW(type, x) type *x = (type *)calloc(1, sizeof(type))

#define EOL(x) la_vstring_append_sprintf((x), "%s", "\n")
#define SAFE_JSON_APPEND_STRING(v, n, val) \
    do { \
        if((val) != NULL) { \
            la_json_append_string((v), (n), (val)); \
        } \
    } while(0)

// Message type flags (a subset of dumpvdl2's, same values)
#define MSGFLT_SRC_GND              (1 <<  0)
#define MSGFLT_SRC_AIR              (1 <<  1)
#define MSGFLT_XID_NO_GSIF          (1 <<  7)
#define MSGFLT_XID_GSIF             (1 <<  8)
#define MSGFLT_CM                   (1 << 14)
#define MSGFLT_CPDLC                (1 << 15)
#define MSGFLT_ADSC                 (1 << 16)

// The vendored sources test these to decide whether to emit raw ASN.1 dumps;
// both are permanently off here
typedef struct {
    uint32_t debug_filter;
    bool dump_asn1;
} dumpvdl2_config_t;
extern dumpvdl2_config_t Config;

typedef struct {
    uint8_t *buf;
    size_t len;
} octet_string_t;

octet_string_t *octet_string_new(void *buf, size_t len);
void octet_string_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent);
void octet_string_with_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent);
la_proto_node *unknown_proto_pdu_new(void *buf, size_t len);
uint16_t extract_uint16_msbfirst(uint8_t const *data);
uint32_t extract_uint32_msbfirst(uint8_t const *data);
void octet_string_as_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent);
void octet_string_as_ascii_format_json(la_vstring *vstr, char const *key, octet_string_t const *ostring);
void bitfield_format_text(la_vstring *vstr, uint8_t const *buf, size_t len, la_dict const *d);
void bitfield_format_json(la_vstring *vstr, uint8_t const *buf, size_t len, la_dict const *d, char const *key);
uint32_t parse_dlc_addr(uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif // DUMPVDL2_COMPAT_H
