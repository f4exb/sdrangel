/*-
 * Copyright (c) 2003 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_BOOLEAN_H_
#define	_BOOLEAN_H_

#include "asn_application.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The underlying integer may contain various values, but everything
 * non-zero is capped to 0xff by the DER encoder. The BER decoder may
 * yield non-zero values different from 1, beware.
 */
typedef int BOOLEAN_t;

extern asn_TYPE_descriptor_t asn_DEF_BOOLEAN;

asn_struct_free_f BOOLEAN_free;
asn_struct_print_f BOOLEAN_print;
ber_type_decoder_f BOOLEAN_decode_ber;
der_type_encoder_f BOOLEAN_encode_der;
xer_type_decoder_f BOOLEAN_decode_xer;
xer_type_encoder_f BOOLEAN_encode_xer;
per_type_decoder_f BOOLEAN_decode_uper;
per_type_encoder_f BOOLEAN_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _BOOLEAN_H_ */
