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
#include <stdint.h>
#include <stdlib.h>
#include <libacars/libacars.h>          // la_proto_node, la_type_descriptor
#include <libacars/vstring.h>           // la_vstring
#include <libacars/dict.h>              // la_dict
#include <libacars/json.h>
#include "asn1/BIT_STRING.h"
#include "asn1/ACSE-apdu.h"
#include "asn1/CMAircraftMessage.h"
#include "asn1/CMGroundMessage.h"
#include "asn1/Fully-encoded-data.h"
#include "asn1/ProtectedAircraftPDUs.h"
#include "asn1/ProtectedGroundPDUs.h"
#include "asn1/ATCDownlinkMessage.h"
#include "asn1/ATCUplinkMessage.h"
#include "asn1/ADSAircraftPDUs.h"
#include "asn1/ADSGroundPDUs.h"
#include "asn1/ADSMessage.h"
#include "asn1/ADSAccept.h"
#include "asn1/ADSReject.h"
#include "asn1/ADSReport.h"
#include "asn1/ADSNonCompliance.h"
#include "asn1/ADSPositiveAcknowledgement.h"
#include "asn1/ADSRequestContract.h"
#include "dumpvdl2.h"
#include "asn1-util.h"                  // asn1_decode_as(), asn1_pdu_destroy(), asn1_pdu_t, proto_DEF_asn1_pdu
#include "asn1-format-icao.h"           // asn1_*_formatter_table, asn1_*_formatter_table_len
#include "icao.h"

#define ACSE_APDU_TYPE_MATCHES(type, value) ((type) == (value) || (type) == ACSE_apdu_PR_NOTHING)
#define APP_TYPE_MATCHES(type, value) ((type) == (value) || (type) == ICAO_APP_TYPE_UNKNOWN)

// Forward declarations
la_type_descriptor const proto_DEF_x225_spdu;
la_type_descriptor const proto_DEF_cpdlc;
la_type_descriptor const proto_DEF_cm;
la_type_descriptor const proto_DEF_adsc_v2;
la_type_descriptor const proto_DEF_x227_acse_apdu;

/********************************************************************************
  * ICAO applications
*********************************************************************************/

static int decode_ADSAircraftPDUs(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
		uint8_t *buf, int size) {
	ADSAircraftPDUs_t *adsairpdus = NULL;
	int ret = -1;   // error by default

	if(asn1_decode_as(&asn_DEF_ADSAircraftPDUs, (void **)&adsairpdus, buf, size) != 0)
		goto ads_aircraft_pdus_cleanup;

	asn_TYPE_descriptor_t *next_td = NULL;
	ADSMessage_t *next_pdu = NULL;
	switch(adsairpdus->adsAircraftPdu.present) {
		// The following PDU types contain a full ADS Message (of type ADSMessage_t)
		// PER-encoded and carried as BIT STRING with an additional integrity check
		// (ATN checksum). For these types we need to perform a second decoding pass of
		// the PER-encoded part.
		// First, save the type descriptor of the inner message and the pointer to it.
		case ADSAircraftPDU_PR_aDS_report_PDU:
			next_td = &asn_DEF_ADSReport;
			next_pdu = &adsairpdus->adsAircraftPdu.choice.aDS_report_PDU.ic_report.aDSMessage;
			break;
		case ADSAircraftPDU_PR_aDS_accepted_PDU:
			next_td = &asn_DEF_ADSAccept;
			next_pdu = &adsairpdus->adsAircraftPdu.choice.aDS_accepted_PDU.ic_report.aDSMessage;
			break;
		case ADSAircraftPDU_PR_aDS_rejected_PDU:
			next_td = &asn_DEF_ADSReject;
			next_pdu = &adsairpdus->adsAircraftPdu.choice.aDS_rejected_PDU.ic_reject.aDSMessage;
			break;
		case ADSAircraftPDU_PR_aDS_ncn_PDU:
			next_td = &asn_DEF_ADSNonCompliance;
			next_pdu = &adsairpdus->adsAircraftPdu.choice.aDS_ncn_PDU.ic_ncn.aDSMessage;
			break;
		case ADSAircraftPDU_PR_aDS_positive_acknowledgement_PDU:
			next_td = &asn_DEF_ADSPositiveAcknowledgement;
			next_pdu = &adsairpdus->adsAircraftPdu.choice.aDS_positive_acknowledgement_PDU.ic_positive_ack.aDSPositiveAck;
			break;
			// The following are single-layer PDU types. They have already been decoded
			// completely during the first pass
		case ADSAircraftPDU_PR_aDS_cancel_positive_acknowledgement_PDU:
		case ADSAircraftPDU_PR_aDS_cancel_negative_acknowledgement_PDU:
		case ADSAircraftPDU_PR_aDS_provider_abort_PDU:
		case ADSAircraftPDU_PR_aDS_user_abort_PDU:
			// Return the whole outer structure (of type ADSAircraftPDUs_t) as the result.
			*decoded_result = adsairpdus;
			*decoded_apdu_type = &asn_DEF_ADSAircraftPDUs;
			return 0;
		default:
			break;
	}
	// We need a second pass; do we have all the necessary data?
	if(next_td == NULL || next_pdu == NULL) {
		goto ads_aircraft_pdus_cleanup;
	}
	if(asn1_decode_as(next_td, decoded_result, next_pdu->buf, next_pdu->size) == 0) {
		// Second pass succeeded. Return success (but clean up the outer ADSAircraftPDU first,
		// as it's no longer needed)
		ret = 0;
		*decoded_apdu_type = next_td;
		goto ads_aircraft_pdus_cleanup;
	}
	debug_print(D_PROTO, "Unable to decode ADSAircraftPDUs as %s\n", next_td->name);
ads_aircraft_pdus_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ADSAircraftPDUs, adsairpdus);
	return ret;
}

static int decode_ADSGroundPDUs(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
		uint8_t *buf, int size) {
	ADSGroundPDUs_t *adsgndpdus = NULL;
	int ret = -1;   // error by default

	if(asn1_decode_as(&asn_DEF_ADSGroundPDUs, (void **)&adsgndpdus, buf, size) != 0)
		goto ads_ground_pdus_cleanup;

	asn_TYPE_descriptor_t *next_td = NULL;
	ADSMessage_t *next_pdu = NULL;
	switch(adsgndpdus->adsGroundPdu.present) {
		// The following PDU types contain a full ADS Message (of type ADSMessage_t)
		// PER-encoded and carried as BIT STRING with an additional integrity check
		// (ATN checksum). For these types we need to perform a second decoding pass of
		// the PER-encoded part.
		// First, save the type descriptor of the inner message and the pointer to it.
		case ADSGroundPDU_PR_aDS_contract_PDU:
			next_td = &asn_DEF_ADSRequestContract;
			next_pdu = &adsgndpdus->adsGroundPdu.choice.aDS_contract_PDU.ic_contract_request.aDSMessage;
			break;
			// The following are single-layer PDU types. They have already been decoded
			// completely during the first pass
		case ADSGroundPDU_PR_aDS_cancel_contract_PDU:
		case ADSGroundPDU_PR_aDS_cancel_all_contracts_PDU:
		case ADSGroundPDU_PR_aDS_provider_abort_PDU:
		case ADSGroundPDU_PR_aDS_user_abort_PDU:
			// Return the whole outer structure (of type ADSGroundPDUs_t) as the result.
			*decoded_result = adsgndpdus;
			*decoded_apdu_type = &asn_DEF_ADSGroundPDUs;
			return 0;
		default:
			break;
	}
	// We need a second pass; do we have all the necessary data?
	if(next_td == NULL || next_pdu == NULL) {
		goto ads_ground_pdus_cleanup;
	}
	if(asn1_decode_as(next_td, decoded_result, next_pdu->buf, next_pdu->size) == 0) {
		// Second pass succeeded. Return success (but clean up the outer ADSGroundPDUs first,
		// as it's no longer needed)
		ret = 0;
		*decoded_apdu_type = next_td;
		goto ads_ground_pdus_cleanup;
	}
	debug_print(D_PROTO, "Unable to decode ADSGroundPDUs as %s\n", next_td->name);
ads_ground_pdus_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ADSGroundPDUs, adsgndpdus);
	return ret;
}

static int decode_protected_ATCDownlinkMessage(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
		ACSE_apdu_PR acse_apdu_type, uint8_t *buf, int size) {
	ProtectedAircraftPDUs_t *pairpdu = NULL;
	int ret = -1;   // error by default

	if(asn1_decode_as(&asn_DEF_ProtectedAircraftPDUs, (void **)&pairpdu, buf, size) != 0)
		goto protected_aircraft_pdu_cleanup;

	ProtectedDownlinkMessage_t *p_downlink_msg = NULL;
	switch(pairpdu->present) {
		case ProtectedAircraftPDUs_PR_startdown:
			p_downlink_msg = &pairpdu->choice.startdown.startDownlinkMessage;
			break;
		case ProtectedAircraftPDUs_PR_send:
			p_downlink_msg = &pairpdu->choice.send;
			break;
		case ProtectedAircraftPDUs_PR_abortUser:
		case ProtectedAircraftPDUs_PR_abortProvider:
			// First do a check on ACSE APDU type, if present, to avoid possible clashing with
			// other message types (eg. CMContactResponse). abortUser and abortProvider shall
			// appear in ABRT APDUs only.
			if(!ACSE_APDU_TYPE_MATCHES(acse_apdu_type, ACSE_apdu_PR_abrt))
				break;
			// These messages have no ATCDownlinkMessage inside.
			// Return the whole ProtectedAircraftPDUs for output.
			*decoded_result = pairpdu;
			*decoded_apdu_type = &asn_DEF_ProtectedAircraftPDUs;
			return 0;
		default:
			break;
	}
	if(p_downlink_msg == NULL)
		goto protected_aircraft_pdu_cleanup;
	if(p_downlink_msg->protectedMessage == NULL) {  // NULL message is valid
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCDownlinkMessage;
		goto protected_aircraft_pdu_cleanup;
	}
	if(asn1_decode_as(&asn_DEF_ATCDownlinkMessage, decoded_result,
				p_downlink_msg->protectedMessage->buf,
				p_downlink_msg->protectedMessage->size) == 0) {
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCDownlinkMessage;
		goto protected_aircraft_pdu_cleanup;
	}
	debug_print(D_PROTO, "unable to decode ProtectedAircraftPDU as ATCDownlinkMessage\n");
protected_aircraft_pdu_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ProtectedAircraftPDUs, pairpdu);
	return ret;
}

static int decode_protected_ATCUplinkMessage(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
		ACSE_apdu_PR acse_apdu_type, uint8_t *buf, int size) {
	ProtectedGroundPDUs_t *pgndpdu = NULL;
	int ret = -1;   // error by default

	if(asn1_decode_as(&asn_DEF_ProtectedGroundPDUs, (void **)&pgndpdu, buf, size) != 0)
		goto protected_ground_pdu_cleanup;

	ProtectedUplinkMessage_t *p_uplink_msg = NULL;
	switch(pgndpdu->present) {
		case ProtectedGroundPDUs_PR_startup:
			p_uplink_msg = &pgndpdu->choice.startup;
			break;
		case ProtectedGroundPDUs_PR_send:
			p_uplink_msg = &pgndpdu->choice.send;
			break;
		case ProtectedGroundPDUs_PR_abortUser:
		case ProtectedGroundPDUs_PR_abortProvider:
			// First do a check on ACSE APDU type, if present, to avoid possible clashing with
			// other message types (eg. CMContactResponse). abortUser and abortProvider shall
			// appear in ABRT APDUs only.
			if(!ACSE_APDU_TYPE_MATCHES(acse_apdu_type, ACSE_apdu_PR_abrt))
				break;
			// These messages have no ATCUplinkMessage inside.
			// Return the whole ProtectedGroundPDUs for output.
			*decoded_result = pgndpdu;
			*decoded_apdu_type = &asn_DEF_ProtectedGroundPDUs;
			return 0;
		default:
			break;
	}
	if(p_uplink_msg == NULL)
		goto protected_ground_pdu_cleanup;
	if(p_uplink_msg->protectedMessage == NULL) {    // NULL message is valid
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCUplinkMessage;
		goto protected_ground_pdu_cleanup;
	}
	if(asn1_decode_as(&asn_DEF_ATCUplinkMessage, decoded_result,
				p_uplink_msg->protectedMessage->buf,
				p_uplink_msg->protectedMessage->size) == 0) {
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCUplinkMessage;
		goto protected_ground_pdu_cleanup;
	}
	debug_print(D_PROTO, "unable to decode ProtectedGroundPDU as ATCUplinkMessage\n");
protected_ground_pdu_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ProtectedGroundPDUs, pgndpdu);
	return ret;
}

static la_proto_node *arbitrary_payload_parse(AE_qualifier_form2_t app_type,
		ACSE_apdu_PR acse_apdu_type, uint8_t *buf, uint32_t size, uint32_t *msg_type) {
	void *msg = NULL;
	la_proto_node *node = NULL;
	la_type_descriptor const *td = NULL;
	NEW(asn1_pdu_t, pdu);
	asn_TYPE_descriptor_t *decoded_apdu_type = NULL;
	if(*msg_type & MSGFLT_SRC_AIR) {
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CPC) &&
				decode_protected_ATCDownlinkMessage((void **)&msg, &decoded_apdu_type, acse_apdu_type, buf, size) == 0) {
			pdu->type = decoded_apdu_type;
			pdu->data = msg;
			td = &proto_DEF_cpdlc;
			*msg_type |= MSGFLT_CPDLC;
			goto end;
		}
		ASN_STRUCT_FREE(asn_DEF_ATCDownlinkMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CMA) &&
				asn1_decode_as(&asn_DEF_CMAircraftMessage, (void **)&msg, buf, size) == 0) {
			pdu->type = &asn_DEF_CMAircraftMessage;
			pdu->data = msg;
			td = &proto_DEF_cm;
			*msg_type |= MSGFLT_CM;
			goto end;
		}
		ASN_STRUCT_FREE(asn_DEF_CMAircraftMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_ADS) &&
				decode_ADSAircraftPDUs((void **)&msg, &decoded_apdu_type, buf, size) == 0) {
			pdu->type = decoded_apdu_type;
			pdu->data = msg;
			td = &proto_DEF_adsc_v2;
			*msg_type |= MSGFLT_ADSC;
			goto end;
		}
		ASN_STRUCT_FREE(asn_DEF_ADSAircraftPDUs, msg);
		msg = NULL;
	} else {    // MSGFLT_SRC_GND implied
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CPC) &&
				decode_protected_ATCUplinkMessage((void **)&msg, &decoded_apdu_type, acse_apdu_type, buf, size) == 0) {
			pdu->type = decoded_apdu_type;
			pdu->data = msg;
			td = &proto_DEF_cpdlc;
			*msg_type |= MSGFLT_CPDLC;
			goto end;
		}
		ASN_STRUCT_FREE(asn_DEF_ATCUplinkMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CMA) &&
				asn1_decode_as(&asn_DEF_CMGroundMessage, (void **)&msg, buf, size) == 0) {
			pdu->type = &asn_DEF_CMGroundMessage;
			pdu->data = msg;
			td = &proto_DEF_cm;
			*msg_type |= MSGFLT_CM;
			goto end;
		}
		ASN_STRUCT_FREE(asn_DEF_CMGroundMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_ADS) &&
				decode_ADSGroundPDUs((void **)&msg, &decoded_apdu_type, buf, size) == 0) {
			pdu->type = decoded_apdu_type;
			pdu->data = msg;
			td = &proto_DEF_adsc_v2;
			*msg_type |= MSGFLT_ADSC;
			goto end;
		}
		ASN_STRUCT_FREE(asn_DEF_ADSGroundPDUs, msg);
		msg = NULL;
	}
	debug_print(D_PROTO, "unknown payload type\n");
	XFREE(pdu);
	return NULL;        // the caller will turn this into unknown_proto_pdu
end:
	pdu->formatter_table_text = asn1_icao_formatter_table_text;
	pdu->formatter_table_text_len = asn1_icao_formatter_table_text_len;
	pdu->formatter_table_json = asn1_icao_formatter_table_json;
	pdu->formatter_table_json_len = asn1_icao_formatter_table_json_len;
	node = la_proto_node_new();
	node->td = td;
	node->data = pdu;
	node->next = NULL;
	return node;
}

/********************************************************************************
  * ICAO Doc 9705 ULCS / X.227 ACSE
*********************************************************************************/

static la_proto_node *ulcs_acse_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	ACSE_apdu_t *acse_apdu = NULL;
	la_proto_node *node = NULL;
	la_proto_node *next_node = NULL;
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, &asn_DEF_ACSE_apdu, (void **)&acse_apdu, buf, len);
	if(rval.code != RC_OK) {
		debug_print(D_PROTO, "Decoding failed at position %ld\n", (long)rval.consumed);
		goto fail;
	}
#ifdef DEBUG
	if(Config.debug_filter & D_PROTO_DETAIL) {
		asn_fprint(stderr, &asn_DEF_ACSE_apdu, acse_apdu, 1);
	}
#endif

	NEW(asn1_pdu_t, apdu);
	apdu->data = acse_apdu;
	apdu->type = &asn_DEF_ACSE_apdu;
	apdu->formatter_table_text = asn1_acse_formatter_table_text;
	apdu->formatter_table_text_len = asn1_acse_formatter_table_text_len;
	apdu->formatter_table_json = asn1_acse_formatter_table_json;
	apdu->formatter_table_json_len = asn1_acse_formatter_table_json_len;

	AE_qualifier_form2_t ae_qualifier = ICAO_APP_TYPE_UNKNOWN;
	Association_information_t *user_info = NULL;
	switch(acse_apdu->present) {
		case ACSE_apdu_PR_aarq:
			user_info = acse_apdu->choice.aarq.user_information;
			if(acse_apdu->choice.aarq.calling_AE_qualifier != NULL) {
				if(acse_apdu->choice.aarq.calling_AE_qualifier->present == AE_qualifier_PR_ae_qualifier_form2) {
					ae_qualifier = acse_apdu->choice.aarq.calling_AE_qualifier->choice.ae_qualifier_form2;
				}
			}
			break;
		case ACSE_apdu_PR_aare:
			user_info = acse_apdu->choice.aare.user_information;
			break;
		case ACSE_apdu_PR_abrt:
			user_info = acse_apdu->choice.abrt.user_information;
			break;
		case ACSE_apdu_PR_rlre:
			user_info = acse_apdu->choice.rlre.user_information;
			break;
		case ACSE_apdu_PR_rlrq:
			user_info = acse_apdu->choice.rlrq.user_information;
			break;
		default:
			break;
	}
	debug_print(D_PROTO, "calling-AE-qualifier: %ld\n", ae_qualifier);
	if(user_info == NULL) {
		debug_print(D_PROTO, "No user-information field\n");
		goto end;           // empty payload is not an error
	}
	if(user_info->data.encoding.present != EXTERNALt__encoding_PR_arbitrary) {
		debug_print(D_PROTO, "unsupported encoding: %d\n", user_info->data.encoding.present);
		goto end;
	}
	debug_print(D_PROTO, "Decoding %d octets as arbitrary payload\n",
			user_info->data.encoding.choice.arbitrary.size);
	next_node = arbitrary_payload_parse(ae_qualifier, acse_apdu->present,
			user_info->data.encoding.choice.arbitrary.buf,
			user_info->data.encoding.choice.arbitrary.size,
			msg_type);
	if(next_node == NULL) {
		debug_print(D_PROTO, "Could not decode as arbitrary payload, returning unknown_proto_pdu\n");
		next_node = unknown_proto_pdu_new(user_info->data.encoding.choice.arbitrary.buf,
				user_info->data.encoding.choice.arbitrary.size);
	}
end:
	node = la_proto_node_new();
	node->td = &proto_DEF_x227_acse_apdu;
	node->data = apdu;
	node->next = next_node;
	return node;
fail:
	ASN_STRUCT_FREE(asn_DEF_ACSE_apdu, acse_apdu);
	return NULL;        // the caller will convert this to unknown_proto_pdu
}

static la_proto_node *fully_encoded_data_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	Fully_encoded_data_t *fed = NULL;
	la_proto_node *node = NULL;
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, &asn_DEF_Fully_encoded_data, (void **)&fed, buf, len);
	if(rval.code != RC_OK) {
		debug_print(D_PROTO, "uper_decode_complete() failed at position %ld\n", (long)rval.consumed);
		goto end;
	}
#ifdef DEBUG
	if(Config.debug_filter & D_PROTO_DETAIL) {
		asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);
	}
#endif
	debug_print(D_PROTO, "%ld bytes consumed, %ld left\n", (long)rval.consumed, (long)(len) - (long)rval.consumed);

	if(fed->data.presentation_data_values.present != PDV_list__presentation_data_values_PR_arbitrary) {
		debug_print(D_PROTO, "unsupported encoding of fully-encoded-data\n");
		goto end;
	}
	switch(fed->data.presentation_context_identifier) {
		case Presentation_context_identifier_acse_apdu:
			debug_print(D_PROTO, "Decoding %d octets as ACSE APDU\n",
					fed->data.presentation_data_values.choice.arbitrary.size);
			node = ulcs_acse_parse(fed->data.presentation_data_values.choice.arbitrary.buf,
					fed->data.presentation_data_values.choice.arbitrary.size,
					msg_type);
			break;
		case Presentation_context_identifier_user_ase_apdu:
			// AE-qualifier and ACSE APDU type are unknown here
			debug_print(D_PROTO, "Decoding %d octets as arbitrary payload\n",
					fed->data.presentation_data_values.choice.arbitrary.size);
			node = arbitrary_payload_parse(ICAO_APP_TYPE_UNKNOWN, ACSE_apdu_PR_NOTHING,
					fed->data.presentation_data_values.choice.arbitrary.buf,
					fed->data.presentation_data_values.choice.arbitrary.size,msg_type);
			break;
		default:
			debug_print(D_PROTO, "unsupported presentation-context-identifier: %ld\n",
					fed->data.presentation_context_identifier);
			break;
	}
end:
	// Fully-encoded-data is not stored in the resulting la_proto_node,
	// so it must be destroyed here.
	ASN_STRUCT_FREE(asn_DEF_Fully_encoded_data, fed);
	return node;
}

/********************************************************************************
  * X.225 Session Protocol / X.226 Presentation Protocol
*********************************************************************************/

#define X225_SPDU_SCN  0xe8
#define X225_SPDU_SAC  0xf0
#define X225_SPDU_SACC 0xd8
#define X225_SPDU_SRF  0xe0
#define X225_SPDU_SRFC 0xa0

static la_dict const x225_spdu_names[] = {
	{ X225_SPDU_SCN,  "Short Connect" },
	{ X225_SPDU_SAC,  "Short Accept" },
	{ X225_SPDU_SACC, "Short Accept Continue" },
	{ X225_SPDU_SRF,  "Short Refuse" },
	{ X225_SPDU_SRFC, "Short Refuse Continue" },
	{ 0, NULL }
};

static la_proto_node *x225_spdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	la_proto_node *node = NULL;
	la_proto_node *next_node = NULL;
	NEW(x225_spdu_t, spdu);
	uint8_t *ptr = buf;
	uint32_t remaining = len;

	uint8_t spdu_id = ptr[0] & 0xf8;
	if(la_dict_search(x225_spdu_names, spdu_id) == NULL) {
		debug_print(D_PROTO, "Unknown SPDU type 0x%02x\n", spdu_id);
		goto fail;
	}
	// p-bit must be 0, as SPDU parameters are not supported in the ATN
	// (Doc 9880 2.4.5.2.2)
	if(ptr[0] & 4) {
		debug_print(D_PROTO, "Unexpected p-bit value 1\n");
		goto fail;
	}
	spdu->spdu_special_data = ptr[0] & 0x3;
	spdu->spdu_id = spdu_id;

	ptr++; remaining--;
	if(remaining < 1) {
		goto end;
	}

	// The next octet shall then contain a X.226 Amdt 1 (1997) Presentation layer protocol
	// control information. We only care about two least significant bits, which carry
	// encoding information - 0x2 indicates ASN.1 encoded with Packed Encoding Rules
	// Unaligned (X.691)
	if((ptr[0] & 3) != 2) {
		debug_print(D_PROTO, "Unknown PPDU payload encoding: %u\n", ptr[1] & 3);
		goto fail;
	}
	ptr++; remaining--;
	if(remaining < 1) {
		goto end;
	}
	// Decode as ICAO Doc 9705 / X.227 ACSE APDU
	debug_print(D_PROTO, "Decoding %d octets as ACSE APDU\n", remaining);
	next_node = ulcs_acse_parse(ptr, remaining, msg_type);
	if(next_node == NULL) {
		debug_print(D_PROTO, "Could not decode as ACSE APDU, returning unknown_proto_pdu\n");
		next_node = unknown_proto_pdu_new(ptr, remaining);
	}
end:
	node = la_proto_node_new();
	node->td = &proto_DEF_x225_spdu;
	node->data = spdu;
	node->next = next_node;
	return node;
fail:
	XFREE(spdu);
	return NULL;    // the caller will convert this to unknown_proto_pdu
}

void x225_spdu_format_text(la_vstring *vstr, void const *data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	x225_spdu_t const *spdu = data;
	char const *str = la_dict_search(x225_spdu_names, spdu->spdu_id);
	if(str != NULL) {
		LA_ISPRINTF(vstr, indent, "X.225 Session SPDU: %s\n", str);
	} else {
		LA_ISPRINTF(vstr, indent, "X.225 Session SPDU: unknown type (0x%02x)\n",
				spdu->spdu_id);
	}
	if(spdu->spdu_id == X225_SPDU_SRF) {
		LA_ISPRINTF(vstr, indent+1, "Refusal: %s\n",
				(spdu->spdu_special_data & 1 ? "persistent" : "transient"));
		LA_ISPRINTF(vstr, indent+1, "Transport connection: %s\n",
				(spdu->spdu_special_data & 2 ? "release" : "retain"));
	}
}

void x225_spdu_format_json(la_vstring *vstr, void const *data) {
	ASSERT(vstr != NULL);
	ASSERT(data);

	x225_spdu_t const *spdu = data;
	la_json_append_int64(vstr, "spdu_id", spdu->spdu_id);
	char const *str = la_dict_search(x225_spdu_names, spdu->spdu_id);
	SAFE_JSON_APPEND_STRING(vstr, "spdu_type", str);
	if(spdu->spdu_id == X225_SPDU_SRF) {
		la_json_append_string(vstr, "refusal",
				(spdu->spdu_special_data & 1 ? "persistent" : "transient"));
		la_json_append_string(vstr, "transport_connection",
				(spdu->spdu_special_data & 2 ? "release" : "retain"));
	}
}

la_type_descriptor const proto_DEF_x225_spdu = {
	.format_text    = x225_spdu_format_text,
	.format_json    = x225_spdu_format_json,
	.json_key       = "x225_spdu",
	.destroy        = NULL
};

/********************************************************************************
  * Main application layer decoding routine
*********************************************************************************/

la_proto_node *icao_apdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	la_proto_node *node = NULL;
	if(len < 1) {
		debug_print(D_PROTO, "APDU too short (len: %u)\n", len);
		goto end;
	}
	uint8_t *ptr = buf;
	uint32_t remaining = len;

	// Check if it's a X.225 Amdt 1 (1997) Short-form SPDU.
	// All SPDU types have the 8-th bit of SI&P field (the first octet) set to 1.
	uint8_t spdu_id = ptr[0] & 0xf8;
	if((spdu_id & 0x80) != 0) {
		debug_print(D_PROTO, "Decoding %d octets as X.225 SPDU\n", remaining);
		node = x225_spdu_parse(ptr, remaining, msg_type);
	} else {
		// Long-Form SPDUs are not used in the ATN, hence this must be a NULL encoding of Session
		// Layer and Presentation Layer, ie. only user data field is present without any header.
		// Try decoding it as Fully-encoded-data first
		debug_print(D_PROTO, "Decoding %d octets as Fully-encoded-data\n", remaining);
		node = fully_encoded_data_parse(ptr, remaining, msg_type);
		// If it failed, then try decoding as X.227 ACSE APDU as a last resort.
		// A notable example of ACSE APDUs carried in COTP user data field without
		// X.225 SPDU header are CPDLC Aborts carried in COTP DR TPDUs.
		if(node == NULL) {
			debug_print(D_PROTO, "Failed to decode as Fully-encoded-data, decoding %d octets as X.225 SPDU\n",
					remaining);
			node = ulcs_acse_parse(ptr, remaining, msg_type);
		}
	}
end:
	return node ? node : unknown_proto_pdu_new(buf, len);
}

la_type_descriptor const proto_DEF_cpdlc = {
	.format_text    = asn1_pdu_format_text,
	.format_json    = asn1_pdu_format_json,
	.json_key       = "cpdlc",
	.destroy        = asn1_pdu_destroy
};

la_type_descriptor const proto_DEF_cm = {
	.format_text    = asn1_pdu_format_text,
	.format_json    = asn1_pdu_format_json,
	.json_key       = "context_mgmt",
	.destroy        = asn1_pdu_destroy
};

la_type_descriptor const proto_DEF_adsc_v2 = {
	.format_text    = asn1_pdu_format_text,
	.format_json    = asn1_pdu_format_json,
	.json_key       = "adsc_v2",
	.destroy        = asn1_pdu_destroy
};

la_type_descriptor const proto_DEF_x227_acse_apdu = {
	.format_text    = asn1_pdu_format_text,
	.format_json    = asn1_pdu_format_json,
	.json_key       = "x227_apdu",
	.destroy        = asn1_pdu_destroy
};
