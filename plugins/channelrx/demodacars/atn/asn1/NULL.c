/*-
 * Copyright (c) 2003, 2005 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "asn_codecs_prim.h"
#include "NULL.h"
#include "BOOLEAN.h"	/* Implemented in terms of BOOLEAN type */

/*
 * NULL basic type description.
 */
static const ber_tlv_tag_t asn_DEF_NULL_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (5 << 2))
};
asn_TYPE_descriptor_t asn_DEF_NULL = {
	"NULL",
	"NULL",
	BOOLEAN_free,
	NULL_print,
	asn_generic_no_constraint,
	BOOLEAN_decode_ber,	/* Implemented in terms of BOOLEAN */
	NULL_encode_der,	/* Special handling of DER encoding */
	NULL_decode_xer,
	NULL_encode_xer,
	NULL_decode_uper,	/* Unaligned PER decoder */
	NULL_encode_uper,	/* Unaligned PER encoder */
	0, /* Use generic outmost tag fetcher */
	asn_DEF_NULL_tags,
	sizeof(asn_DEF_NULL_tags) / sizeof(asn_DEF_NULL_tags[0]),
	asn_DEF_NULL_tags,	/* Same as above */
	sizeof(asn_DEF_NULL_tags) / sizeof(asn_DEF_NULL_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	0	/* No specifics */
};

asn_enc_rval_t
NULL_encode_der(asn_TYPE_descriptor_t *td, void *ptr,
	int tag_mode, ber_tlv_tag_t tag,
	asn_app_consume_bytes_f *cb, void *app_key) {
	asn_enc_rval_t erval;

	erval.encoded = der_write_tags(td, 0, tag_mode, 0, tag, cb, app_key);
	if(erval.encoded == -1) {
		erval.failed_type = td;
		erval.structure_ptr = ptr;
	}

	ASN__ENCODED_OK(erval);
}

asn_enc_rval_t
NULL_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	asn_enc_rval_t er;

	(void)td;
	(void)sptr;
	(void)ilevel;
	(void)flags;
	(void)cb;
	(void)app_key;

	/* XMLNullValue is empty */
	er.encoded = 0;
	ASN__ENCODED_OK(er);
}


static enum xer_pbd_rval
NULL__xer_body_decode(asn_TYPE_descriptor_t *td, void *sptr, const void *chunk_buf, size_t chunk_size) {
	(void)td;
	(void)sptr;
	(void)chunk_buf;    /* Going to be empty according to the rules below. */

	/*
	 * There must be no content in self-terminating <NULL/> tag.
	 */
	if(chunk_size)
		return XPBD_BROKEN_ENCODING;
	else
		return XPBD_BODY_CONSUMED;
}

asn_dec_rval_t
NULL_decode_xer(asn_codec_ctx_t *opt_codec_ctx,
	asn_TYPE_descriptor_t *td, void **sptr, const char *opt_mname,
		const void *buf_ptr, size_t size) {

	return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(NULL_t), opt_mname, buf_ptr, size,
		NULL__xer_body_decode);
}

int
NULL_print(asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
	asn_app_consume_bytes_f *cb, void *app_key) {

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(sptr) {
		return (cb("<present>", 9, app_key) < 0) ? -1 : 0;
	} else {
		return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;
	}
}

asn_dec_rval_t
NULL_decode_uper(asn_codec_ctx_t *opt_codec_ctx, asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void **sptr, asn_per_data_t *pd) {
	asn_dec_rval_t rv;

	(void)opt_codec_ctx;
	(void)td;
	(void)constraints;
	(void)pd;

	if(!*sptr) {
		*sptr = MALLOC(sizeof(NULL_t));
		if(*sptr) {
			*(NULL_t *)*sptr = 0;
		} else {
			ASN__DECODE_FAILED;
		}
	}

	/*
	 * NULL type does not have content octets.
	 */

	rv.code = RC_OK;
	rv.consumed = 0;
	return rv;
}

asn_enc_rval_t
NULL_encode_uper(asn_TYPE_descriptor_t *td, asn_per_constraints_t *constraints,
		void *sptr, asn_per_outp_t *po) {
	asn_enc_rval_t er;

	(void)td;
	(void)constraints;
	(void)sptr;
	(void)po;

	er.encoded = 0;
	ASN__ENCODED_OK(er);
}
