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

#ifndef _ASN1_FORMAT_ICAO_H
#define _ASN1_FORMAT_ICAO_H

#include <libacars/asn1-util.h>                 // la_asn1_formatter_func
#include <libacars/dict.h>                      // la_dict

// asn1-format-icao-text.c
extern la_dict const Associate_result_labels[];
extern la_dict const Release_request_reason_labels[];
extern la_dict const Release_response_reason_labels[];
extern la_dict const RevisionReason_labels[];
extern la_dict const RunwayUse_labels[];
extern la_dict const TransferConstraints_bit_labels[];
extern la_dict const ABRT_source_labels[];
extern la_dict const ATCUplinkMsgElementId_labels[];
extern la_dict const VerticalType_bit_labels[];
extern la_dict const ReportTypeNotSupported_bit_labels[];
extern la_dict const EPPLimitations_bit_labels[];
extern la_dict const EventTypeNotSupported_bit_labels[];
extern la_dict const EmergencyUrgencyStatus_bit_labels[];
extern la_dict const ATCDownlinkMsgElementId_labels[];
extern la_asn1_formatter const asn1_icao_formatter_table_text[];
extern size_t asn1_icao_formatter_table_text_len;
extern la_asn1_formatter const asn1_acse_formatter_table_text[];
extern size_t asn1_acse_formatter_table_text_len;

// asn1-format-icao-json.c
extern la_asn1_formatter const asn1_icao_formatter_table_json[];
extern size_t asn1_icao_formatter_table_json_len;
extern la_asn1_formatter const asn1_acse_formatter_table_json[];
extern size_t asn1_acse_formatter_table_json_len;

#endif // !_ASN1_FORMAT_ICAO_H
