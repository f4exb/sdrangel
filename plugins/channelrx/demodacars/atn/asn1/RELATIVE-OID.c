/*-
 * Copyright (c) 2003, 2004, 2005 Lev Walkin <vlm@lionet.info>.
 * 	All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "RELATIVE-OID.h"
#include "OCTET_STRING.h"
#include "asn_codecs_prim.h"	/* Encoder and decoder of a primitive type */
#include <limits.h>	/* for CHAR_BIT */
#include <errno.h>

/*
 * RELATIVE-OID basic type description.
 */
static const ber_tlv_tag_t asn_DEF_RELATIVE_OID_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (13 << 2))
};
asn_TYPE_descriptor_t asn_DEF_RELATIVE_OID = {
	"RELATIVE-OID",
	"RELATIVE_OID",
	ASN__PRIMITIVE_TYPE_free,
	RELATIVE_OID_print,
	asn_generic_no_constraint,
	ber_decode_primitive,
	der_encode_primitive,
	RELATIVE_OID_decode_xer,
	RELATIVE_OID_encode_xer,
	OCTET_STRING_decode_uper,
	OCTET_STRING_encode_uper,
	0, /* Use generic outmost tag fetcher */
	asn_DEF_RELATIVE_OID_tags,
	sizeof(asn_DEF_RELATIVE_OID_tags)
	    / sizeof(asn_DEF_RELATIVE_OID_tags[0]),
	asn_DEF_RELATIVE_OID_tags,	/* Same as above */
	sizeof(asn_DEF_RELATIVE_OID_tags)
	    / sizeof(asn_DEF_RELATIVE_OID_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	0	/* No specifics */
};

static ssize_t
RELATIVE_OID__dump_body(const RELATIVE_OID_t *st, asn_app_consume_bytes_f *cb, void *app_key) {
	ssize_t wrote = 0;
	ssize_t ret;
	int startn;
	int i;

	for(i = 0, startn = 0; i < st->size; i++) {
		uint8_t b = st->buf[i];
		if((b & 0x80))			/* Continuation expected */
			continue;
		if(startn) {
			/* Separate arcs */
			if(cb(".", 1, app_key) < 0)
				return -1;
			wrote++;
		}

		ret = OBJECT_IDENTIFIER__dump_arc(&st->buf[startn],
			i - startn + 1, 0, cb, app_key);
		if(ret < 0) return -1;
		wrote += ret;

		startn = i + 1;
	}

	return wrote;
}

int
RELATIVE_OID_print(asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
	asn_app_consume_bytes_f *cb, void *app_key) {
	const RELATIVE_OID_t *st = (const RELATIVE_OID_t *)sptr;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(!st || !st->buf)
		return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	/* Dump preamble */
	if(cb("{ ", 2, app_key) < 0)
		return -1;

	if(RELATIVE_OID__dump_body(st, cb, app_key) < 0)
		return -1;

	return (cb(" }", 2, app_key) < 0) ? -1 : 0;
}

static enum xer_pbd_rval
RELATIVE_OID__xer_body_decode(asn_TYPE_descriptor_t *td, void *sptr, const void *chunk_buf, size_t chunk_size) {
	RELATIVE_OID_t *st = (RELATIVE_OID_t *)sptr;
	const char *chunk_end = (const char *)chunk_buf + chunk_size;
	const char *endptr;
	long s_arcs[6];
	long *arcs = s_arcs;
	int arcs_count;
	int ret;

	(void)td;
	/* Only read by the assert below, which NDEBUG compiles away */
	(void)chunk_end;

	arcs_count = OBJECT_IDENTIFIER_parse_arcs(
		(const char *)chunk_buf, chunk_size,
		arcs, sizeof(s_arcs)/sizeof(s_arcs[0]), &endptr);
	if(arcs_count < 0) {
		/* Expecting at least one arc arcs */
		return XPBD_BROKEN_ENCODING;
	} else if(arcs_count == 0) {
		return XPBD_NOT_BODY_IGNORE;
	}
	assert(endptr == chunk_end);

	if((size_t)arcs_count > sizeof(s_arcs)/sizeof(s_arcs[0])) {
		arcs = (long *)MALLOC(arcs_count * sizeof(long));
		if(!arcs) return XPBD_SYSTEM_FAILURE;
		ret = OBJECT_IDENTIFIER_parse_arcs(
			(const char *)chunk_buf, chunk_size,
			arcs, arcs_count, &endptr);
		if(ret != arcs_count)
			return XPBD_SYSTEM_FAILURE;	/* assert?.. */
	}

	/*
	 * Convert arcs into BER representation.
	 */
	ret = RELATIVE_OID_set_arcs(st, arcs, sizeof(*arcs), arcs_count);
	if(arcs != s_arcs) FREEMEM(arcs);

	return ret ? XPBD_SYSTEM_FAILURE : XPBD_BODY_CONSUMED;
}

asn_dec_rval_t
RELATIVE_OID_decode_xer(asn_codec_ctx_t *opt_codec_ctx,
	asn_TYPE_descriptor_t *td, void **sptr, const char *opt_mname,
		const void *buf_ptr, size_t size) {

	return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(RELATIVE_OID_t), opt_mname,
			buf_ptr, size, RELATIVE_OID__xer_body_decode);
}

asn_enc_rval_t
RELATIVE_OID_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	RELATIVE_OID_t *st = (RELATIVE_OID_t *)sptr;
	asn_enc_rval_t er;

	(void)ilevel;	/* Unused argument */
	(void)flags;	/* Unused argument */

	if(!st || !st->buf)
		ASN__ENCODE_FAILED;

	er.encoded = RELATIVE_OID__dump_body(st, cb, app_key);
	if(er.encoded < 0) ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}

int
RELATIVE_OID_get_arcs(const RELATIVE_OID_t *roid,
	void *arcs, unsigned int arc_type_size, unsigned int arc_slots) {
	void *arcs_end = (char *)arcs + (arc_slots * arc_type_size);
	int num_arcs = 0;
	int startn = 0;
	int i;

	if(!roid || !roid->buf) {
		errno = EINVAL;
		return -1;
	}

	for(i = 0; i < roid->size; i++) {
		uint8_t b = roid->buf[i];
		if((b & 0x80))			/* Continuation expected */
			continue;

		if(arcs < arcs_end) {
			if(OBJECT_IDENTIFIER_get_single_arc(
				&roid->buf[startn],
					i - startn + 1, 0,
					arcs, arc_type_size))
				return -1;
			arcs = ((char *)arcs) + arc_type_size;
			num_arcs++;
		}

		startn = i + 1;
	}

	return num_arcs;
}

int
RELATIVE_OID_set_arcs(RELATIVE_OID_t *roid, void *arcs, unsigned int arc_type_size, unsigned int arcs_slots) {
	uint8_t *buf;
	uint8_t *bp;
	unsigned int size;
	unsigned int i;

	if(roid == NULL || arcs == NULL || arc_type_size < 1) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Roughly estimate the maximum size necessary to encode these arcs.
	 */
	size = ((arc_type_size * CHAR_BIT + 6) / 7) * arcs_slots;
	bp = buf = (uint8_t *)MALLOC(size + 1);
	if(!buf) {
		/* ENOMEM */
		return -1;
	}

	/*
	 * Encode the arcs.
	 */
	for(i = 0; i < arcs_slots; i++, arcs = ((char *)arcs) + arc_type_size) {
		bp += OBJECT_IDENTIFIER_set_single_arc(bp,
			arcs, arc_type_size, 0);
	}

	assert((unsigned)(bp - buf) <= size);

	/*
	 * Replace buffer.
	 */
	roid->size = (int)(bp - buf);
	bp = roid->buf;
	roid->buf = buf;
	if(bp) FREEMEM(bp);

	return 0;
}

