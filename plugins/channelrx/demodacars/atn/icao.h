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
#include <stdint.h>
#include <stdbool.h>
#include <libacars/libacars.h>		// la_proto_node
#include "asn1/constr_TYPE.h"

typedef struct {
	uint8_t spdu_id;
	uint8_t spdu_special_data;
} x225_spdu_t;

// app-type values for ATN applications
#define ICAO_APP_TYPE_ADS	0
#define ICAO_APP_TYPE_CMA	1
#define ICAO_APP_TYPE_CPC	22
#define ICAO_APP_TYPE_UNKNOWN	-1

// icao.c
la_proto_node *icao_apdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
