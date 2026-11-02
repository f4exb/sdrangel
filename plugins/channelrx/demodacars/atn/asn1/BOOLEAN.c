/*-
 * Copyright (c) 2003, 2005 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "asn_codecs_prim.h"
#include "BOOLEAN.h"

/*
 * BOOLEAN basic type description.
 */
static const ber_tlv_tag_t asn_DEF_BOOLEAN_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (1 << 2))
};
asn_TYPE_descriptor_t asn_DEF_BOOLEAN = {
	"BOOLEAN",
	"BOOLEAN",
	BOOLEAN_free,
	BOOLEAN_print,
	asn_generic_no_constraint,
	BOOLEAN_decode_ber,
	BOOLEAN_encode_der,
	BOOLEAN_decode_xer,
	BOOLEAN_encode_xer,
	BOOLEAN_decode_uper,	/* Unaligned PER decoder */
	BOOLEAN_encode_uper,	/* Unaligned PER encoder */
	0, /* Use generic outmost tag fetcher */
	asn_DEF_BOOLEAN_tags,
	sizeof(asn_DEF_BOOLEAN_tags) / sizeof(asn_DEF_BOOLEAN_tags[0]),
	asn_DEF_BOOLEAN_tags,	/* Same as above */
	sizeof(asn_DEF_BOOLEAN_tags) / sizeof(asn_DEF_BOOLEAN_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	0	/* No specifics */
};

/*
 * Decode BOOLEAN type.
 */
asn_dec_rval_t
BOOLEAN_decode_ber(asn_codec_ctx_t *opt_codec_ctx,
		asn_TYPE_descriptor_t *td,
		void **bool_value, const void *buf_ptr, size_t size,
		int tag_mode) {
	BOOLEAN_t *st = (BOOLEAN_t *)*bool_value;
	asn_dec_rval_t rval;
	ber_tlv_len_t length;
	ber_tlv_len_t lidx;

	if(st == NULL) {
		st = (BOOLEAN_t *)(*bool_value = CALLOC(1, sizeof(*st)));
		if(st == NULL) {
			rval.code = RC_FAIL;
			rval.consumed = 0;
			return rval;
		}
	}

	ASN_DEBUG("Decoding %s as BOOLEAN (tm=%d)",
		td->name, tag_mode);

	/*
	 * Check tags.
	 */
	rval = ber_check_tags(opt_codec_ctx, td, 0, buf_ptr, size,
		tag_mode, 0, &length, 0);
	if(rval.code != RC_OK)
		return rval;

	ASN_DEBUG("Boolean length is %d bytes", (int)length);

	buf_ptr = ((const char *)buf_ptr) + rval.consumed;
	size -= rval.consumed;
	if(length > (ber_tlv_len_t)size) {
		rval.code = RC_WMORE;
		rval.consumed = 0;
		return rval;
	}

	/*
	 * Compute boolean value.
	 */
	for(*st = 0, lidx = 0;
		(lidx < length) && *st == 0; lidx++) {
		/*
		 * Very simple approach: read bytes until the end or
		 * value is already TRUE.
		 * BOOLEAN is not supposed to contain meaningful data anyway.
		 */
		*st |= ((const uint8_t *)buf_ptr)[lidx];
	}

	rval.code = RC_OK;
	rval.consumed += length;

	ASN_DEBUG("Took %ld/%ld bytes to encode %s, value=%d",
		(long)rval.consumed, (long)length,
		td->name, *st);
	
	return rval;
}

asn_enc_rval_t
BOOLEAN_encode_der(asn_TYPE_descriptor_t *td, void *sptr,
	int tag_mode, ber_tlv_tag_t tag,
	asn_app_consume_bytes_f *cb, void *app_key) {
	asn_enc_rval_t erval;
	BOOLEAN_t *st = (BOOLEAN_t *)sptr;

	erval.encoded = der_write_tags(td, 1, tag_mode, 0, tag, cb, app_key);
	if(erval.encoded == -1) {
		erval.failed_type = td;
		erval.structure_ptr = sptr;
		return erval;
	}

	if(cb) {
		uint8_t bool_value;

		bool_value = *st ? 0xff : 0; /* 0xff mandated by DER */

		if(cb(&bool_value, 1, app_key) < 0) {
			erval.encoded = -1;
			erval.failed_type = td;
			erval.structure_ptr = sptr;
			return erval;
		}
	}

	erval.encoded += 1;

	ASN__ENCODED_OK(erval);
}


/*
 * Decode the chunk of XML text encoding INTEGER.
 */
static enum xer_pbd_rval
BOOLEAN__xer_body_decode(asn_TYPE_descriptor_t *td, void *sptr, const void *chunk_buf, size_t chunk_size) {
	BOOLEAN_t *st = (BOOLEAN_t *)sptr;
	const char *p = (const char *)chunk_buf;

	(void)td;

	if(chunk_size && p[0] == 0x3c /* '<' */) {
		switch(xer_check_tag(chunk_buf, chunk_size, "false")) {
		case XCT_BOTH:
			/* "<false/>" */
			*st = 0;
			break;
		case XCT_UNKNOWN_BO:
			if(xer_check_tag(chunk_buf, chunk_size, "true")
					!= XCT_BOTH)
				return XPBD_BROKEN_ENCODING;
			/* "<true/>" */
			*st = 1;	/* Or 0xff as in DER?.. */
			break;
		default:
			return XPBD_BROKEN_ENCODING;
		}
		return XPBD_BODY_CONSUMED;
	} else {
		return XPBD_BROKEN_ENCODING;
	}
}


asn_dec_rval_t
BOOLEAN_decode_xer(asn_codec_ctx_t *opt_codec_ctx,
	asn_TYPE_descriptor_t *td, void **sptr, const char *opt_mname,
		const void *buf_ptr, size_t size) {

	return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(BOOLEAN_t), opt_mname, buf_ptr, size,
		BOOLEAN__xer_body_decode);
}

asn_enc_rval_t
BOOLEAN_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	const BOOLEAN_t *st = (const BOOLEAN_t *)sptr;
	asn_enc_rval_t er;

	(void)ilevel;
	(void)flags;

	if(!st) ASN__ENCODE_FAILED;

	if(*st) {
		ASN__CALLBACK("<true/>", 7);
		er.encoded = 7;
	} else {
		ASN__CALLBACK("<false/>", 8);
		er.encoded = 8;
	}

	ASN__ENCODED_OK(er);
cb_failed:
	ASN__ENCODE_FAILED;
}

int
BOOLEAN_print(asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
	asn_app_consume_bytes_f *cb, void *app_key) {
	const BOOLEAN_t *st = (const BOOLEAN_t *)sptr;
	const char *buf;
	size_t buflen;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(st) {
		if(*st) {
			buf = "TRUE";
			buflen = 4;
		} else {
			buf = "FALSE";
			buflen = 5;
		}
	} else {
		buf = "<absent>";
		buflen = 8;
	}

	return (cb(buf, buflen, app_key) < 0) ? -1 : 0;
}

void
BOOLEAN_free(asn_TYPE_descriptor_t *td, void *ptr, int contents_only) {
	if(td && ptr && !contents_only) {
		FREEMEM(ptr);
	}
}

asn_dec_rval_t
BOOLEAN_decode_uper(asn_codec_ctx_t *opt_codec_ctx, asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void **sptr, asn_per_data_t *pd) {
	asn_dec_rval_t rv;
	BOOLEAN_t *st = (BOOLEAN_t *)*sptr;

	(void)opt_codec_ctx;
	(void)constraints;

	if(!st) {
		st = (BOOLEAN_t *)(*sptr = MALLOC(sizeof(*st)));
		if(!st) ASN__DECODE_FAILED;
	}

	/*
	 * Extract a single bit
	 */
	switch(per_get_few_bits(pd, 1)) {
	case 1: *st = 1; break;
	case 0: *st = 0; break;
	case -1: default: ASN__DECODE_STARVED;
	}

	ASN_DEBUG("%s decoded as %s", td->name, *st ? "TRUE" : "FALSE");

	rv.code = RC_OK;
	rv.consumed = 1;
	return rv;
}


asn_enc_rval_t
BOOLEAN_encode_uper(asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void *sptr, asn_per_outp_t *po) {
	const BOOLEAN_t *st = (const BOOLEAN_t *)sptr;
	asn_enc_rval_t er = { 0, 0, 0 };

	(void)constraints;

	if(!st) ASN__ENCODE_FAILED;

	if(per_put_few_bits(po, *st ? 1 : 0, 1))
		ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}
