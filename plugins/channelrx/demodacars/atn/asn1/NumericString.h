/*-
 * Copyright (c) 2003 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_NumericString_H_
#define	_NumericString_H_

#include "OCTET_STRING.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t NumericString_t;	/* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_NumericString;

asn_constr_check_f NumericString_constraint;

#ifdef __cplusplus
}
#endif

#endif	/* _NumericString_H_ */
