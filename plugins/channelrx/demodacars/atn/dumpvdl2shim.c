/*
 * Implementations for the dumpvdl2 compatibility layer in dumpvdl2.h: the handful of
 * utility functions the vendored ICAO ATN decoder sources call, ported from
 * dumpvdl2/src/util.c (https://github.com/szpajder/dumpvdl2, GPL-3.0,
 * Copyright (c) 2017-2026 Tomasz Lemiech <szpajder@gmail.com>).
 */
#include "dumpvdl2.h"

dumpvdl2_config_t Config = { 0, false };

uint16_t extract_uint16_msbfirst(uint8_t const *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

uint32_t extract_uint32_msbfirst(uint8_t const *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
         | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

octet_string_t *octet_string_new(void *buf, size_t len)
{
    NEW(octet_string_t, ostring);
    ostring->buf = (uint8_t *)buf;
    ostring->len = len;
    return ostring;
}

static char *fmt_hexstring(octet_string_t const *ostring)
{
    static const char hex[] = "0123456789abcdef";
    if ((ostring->buf == NULL) || (ostring->len == 0)) {
        return strdup("none");
    }
    char *buf = (char *)XCALLOC(3 * ostring->len + 1, sizeof(char));
    char *ptr = buf;
    for (size_t i = 0; i < ostring->len; i++)
    {
        *ptr++ = hex[(ostring->buf[i] >> 4) & 0xf];
        *ptr++ = hex[ostring->buf[i] & 0xf];
        *ptr++ = ' ';
    }
    if (ptr > buf) {
        ptr[-1] = '\0';
    }
    return buf;
}

static char *replace_nonprintable_chars(octet_string_t const *ostring)
{
    char *buf = (char *)XCALLOC(ostring->len + 1, sizeof(char));
    for (size_t i = 0; i < ostring->len; i++) {
        buf[i] = (ostring->buf[i] >= 32 && ostring->buf[i] < 127) ? (char)ostring->buf[i] : '.';
    }
    return buf;
}

void octet_string_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent)
{
    char *h = fmt_hexstring(ostring);
    LA_ISPRINTF(vstr, indent, "%s", h);
    XFREE(h);
}

void octet_string_with_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent)
{
    char *h = fmt_hexstring(ostring);
    char *ascii = replace_nonprintable_chars(ostring);
    LA_ISPRINTF(vstr, indent, "%s\t\"%s\"", h, ascii);
    XFREE(h);
    XFREE(ascii);
}

void octet_string_as_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent)
{
    LA_ISPRINTF(vstr, indent, "%s", "");
    if (ostring->len == 0) {
        return;
    }
    char *replaced = replace_nonprintable_chars(ostring);
    la_vstring_append_sprintf(vstr, "%s", replaced);
    XFREE(replaced);
}

void octet_string_as_ascii_format_json(la_vstring *vstr, char const *key, octet_string_t const *ostring)
{
    char *replaced = replace_nonprintable_chars(ostring);
    la_json_append_string(vstr, key, replaced);
    XFREE(replaced);
}

void bitfield_format_text(la_vstring *vstr, uint8_t const *buf, size_t len, la_dict const *d)
{
    uint32_t val = 0;
    for (size_t i = 0; i < len; val = (val << 8) | buf[i++])
        ;
    if (val == 0)
    {
        la_vstring_append_sprintf(vstr, "%s", "none");
        return;
    }
    bool first = true;
    for (la_dict const *ptr = d; ptr->val != NULL; ptr++)
    {
        if ((val & (uint32_t)ptr->id) == (uint32_t)ptr->id)
        {
            la_vstring_append_sprintf(vstr, "%s%s", (first ? "" : ", "), (char *)ptr->val);
            first = false;
        }
    }
}

void bitfield_format_json(la_vstring *vstr, uint8_t const *buf, size_t len, la_dict const *d, char const *key)
{
    uint32_t val = 0;
    for (size_t i = 0; i < len; val = (val << 8) | buf[i++])
        ;
    la_json_array_start(vstr, key);
    if (val != 0)
    {
        for (la_dict const *ptr = d; ptr->val != NULL; ptr++)
        {
            if ((val & (uint32_t)ptr->id) == (uint32_t)ptr->id) {
                la_json_append_string(vstr, NULL, ptr->val);
            }
        }
    }
    la_json_array_end(vstr);
}

// The 28 significant bits of a 4 octet AVLC DLS address field, bit-reversed so the result
// reads address(24) | type(3) << 24 | status(1) << 27 (from dumpvdl2 avlc.c / bitstream.c)
static uint32_t reverse_bits(uint32_t v, int numbits)
{
    uint32_t r = 0;
    for (int i = 0; i < numbits; i++)
    {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}

uint32_t parse_dlc_addr(uint8_t *buf)
{
    uint32_t v = (buf[0] >> 1) | ((uint32_t)buf[1] << 6) | ((uint32_t)buf[2] << 13)
               | ((uint32_t)(buf[3] & 0xfe) << 20);
    return reverse_bits(v, 28) & 0x0fffffff;
}

static void unknown_proto_format_text(la_vstring *vstr, void const *data, int indent)
{
    octet_string_t const *ostring = (octet_string_t const *)data;
    if ((ostring->buf == NULL) || (ostring->len == 0)) {
        return;
    }
    LA_ISPRINTF(vstr, indent, "Data (%zu bytes):\n", ostring->len);
    octet_string_format_text(vstr, ostring, indent + 1);
    EOL(vstr);
}

// Designated, as the descriptors in icao.c and xid.c are: la_type_descriptor ends
// in eight reserved function pointers, and positional initialisers leave those
// unmentioned, which -Wmissing-field-initializers reports
static la_type_descriptor const proto_DEF_unknown = {
    .format_text = unknown_proto_format_text,
    .destroy     = NULL,
    .format_json = NULL,
    .json_key    = "unknown_proto"
};

la_proto_node *unknown_proto_pdu_new(void *buf, size_t len)
{
    octet_string_t *ostring = octet_string_new(buf, len);
    la_proto_node *node = la_proto_node_new();
    node->td = &proto_DEF_unknown;
    node->data = ostring;
    node->next = NULL;
    return node;
}
