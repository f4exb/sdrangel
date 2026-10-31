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

#include <gmodule.h>                            // GByteArray
#include <libacars/dict.h>                      // la_dict
#include <libacars/vstring.h>                   // la_vstring, LA_ISPRINTF()
#include "asn1/ACSE-apdu.h"                     // asn_DEF_ACSE_apdu
#include "asn1/ABRT-source.h"                   // ABRT_source_*
#include "asn1/ATCDownlinkMessage.h"            // ATCDownlinkMessage_t and dependencies
#include "asn1/ATCUplinkMessage.h"              // ATCUplinkMessage_t and dependencies
#include "asn1/Associate-result.h"              // Associate_result_*
#include "asn1/CMAircraftMessage.h"             // asn_DEF_AircraftMessage
#include "asn1/CMContactRequest.h"              // asn_DEF_CMContactRequest
#include "asn1/CMGroundMessage.h"               // asn_DEF_CMGroundMessage
#include "asn1/ProtectedAircraftPDUs.h"         // asn_DEF_ProtectedAircraftPDUs
#include "asn1/ProtectedGroundPDUs.h"           // asn_DEF_ProtectedGroundPDUs
#include "asn1/ADSAircraftPDU.h"                // asn_DEF_ADSAircraftPDU
#include "asn1/ADSAircraftPDUs.h"               // asn_DEF_ADSAircraftPDUs
#include "asn1/ADSAccept.h"                     // asn_DEF_ADSAccept
#include "asn1/ADSGroundPDU.h"                  // asn_DEF_ADSGroundPDU
#include "asn1/ADSGroundPDUs.h"                 // asn_DEF_ADSGroundPDUs
#include "asn1/ADSNonCompliance.h"              // asn_DEF_ADSNonCompliance
#include "asn1/ADSPositiveAcknowledgement.h"    // asn_DEF_ADSPositiveAcknowledgement
#include "asn1/ADSRequestContract.h"            // asn_DEF_ADSRequestContract
#include "asn1/ADSReject.h"                     // asn_DEF_ADSReject
#include "asn1/ADSReport.h"                     // asn_DEF_ADSReport
#include "asn1/Release-request-reason.h"        // Release_request_reason_*
#include "asn1/Release-response-reason.h"       // Release_response_reason_*
#include "dumpvdl2.h"                           // XCALLOC, dict_search()
#include <libacars/asn1-util.h>                 // la_asn1_formatter_func, la_asn1_output()
#include <libacars/asn1-format-common.h>        // common formatters and helper functions

// forward declarations
la_asn1_formatter const asn1_icao_formatter_table_text[];
size_t asn1_icao_formatter_table_text_len;
la_asn1_formatter const asn1_acse_formatter_table_text[];
size_t asn1_acse_formatter_table_text_len;

la_dict const Associate_result_labels[] = {
	{ .id = Associate_result_accepted, .val = "accept" },
	{ .id = Associate_result_rejected_permanent, .val = "reject (permanent)" },
	{ .id = Associate_result_rejected_transient, .val = "reject (transient)" },
	{ .id = 0, .val = NULL }
};

la_dict const Release_request_reason_labels[] = {
	{ .id = Release_request_reason_normal, .val = "normal" },
	{ .id = Release_request_reason_urgent, .val = "urgent" },
	{ .id = Release_request_reason_user_defined, .val = "user defined" },
	{ .id = 0, .val = NULL }
};

la_dict const Release_response_reason_labels[] = {
	{ .id = Release_response_reason_normal, .val = "normal" },
	{ .id = Release_response_reason_not_finished, .val = "not finished" },
	{ .id = Release_response_reason_user_defined, .val = "user defined" },
	{ .id = 0, .val = NULL }
};

la_dict const ABRT_source_labels[] = {
	{ .id = ABRT_source_acse_service_user, .val = "user" },
	{ .id = ABRT_source_acse_service_provider, .val = "provider" },
	{ .id = 0, .val = NULL }
};

la_dict const ATCUplinkMsgElementId_labels[] = {
	{ ATCUplinkMsgElementId_PR_uM0NULL, "UNABLE" },
	{ ATCUplinkMsgElementId_PR_uM1NULL, "STANDBY" },
	{ ATCUplinkMsgElementId_PR_uM2NULL, "REQUEST DEFERRED" },
	{ ATCUplinkMsgElementId_PR_uM3NULL, "ROGER" },
	{ ATCUplinkMsgElementId_PR_uM4NULL, "AFFIRM" },
	{ ATCUplinkMsgElementId_PR_uM5NULL, "NEGATIVE" },
	{ ATCUplinkMsgElementId_PR_uM6Level, "EXPECT [level]" },
	{ ATCUplinkMsgElementId_PR_uM7Time, "EXPECT HIGHER AT TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM8Position, "EXPECT CLIMB AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM9Time, "EXPECT LOWER AT TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM10Position, "EXPECT DESCENT AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM11Time, "EXPECT CRUISE CLIMB AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM12Position, "EXPECT CRUISE CLIMB AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM13TimeLevel, "AT [time] EXPECT CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM14PositionLevel, "AT [position] EXPECT CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM15TimeLevel, "AT [time] EXPECT DESCENT TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM16PositionLevel, "AT [position] EXPECT DESCENT TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM17TimeLevel, "AT [time] EXPECT CRUISE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM18PositionLevel, "AT [position] EXPECT CRUISE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM19Level, "MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM20Level, "CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM21TimeLevel, "AT TIME [time] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM22PositionLevel, "AT [position] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM23Level, "DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM24TimeLevel, "AT [time] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM25PositionLevel, "AT [position] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM26LevelTime, "CLIMB TO REACH [level] BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM27LevelPosition, "CLIMB TO REACH [level] BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM28LevelTime, "DESCEND TO REACH [level] BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM29LevelPosition, "DESCEND TO REACH [level] BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM30LevelLevel, "MAINTAIN BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM31LevelLevel, "CLIMB TO AND MAINTAIN BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM32LevelLevel, "DESCEND TO AND MAINTAIN BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM33NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM34Level, "CRUISE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM35Level, "CRUISE CLIMB ABOVE [level]" },
	{ ATCUplinkMsgElementId_PR_uM36Level, "EXPEDITE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM37Level, "EXPEDITE DESCENT TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM38Level, "IMMEDIATELY CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM39Level, "IMMEDIATELY DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM40NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM41NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM42PositionLevel, "EXPECT TO CROSS [position] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM43PositionLevel, "EXPECT TO CROSS [position] AT OR ABOVE [level]" },
	{ ATCUplinkMsgElementId_PR_uM44PositionLevel, "EXPECT TO CROSS [position] AT OR BELOW [level]" },
	{ ATCUplinkMsgElementId_PR_uM45PositionLevel, "EXPECT TO CROSS [position] AT AND MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM46PositionLevel, "CROSS [position] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM47PositionLevel, "CROSS [position] AT OR ABOVE [level]" },
	{ ATCUplinkMsgElementId_PR_uM48PositionLevel, "CROSS [position] AT OR BELOW [level]" },
	{ ATCUplinkMsgElementId_PR_uM49PositionLevel, "CROSS [position] AT AND MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM50PositionLevelLevel, "CROSS [position] BETWEEN [level] AND [level]" },
	{ ATCUplinkMsgElementId_PR_uM51PositionTime, "CROSS [position] AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM52PositionTime, "CROSS [position] AT OR BEFORE [time]" },
	{ ATCUplinkMsgElementId_PR_uM53PositionTime, "CROSS [position] AT OR AFTER [time]" },
	{ ATCUplinkMsgElementId_PR_uM54PositionTimeTime, "CROSS [position] BETWEEN [time] AND [time]" },
	{ ATCUplinkMsgElementId_PR_uM55PositionSpeed, "CROSS [position] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM56PositionSpeed, "CROSS [position] AT OR LESS THAN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM57PositionSpeed, "CROSS [position] AT OR GREATER THAN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM58PositionTimeLevel, "CROSS [position] AT [time] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM59PositionTimeLevel, "CROSS [position] AT OR BEFORE [time] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM60PositionTimeLevel, "CROSS [position] AT OR AFTER [time] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM61PositionLevelSpeed, "CROSS [position] AT AND MAINTAIN [level] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM62TimePositionLevel, "AT [time] CROSS [position] AT AND MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM63TimePositionLevelSpeed, "AT [time] CROSS [position] AT AND MAINTAIN [level] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM64DistanceSpecifiedDirection, "OFFSET [distance] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM65PositionDistanceSpecifiedDirection, "AT [position] OFFSET [distance] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM66TimeDistanceSpecifiedDirection, "AT [time] OFFSET [distance] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM67NULL, "REJOIN ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM68Position, "REJOIN ROUTE BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM69Time, "REJOIN ROUTE BEFORE TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM70Position, "EXPECT BACK ON ROUTE BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM71Time, "EXPECT BACK ON ROUTE BEFORE TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM72NULL, "RESUME OWN NAVIGATION" },
	{ ATCUplinkMsgElementId_PR_uM73DepartureClearance, "[departure clearance]" },
	{ ATCUplinkMsgElementId_PR_uM74Position, "PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM75Position, "WHEN ABLE PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM76TimePosition, "AT [time] PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM77PositionPosition, "AT [position] PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM78LevelPosition, "AT [level] PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM79PositionRouteClearance, "CLEARED TO [position] VIA [route clearance]" },
	{ ATCUplinkMsgElementId_PR_uM80RouteClearance, "CLEARED [route clearance]" },
	{ ATCUplinkMsgElementId_PR_uM81ProcedureName, "CLEARED [procedure name]" },
	{ ATCUplinkMsgElementId_PR_uM82DistanceSpecifiedDirection, "CLEARED TO DEVIATE UP TO [distance] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM83PositionRouteClearance, "AT [position] CLEARED [route clearance]" },
	{ ATCUplinkMsgElementId_PR_uM84PositionProcedureName, "AT [position] CLEARED [procedure name]" },
	{ ATCUplinkMsgElementId_PR_uM85RouteClearance, "EXPECT [route clearance]" },
	{ ATCUplinkMsgElementId_PR_uM86PositionRouteClearance, "AT [position] EXPECT [route clearance]" },
	{ ATCUplinkMsgElementId_PR_uM87Position, "EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM88PositionPosition, "AT [position] EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM89TimePosition, "AT [time] EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM90LevelPosition, "AT [level] EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM91HoldClearance, "HOLD AT [position] MAINTAIN [level] INBOUND TRACK [degrees] [direction] TURNS [leg type]" },
	{ ATCUplinkMsgElementId_PR_uM92PositionLevel, "HOLD AT [position] AS PUBLISHED MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM93Time, "EXPECT FURTHER CLEARANCE AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM94DirectionDegrees, "TURN [direction] HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM95DirectionDegrees, "TURN [direction] GROUND TRACK [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM96NULL, "CONTINUE PRESENT HEADING" },
	{ ATCUplinkMsgElementId_PR_uM97PositionDegrees, "AT [position] FLY HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM98DirectionDegrees, "IMMEDIATELY TURN [direction] HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM99ProcedureName, "EXPECT [procedure name]" },
	{ ATCUplinkMsgElementId_PR_uM100TimeSpeed, "AT [time] EXPECT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM101PositionSpeed, "AT [position] EXPECT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM102LevelSpeed, "AT [level] EXPECT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM103TimeSpeedSpeed, "AT [time] EXPECT [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM104PositionSpeedSpeed, "AT [position] EXPECT [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM105LevelSpeedSpeed, "AT [level] EXPECT [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM106Speed, "MAINTAIN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM107NULL, "MAINTAIN PRESENT SPEED" },
	{ ATCUplinkMsgElementId_PR_uM108Speed, "MAINTAIN [speed] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_uM109Speed, "MAINTAIN [speed] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM110SpeedSpeed, "MAINTAIN [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM111Speed, "INCREASE SPEED TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM112Speed, "INCREASE SPEED TO [speed] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_uM113Speed, "REDUCE SPEED TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM114Speed, "REDUCE SPEED TO [speed] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM115Speed, "DO NOT EXCEED [speed]" },
	{ ATCUplinkMsgElementId_PR_uM116NULL, "RESUME NORMAL SPEED" },
	{ ATCUplinkMsgElementId_PR_uM117UnitNameFrequency, "CONTACT [unit name] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM118PositionUnitNameFrequency, "AT [position] CONTACT [unit name] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM119TimeUnitNameFrequency, "AT [time] CONTACT [unit name] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM120UnitNameFrequency, "MONITOR [unit name] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM121PositionUnitNameFrequency, "AT [position] MONITOR [unit name] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM122TimeUnitNameFrequency, "AT [time] MONITOR [unit name] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM123Code, "SQUAWK [SSR code]" },
	{ ATCUplinkMsgElementId_PR_uM124NULL, "STOP SQUAWK" },
	{ ATCUplinkMsgElementId_PR_uM125NULL, "SQUAWK MODE CHARLIE" },
	{ ATCUplinkMsgElementId_PR_uM126NULL, "STOP SQUAWK MODE CHARLIE" },
	{ ATCUplinkMsgElementId_PR_uM127NULL, "REPORT BACK ON ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM128Level, "REPORT LEAVING [level]" },
	{ ATCUplinkMsgElementId_PR_uM129Level, "REPORT MAINTAINING [level]" },
	{ ATCUplinkMsgElementId_PR_uM130Position, "REPORT PASSING [position]" },
	{ ATCUplinkMsgElementId_PR_uM131NULL, "REPORT ENDURANCE AND PERSONS ON BOARD" },
	{ ATCUplinkMsgElementId_PR_uM132NULL, "REPORT POSITION" },
	{ ATCUplinkMsgElementId_PR_uM133NULL, "REPORT PRESENT LEVEL" },
	{ ATCUplinkMsgElementId_PR_uM134SpeedTypeSpeedTypeSpeedType, "REPORT [speed type] [speed type] [speed type] SPEED" },
	{ ATCUplinkMsgElementId_PR_uM135NULL, "CONFIRM ASSIGNED LEVEL" },
	{ ATCUplinkMsgElementId_PR_uM136NULL, "CONFIRM ASSIGNED SPEED" },
	{ ATCUplinkMsgElementId_PR_uM137NULL, "CONFIRM ASSIGNED ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM138NULL, "CONFIRM TIME OVER REPORTED WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM139NULL, "CONFIRM REPORTED WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM140NULL, "CONFIRM NEXT WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM141NULL, "CONFIRM NEXT WAYPOINT ETA" },
	{ ATCUplinkMsgElementId_PR_uM142NULL, "CONFIRM ENSUING WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM143NULL, "CONFIRM REQUEST" },
	{ ATCUplinkMsgElementId_PR_uM144NULL, "CONFIRM SQUAWK CODE" },
	{ ATCUplinkMsgElementId_PR_uM145NULL, "REPORT HEADING" },
	{ ATCUplinkMsgElementId_PR_uM146NULL, "REPORT GROUND TRACK" },
	{ ATCUplinkMsgElementId_PR_uM147NULL, "REQUEST POSITION REPORT" },
	{ ATCUplinkMsgElementId_PR_uM148Level, "WHEN CAN YOU ACCEPT [level]" },
	{ ATCUplinkMsgElementId_PR_uM149LevelPosition, "CAN YOU ACCEPT [level] AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM150LevelTime, "CAN YOU ACCEPT [level] AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM151Speed, "WHEN CAN YOU ACCEPT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM152DistanceSpecifiedDirection, "WHEN CAN YOU ACCEPT [distance] [direction] OFFSET" },
	{ ATCUplinkMsgElementId_PR_uM153Altimeter, "ALTIMETER [altimeter]" },
	{ ATCUplinkMsgElementId_PR_uM154NULL, "SURVEILLANCE SERVICE TERMINATED" },
	{ ATCUplinkMsgElementId_PR_uM155Position, "RADAR CONTACT [position]" },
	{ ATCUplinkMsgElementId_PR_uM156NULL, "IDENTIFICATION LOST" },
	{ ATCUplinkMsgElementId_PR_uM157Frequency, "CHECK STUCK MICROPHONE [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM158AtisCode, "ATIS [atis code]" },
	{ ATCUplinkMsgElementId_PR_uM159ErrorInformation, "ERROR [error information]" },
	{ ATCUplinkMsgElementId_PR_uM160Facility, "NEXT DATA AUTHORITY ([facility designation])" },
	{ ATCUplinkMsgElementId_PR_uM161NULL, "END SERVICE" },
	{ ATCUplinkMsgElementId_PR_uM162NULL, "MESSAGE NOT SUPPORTED BY THIS ATC UNIT" },
	{ ATCUplinkMsgElementId_PR_uM163FacilityDesignation, "[facility designation]" },
	{ ATCUplinkMsgElementId_PR_uM164NULL, "WHEN READY" },
	{ ATCUplinkMsgElementId_PR_uM165NULL, "THEN" },
	{ ATCUplinkMsgElementId_PR_uM166TrafficType, "DUE TO [traffic type] TRAFFIC" },
	{ ATCUplinkMsgElementId_PR_uM167NULL, "DUE TO AIRSPACE RESTRICTION" },
	{ ATCUplinkMsgElementId_PR_uM168NULL, "DISREGARD" },
	{ ATCUplinkMsgElementId_PR_uM169FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM170FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM171VerticalRate, "CLIMB AT [vertical rate] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_uM172VerticalRate, "CLIMB AT [vertical rate] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM173VerticalRate, "DESCEND AT [vertical rate] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_uM174VerticalRate, "DESCEND AT [vertical rate] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM175Level, "REPORT REACHING [level]" },
	{ ATCUplinkMsgElementId_PR_uM176NULL, "MAINTAIN OWN SEPARATION AND VMC" },
	{ ATCUplinkMsgElementId_PR_uM177NULL, "AT PILOTS DISCRETION" },
	{ ATCUplinkMsgElementId_PR_uM178NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM179NULL, "SQUAWK IDENT" },
	{ ATCUplinkMsgElementId_PR_uM180LevelLevel, "REPORT REACHING BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM181ToFromPosition, "REPORT DISTANCE [to/from] [position]" },
	{ ATCUplinkMsgElementId_PR_uM182NULL, "CONFIRM ATIS CODE" },
	{ ATCUplinkMsgElementId_PR_uM183FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM184TimeToFromPosition, "AT [time] REPORT DISTANCE [to/from] [position]" },
	{ ATCUplinkMsgElementId_PR_uM185PositionLevel, "AFTER PASSING [position] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM186PositionLevel, "AFTER PASSING [position] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM187FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM188PositionSpeed, "AFTER PASSING [position] MAINTAIN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM189Speed, "ADJUST SPEED TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM190Degrees, "FLY HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM191NULL, "ALL ATS TERMINATED" },
	{ ATCUplinkMsgElementId_PR_uM192LevelTime, "REACH [level] BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM193NULL, "IDENTIFICATION LOST" },
	{ ATCUplinkMsgElementId_PR_uM194FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM195FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM196FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM197FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM198FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM199FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM200NULL, "REPORT MAINTAINING" },
	{ ATCUplinkMsgElementId_PR_uM201NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM202NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM203FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM204FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM205FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM206FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM207FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM208FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM209LevelPosition, "REACH [level] BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM210Position, "IDENTIFIED [position]" },
	{ ATCUplinkMsgElementId_PR_uM211NULL, "REQUEST FORWARDED" },
	{ ATCUplinkMsgElementId_PR_uM212FacilityDesignationATISCode, "[facility designation] ATIS [atis code] CURRENT" },
	{ ATCUplinkMsgElementId_PR_uM213FacilityDesignationAltimeter, "[facility designation] ALTIMETER [altimeter]" },
	{ ATCUplinkMsgElementId_PR_uM214RunwayRVR, "RVR RUNWAY [runway] [rvr]" },
	{ ATCUplinkMsgElementId_PR_uM215DirectionDegrees, "TURN [direction] [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM216NULL, "REQUEST FLIGHT PLAN" },
	{ ATCUplinkMsgElementId_PR_uM217NULL, "REPORT LANDING" },
	{ ATCUplinkMsgElementId_PR_uM218NULL, "REQUEST ALREADY RECEIVED" },
	{ ATCUplinkMsgElementId_PR_uM219Level, "STOP CLIMB AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM220Level, "STOP DESCENT AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM221Degrees, "STOP TURN HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM222NULL, "NO SPEED RESTRICTION" },
	{ ATCUplinkMsgElementId_PR_uM223NULL, "REDUCE TO MINIMUM APPROACH SPEED" },
	{ ATCUplinkMsgElementId_PR_uM224NULL, "NO DELAY EXPECTED" },
	{ ATCUplinkMsgElementId_PR_uM225NULL, "DELAY NOT DETERMINED" },
	{ ATCUplinkMsgElementId_PR_uM226Time, "EXPECTED APPROACH TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM227NULL, "LOGICAL ACKNOWLEDGEMENT" },
	{ ATCUplinkMsgElementId_PR_uM228Position, "REPORT ETA [position]" },
	{ ATCUplinkMsgElementId_PR_uM229NULL, "ADVISE ALTERNATE AERODROME" },
	{ ATCUplinkMsgElementId_PR_uM230NULL, "IMMEDIATELY" },
	{ ATCUplinkMsgElementId_PR_uM231NULL, "ADVISE PREFERRED LEVEL" },
	{ ATCUplinkMsgElementId_PR_uM232NULL, "STATE TOP OF DESCENT" },
	{ ATCUplinkMsgElementId_PR_uM233NULL, "USE OF LOGICAL ACKNOWLEDGEMENT PROHIBITED" },
	{ ATCUplinkMsgElementId_PR_uM234NULL, "FLIGHT PLAN NOT HELD" },
	{ ATCUplinkMsgElementId_PR_uM235NULL, "ROGER 7500" },
	{ ATCUplinkMsgElementId_PR_uM236NULL, "LEAVE CONTROLLED AIRSPACE" },
	{ ATCUplinkMsgElementId_PR_uM237NULL, "REQUEST AGAIN WITH NEXT ATC UNIT" },
	{ ATCUplinkMsgElementId_PR_null238, "Reserved: SECONDARY FREQUENCY [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM239, "STOP ADS-B TRANSMISSION" },
	{ ATCUplinkMsgElementId_PR_null240, "Reserved: TRANSMIT ADS-B ALTITUDE" },
	{ ATCUplinkMsgElementId_PR_null241, "Reserved: STOP ADS-B ALTITUDE TRANSMISSION" },
	{ ATCUplinkMsgElementId_PR_null242, "Reserved: TRANSMIT ADS-B IDENT" },
	{ ATCUplinkMsgElementId_PR_uM243, "REPORT CLEAR OF WEATHER" },
	{ ATCUplinkMsgElementId_PR_null244, "Reserved: IDENTIFICATION TERMINATED" },
	{ ATCUplinkMsgElementId_PR_uM245, "EXPEDITE" },
	{ ATCUplinkMsgElementId_PR_uM246, "WHEN ABLE" },
	{ ATCUplinkMsgElementId_PR_uM247, "REST OF ROUTE UNCHANGED" },
	{ ATCUplinkMsgElementId_PR_uM248, "[deviation type] DEVIATION DETECTED. VERIFY AND ADVISE" },
	{ ATCUplinkMsgElementId_PR_uM249, "REVISED ([revision reason])" },
	{ ATCUplinkMsgElementId_PR_uM250, "CONFIRM ADS-C EMERGENCY" },
	{ ATCUplinkMsgElementId_PR_uM251, "URGENT ATC MESSAGE" },
	{ ATCUplinkMsgElementId_PR_uM252, "AFTER TIME [time] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM253, "AFTER TIME [time] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM254, "AT [position ATW] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM255, "AT [position ATW] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM256, "REQUESTED LEVEL IS NOT AVAILABLE" },
	{ ATCUplinkMsgElementId_PR_uM257, "HIGHER LEVEL IS NOT AVAILABLE" },
	{ ATCUplinkMsgElementId_PR_uM258, "LOWER LEVEL IS NOT AVAILABLE" },
	{ ATCUplinkMsgElementId_PR_uM259, "LEAVE [level] BEFORE TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM260, "CLIMB VIA [named instruction]" },
	{ ATCUplinkMsgElementId_PR_uM261, "DESCEND VIA [named instruction]" },
	{ ATCUplinkMsgElementId_PR_uM262, "AFTER PASSING [positionR] CLIMB VIA [named instruction]" },
	{ ATCUplinkMsgElementId_PR_uM263, "AFTER PASSING [positionR] DESCEND VIA [named instruction]" },
	{ ATCUplinkMsgElementId_PR_uM264, "EXPECT [level] [number of minutes] AFTER DEPARTURE" },
	{ ATCUplinkMsgElementId_PR_uM265, "CANCEL TIME CONSTRAINT FOR [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM266, "AT [position ATW] CLEARED TO [positionR] VIA [route clearanceR]" },
	{ ATCUplinkMsgElementId_PR_uM267, "CLEARED TO DEVIATE UP TO [number of degrees] DEGREES [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM268, "AT [level] CLEARED TO DEVIATE UP TO [lateral deviation] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM269, "HAZARDOUS WEATHER [hzwx specification]" },
	{ ATCUplinkMsgElementId_PR_uM270, "EXPECT [clearance typeR] [assigned time]" },
	{ ATCUplinkMsgElementId_PR_uM271, "CURRENT ATC UNIT [unit nameR]" },
	{ ATCUplinkMsgElementId_PR_uM272, "CPDLC NOT IN USE UNTIL FURTHER NOTIFICATION" },
	{ ATCUplinkMsgElementId_PR_uM273, "CPDLC BACK IN USE" },
	{ ATCUplinkMsgElementId_PR_uM274, "REQUEST AGAIN AFTER PASSING [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM275, "REQUEST AGAIN AFTER TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM276, "REQUEST AGAIN AFTER REACHING [level]" },
	{ ATCUplinkMsgElementId_PR_uM277, "TRAFFIC IS [traffic description]" },
	{ ATCUplinkMsgElementId_PR_uM278, "REPORT SIGHTING AND PASSING OPPOSITE DIRECTION ([aircraft type]) [traffic location] ([etp time])" },
	{ ATCUplinkMsgElementId_PR_uM279, "ADVISE REQUIRED RVR" },
	{ ATCUplinkMsgElementId_PR_uM280, "WHEN WILL YOU MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM281, "REPORT FLIGHT CONDITIONS" },
	{ ATCUplinkMsgElementId_PR_uM282, "WHICH LEVEL DO YOU PREFER [level] OR [level]" },
	{ ATCUplinkMsgElementId_PR_uM283, "WHEN CAN YOU ACCEPT [clearance typeR]" },
	{ ATCUplinkMsgElementId_PR_uM284, "REDUCE TO MINIMUM CLEAN SPEED" },
	{ ATCUplinkMsgElementId_PR_uM285, "MAINTAIN MAXIMUM SPEED" },
	{ ATCUplinkMsgElementId_PR_uM286, "MAINTAIN MINIMUM SPEED" },
	{ ATCUplinkMsgElementId_PR_uM287, "AT [level] MAINTAIN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM288, "MAINTAIN [speed schedule] IN THE CLIMB" },
	{ ATCUplinkMsgElementId_PR_uM289, "MAINTAIN [speed schedule] IN THE DESCENT" },
	{ ATCUplinkMsgElementId_PR_uM290, "MAINTAIN [speed IAS mach] IN THE CRUISE" },
	{ ATCUplinkMsgElementId_PR_uM291, "INCREASE SPEED [speed delta]" },
	{ ATCUplinkMsgElementId_PR_uM292, "REDUCE SPEED [speed delta]" },
	{ ATCUplinkMsgElementId_PR_uM293, "MAINTAIN TIME CONSTRAINT" },
	{ ATCUplinkMsgElementId_PR_uM294, "MONITOR SECONDARY FREQUENCY [unit nameR] [frequencyR]" },
	{ ATCUplinkMsgElementId_PR_uM295, "RESPOND TO CPDLC MESSAGES" },
	{ ATCUplinkMsgElementId_PR_uM296, "EXPECT CPDLC TRANSFER AT TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM297, "CPDLC WITH [unit nameR] NOT REQUIRED EXPECT NEXT CPDLC FACILITY [unit nameR]" },
	{ ATCUplinkMsgElementId_PR_uM298, "ACTIVATE ADS-C" },
	{ ATCUplinkMsgElementId_PR_uM299, "ADS-C OUT OF SERVICE REVERT TO VOICE POSITION REPORTS" },
	{ ATCUplinkMsgElementId_PR_uM300, "RELAY TO [aircraft identification] [unit nameR] [relay text] ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM301, "LATENCY TIME VALUE [latency value]" },
	{ ATCUplinkMsgElementId_PR_uM302, "STARTUP APPROVED ([assigned time])" },
	{ ATCUplinkMsgElementId_PR_uM303, "CANCEL STARTUP" },
	{ ATCUplinkMsgElementId_PR_uM304, "PUSHBACK APPROVED ([pushback position]) ([assigned time])" },
	{ ATCUplinkMsgElementId_PR_uM305, "EXPECT TAXI [taxi route] ([taxi duration])" },
	{ ATCUplinkMsgElementId_PR_uM306, "RESUME TAXI ([taxi resume condition])" },
	{ ATCUplinkMsgElementId_PR_uM307, "CROSS [ground location]" },
	{ ATCUplinkMsgElementId_PR_uM308, "([runway]) TAXI [taxi route]" },
	{ ATCUplinkMsgElementId_PR_uM309, "DE-ICING APPROVED" },
	{ ATCUplinkMsgElementId_PR_uM310, "WHEN REACHING [ground location]" },
	{ ATCUplinkMsgElementId_PR_uM311, "HOLD POSITION" },
	{ ATCUplinkMsgElementId_PR_uM312, "FOR DE-ICING" },
	{ ATCUplinkMsgElementId_PR_uM313, "CAN YOU ACCEPT INTERSECTION [intersection] FOR DEPARTURE RUNWAY [runway] ([distance ground] AVAILABLE)" },
	{ ATCUplinkMsgElementId_PR_uM314, "DEPARTURES STOPPED" },
	{ ATCUplinkMsgElementId_PR_uM315, "ENGINE SHUTDOWN PERMITTED" },
	{ ATCUplinkMsgElementId_PR_uM316, "CANCEL HOLD [ground location]" },
	{ ATCUplinkMsgElementId_PR_uM317, "([runway]) INTERSECTION DEPARTURE [intersection] ([distance ground] AVAILABLE)" },
	{ ATCUplinkMsgElementId_PR_uM318, "HOLD SHORT [ground location]" },
	{ ATCUplinkMsgElementId_PR_uM319, "ITP [ITP reference aircraft list]" },
	{ ATCUplinkMsgElementId_PR_uM320, "MESSAGE RECEIVED TOO LATE, RESEND MESSAGE OR CONTACT BY VOICE" },
	{ ATCUplinkMsgElementId_PR_uM321, "CONFIRM [transfer constraints] [unit nameR]" },
	{ ATCUplinkMsgElementId_PR_uM322, "CROSS [position ATW] AT TIME [RTA timesec] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM323, "CROSS [position ATW] AT TIME [RTA timesec] AT [speed] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM324, "CROSS [position ATW] AT TIME [RTA timesec] AT [speed] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_null325, "Reserved: SELECT TRAFFIC [aircraft flight identification]" },
	{ ATCUplinkMsgElementId_PR_null326, "Reserved: CONFIRM SELECTED TRAFFIC" },
	{ ATCUplinkMsgElementId_PR_null327, "Reserved: REPORT WHEN TRAFFIC SELECTED" },
	{ ATCUplinkMsgElementId_PR_null328, "Reserved: FOR INTERVAL SPACING MAINTAIN CURRENT [im spacing interval type] SPACING BEHIND [aircraft flight identification]" },
	{ ATCUplinkMsgElementId_PR_null329, "Reserved: FOR INTERVAL SPACING CAPTURE THEN MAINTAIN [im spacing interval] BEHIND [aircraft flight identification] ([im reference aircraft routing]) (TERMINATE AT [position ATW])" },
	{ ATCUplinkMsgElementId_PR_null330, "Reserved: FOR INTERVAL SPACING (TURN TO INTERCEPT [position ATW] TO) CROSS [position ATW] [im spacing] BEHIND [aircraft flight identification] ([im reference aircraft routing]) (TERMINATE AT [position ATW])" },
	{ ATCUplinkMsgElementId_PR_null331, "Reserved: EXPECT INTERVAL SPACING TO CROSS [position ATW] BEHIND [aircraft flight identification] ([im reference aircraft routing]) (TERMINATE AT [position ATW]) ASSIGNED SPACING INTERVAL PENDING" },
	{ ATCUplinkMsgElementId_PR_null332, "Reserved: REPORT STARTING INTERVAL SPACING" },
	{ ATCUplinkMsgElementId_PR_null333, "Reserved: ADVISE PLANNED FINAL APPROACH SPEED" },
	{ ATCUplinkMsgElementId_PR_null334, "Reserved: CONTINUE INTERVAL SPACING BEHIND [aircraft flight identification]" },
	{ ATCUplinkMsgElementId_PR_null335, "Reserved: CONFIRM ASSIGNED SPACING INTERVAL BEHIND [aircraft flight identification]" },
	{ ATCUplinkMsgElementId_PR_null336, "Reserved: REPORT CURRENT [im spacing interval type] SPACING INTERVAL BEHIND [aircraft flight identification]" },
	{ ATCUplinkMsgElementId_PR_null337, "Reserved: SUSPEND INTERVAL SPACING (BEHIND [aircraft flight identification])" },
	{ ATCUplinkMsgElementId_PR_null338, "Reserved: RESUME INTERVAL SPACING (BEHIND [aircraft flight identification])" },
	{ ATCUplinkMsgElementId_PR_null339, "Reserved: CANCEL INTERVAL SPACING (BEHIND [aircraft flight identification])" },
	{ ATCUplinkMsgElementId_PR_uM8R, "EXPECT HIGHER AT [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM10R, "EXPECT LOWER AT [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM22R, "AFTER PASSING [position ATW] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM25R, "AFTER PASSING [position ATW] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM26R, "CLIMB TO REACH [level] BEFORE TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM27R, "CLIMB TO REACH [level] BEFORE PASSING [position ATW]" },
	{ ATCUplinkMsgElementId_PR_uM28R, "DESCEND TO REACH [level] BEFORE TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM29R, "DESCEND TO REACH [level] BEFORE PASSING [position ATW]" },
	{ ATCUplinkMsgElementId_PR_uM46R, "CROSS [position ATW] (AT) (BETWEEN [level] AND) [level]" },
	{ ATCUplinkMsgElementId_PR_uM47R, "CROSS [position ATW] AT OR ABOVE [level]" },
	{ ATCUplinkMsgElementId_PR_uM48R, "CROSS [position ATW] AT OR BELOW [level]" },
	{ ATCUplinkMsgElementId_PR_uM51R, "CROSS [position ATW] AT TIME [RTA timesec]" },
	{ ATCUplinkMsgElementId_PR_uM52R, "CROSS [position ATW] BEFORE TIME [RTA timesec]" },
	{ ATCUplinkMsgElementId_PR_uM53R, "CROSS [position ATW] AFTER TIME [RTA timesec]" },
	{ ATCUplinkMsgElementId_PR_uM54R, "CROSS [position ATW] BETWEEN TIME [RTA timesec] AND TIME [RTA timesec]" },
	{ ATCUplinkMsgElementId_PR_uM55R, "CROSS [position ATW] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM56R, "CROSS [position ATW] AT [speed] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM57R, "CROSS [position ATW] AT [speed] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_uM58R, "CROSS [position ATW] AT TIME [RTA timesec] (AT) (BETWEEN [level] AND) [level]" },
	{ ATCUplinkMsgElementId_PR_uM59R, "CROSS [position ATW] BEFORE TIME [RTA timesec] (AT) (BETWEEN [level] AND) [level]" },
	{ ATCUplinkMsgElementId_PR_uM60R, "CROSS [position ATW] AFTER TIME [RTA timesec] (AT) (BETWEEN [level] AND) [level]" },
	{ ATCUplinkMsgElementId_PR_uM61R, "CROSS [position ATW] (AT) (BETWEEN [level] AND) [level] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM63R, "CROSS [position ATW] AT TIME [RTA timesec] (AT) (BETWEEN [level] AND) [level] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM64R, "OFFSET [distanceR] [direction side] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM65R, "AT [position ATW] OFFSET [distanceR] [direction side] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM66R, "AT TIME [time] OFFSET [distanceR] [direction side] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM68R, "REJOIN ROUTE BEFORE PASSING [position ATW]" },
	{ ATCUplinkMsgElementId_PR_uM70R, "EXPECT BACK ON ROUTE BEFORE PASSING [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM73R, "[departure clearanceR]" },
	{ ATCUplinkMsgElementId_PR_uM74R, "PROCEED DIRECT TO [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM76R, "AT TIME [time] PROCEED DIRECT TO [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM77R, "AT [position ATW] PROCEED DIRECT TO [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM78R, "AT [level] PROCEED DIRECT TO [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM79R, "CLEARED TO [positionR] VIA ([departure data]) [route clearanceR]" },
	{ ATCUplinkMsgElementId_PR_uM80R, "CLEARED ([departure data]) [route clearanceR] [arrival approach data]" },
	{ ATCUplinkMsgElementId_PR_uM81R, "CLEARED [procedure nameR]" },
	{ ATCUplinkMsgElementId_PR_uM82R, "CLEARED TO DEVIATE UP TO [lateral deviation] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM83R, "AT [position ATW] CLEARED [route clearanceR] [arrival approach data]" },
	{ ATCUplinkMsgElementId_PR_uM84R, "AT [positionR] CLEARED [procedure nameR]" },
	{ ATCUplinkMsgElementId_PR_uM91R, "AT [positionR] HOLD INBOUND TRACK [degrees] [direction side] TURNS [leg typeR] LEGS" },
	{ ATCUplinkMsgElementId_PR_uM92R, "AT [positionR] HOLD ([direction compass]) AS PUBLISHED" },
	{ ATCUplinkMsgElementId_PR_uM94R, "TURN [direction side] HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM95R, "TURN [direction side] GROUND TRACK [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM97R, "AT [position ATW] FLY HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM99R, "EXPECT [named instruction]" },
	{ ATCUplinkMsgElementId_PR_uM100R, "EXPECT SPEED CHANGE AT TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM101R, "EXPECT SPEED CHANGE AT [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM102R, "EXPECT SPEED CHANGE AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM116R, "RESUME NORMAL SPEED ([flight phase])" },
	{ ATCUplinkMsgElementId_PR_uM117R, "CONTACT [unit nameR] ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM118R, "AT [position ground air] CONTACT [unit nameR] ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM119R, "AT TIME [time] CONTACT [unit nameR] ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM120R, "MONITOR [unit nameR] ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM121R, "AT [position ground air] MONITOR [unit nameR] ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM122R, "AT TIME [time] MONITOR [unit nameR] ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM128R, "REPORT LEAVING [level]" },
	{ ATCUplinkMsgElementId_PR_uM129R, "REPORT MAINTAINING [level]" },
	{ ATCUplinkMsgElementId_PR_uM130R, "REPORT PASSING [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM134R, "REPORT [speed types] SPEED" },
	{ ATCUplinkMsgElementId_PR_uM148R, "WHEN CAN YOU ACCEPT [level]" },
	{ ATCUplinkMsgElementId_PR_uM149R, "CAN YOU ACCEPT [level] AT [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM150R, "CAN YOU ACCEPT [level] AT TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM153R, "([facility designation]) ALTIMETER [altimeter setting] (ALTIMETER TIME [time])" },
	{ ATCUplinkMsgElementId_PR_uM155R, "IDENTIFIED ([positionR])" },
	{ ATCUplinkMsgElementId_PR_uM157R, "CHECK STUCK MICROPHONE ([frequencyR])" },
	{ ATCUplinkMsgElementId_PR_uM159R, "ERROR [error informationR]" },
	{ ATCUplinkMsgElementId_PR_uM166R, "DUE TO [reason]" },
	{ ATCUplinkMsgElementId_PR_uM180R, "REPORT REACHING BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM188R, "AFTER PASSING [positionR] MAINTAIN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM192R, "REACH [level] BEFORE TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM209R, "REACH [level] BEFORE PASSING [position ATW]" },
	{ ATCUplinkMsgElementId_PR_uM158R, "([airport]) ATIS [atis code]" },
	{ ATCUplinkMsgElementId_PR_uM214R, "RVR ([airport]) ([runway]) [rvrData]" },
	{ ATCUplinkMsgElementId_PR_uM215R, "TURN [direction side] [number of degrees] DEGREES" },
	{ ATCUplinkMsgElementId_PR_uM219R, "STOP CLIMB AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM220R, "STOP DESCENT AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM233R, "LOGICAL ACKNOWLEDGEMENT STATUS [lack status]" },
	{ ATCUplinkMsgElementId_PR_uM236R, "LEAVE CONTROLLED AIRSPACE ([leave instruction])" },
	{ ATCUplinkMsgElementId_PR_uM238R, "SECONDARY FREQUENCY [frequencyR]" },
	{ ATCUplinkMsgElementId_PR_uM340, "CANCEL SPEED CONSTRAINT FOR [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM341, "CANCEL LEVEL CONSTRAINT FOR [positionR]" },
	{ ATCUplinkMsgElementId_PR_uM342, "CLIMB AT [vertical rate]" },
	{ ATCUplinkMsgElementId_PR_uM343, "DESCEND AT [vertical rate]" },
	{ ATCUplinkMsgElementId_PR_uM344, "PUSHBACK AT PILOTS DISCRETION" },
	{ 0, NULL }
};

la_dict const VerticalType_bit_labels[] = {
	{ 0, "top of climb" },
	{ 1, "top of descent" },
	{ 2, "start of climb" },
	{ 3, "start of descent" },
	{ 4, "start of level" },
	{ 5, "start of speed change" },
	{ 6, "end of speed change" },
	{ 7, "speed limit" },
	{ 8, "cross over" },
	{ 0, NULL }
};

la_dict const ReportTypeNotSupported_bit_labels[] = {
	{ 0, "projected profile" },
	{ 1, "ground vector" },
	{ 2, "air vector" },
	{ 3, "meteo info" },
	{ 4, "extended projected profile" },
	{ 5, "ToA range" },
	{ 6, "speed schedule profile" },
	{ 7, "RNP profile" },
	{ 8, "planned final approach speed" },
	{ 0, NULL }
};

la_dict const EPPLimitations_bit_labels[] = {
	{ 0, "requested distance tolerance not supported" },
	{ 1, "requested level tolerance not supported" },
	{ 2, "requested time tolerance not supported" },
	{ 3, "requested speed tolerance not supported" },
	{ 0, NULL }
};

la_dict const EventTypeNotSupported_bit_labels[] = {
	{ 0, "lateral deviations" },
	{ 1, "vertical rate deviations" },
	{ 2, "level range deviations" },
	{ 3, "way point changes" },
	{ 4, "air speed changes" },
	{ 5, "ground speed changes" },
	{ 6, "EPP flight plan changes" },
	{ 7, "EPP next waypoint in horizon" },
	{ 8, "EPP tolerance changes" },
	{ 9, "RTA status changes" },
	{ 10, "FoM changes" },
	{ 11, "level changes" },
	{ 12, "vertical clearance deviations" },
	{ 13, "airspeed range deviations" },
	{ 14, "turbulence deviations" },
	{ 15, "RNP not met" },
	{ 16, "planned final approach speed changes" },
	{ 0, NULL }
};

la_dict const EmergencyUrgencyStatus_bit_labels[] = {
	{ 0, "emergency" },
	{ 1, "reserved0" },
	{ 2, "unlawful-interference" },
	{ 3, "reserved1" },
	{ 4, "reserved2" },
	{ 5, "emergency-cancelled" },
	{ 0, NULL }

};

la_dict const TransferConstraints_bit_labels[] = {
	{ 0, "assigned level" },
	{ 1, "assigned heading" },
	{ 2, "assigned speed" },
	{ 3, "conducting interval spacing" },
	{ 0, NULL }
};

la_dict const ATCDownlinkMsgElementId_labels[] = {
	{ ATCDownlinkMsgElementId_PR_dM0NULL, "WILCO" },
	{ ATCDownlinkMsgElementId_PR_dM1NULL, "UNABLE" },
	{ ATCDownlinkMsgElementId_PR_dM2NULL, "STANDBY" },
	{ ATCDownlinkMsgElementId_PR_dM3NULL, "ROGER" },
	{ ATCDownlinkMsgElementId_PR_dM4NULL, "AFFIRM" },
	{ ATCDownlinkMsgElementId_PR_dM5NULL, "NEGATIVE" },
	{ ATCDownlinkMsgElementId_PR_dM6Level, "REQUEST [level]" },
	{ ATCDownlinkMsgElementId_PR_dM7LevelLevel, "REQUEST BLOCK [level] TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM8Level, "REQUEST CRUISE CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM9Level, "REQUEST CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM10Level, "REQUEST DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM11PositionLevel, "AT [position] REQUEST CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM12PositionLevel, "AT [position] REQUEST DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM13TimeLevel, "AT TIME [time] REQUEST [level]" },
	{ ATCDownlinkMsgElementId_PR_dM14TimeLevel, "AT [time] REQUEST DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM15DistanceSpecifiedDirection, "REQUEST OFFSET [distance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM16PositionDistanceSpecifiedDirection, "AT [position] REQUEST OFFSET [distance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM17TimeDistanceSpecifiedDirection, "AT [time] REQUEST OFFSET [distance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM18Speed, "REQUEST [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM19SpeedSpeed, "REQUEST [speed] TO [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM20NULL, "REQUEST VOICE CONTACT" },
	{ ATCDownlinkMsgElementId_PR_dM21Frequency, "REQUEST VOICE CONTACT [frequency]" },
	{ ATCDownlinkMsgElementId_PR_dM22Position, "REQUEST DIRECT TO [position]" },
	{ ATCDownlinkMsgElementId_PR_dM23ProcedureName, "REQUEST [procedure name]" },
	{ ATCDownlinkMsgElementId_PR_dM24RouteClearance, "REQUEST CLEARANCE [route clearance]" },
	{ ATCDownlinkMsgElementId_PR_dM25ClearanceType, "REQUEST [clearance type] CLEARANCE" },
	{ ATCDownlinkMsgElementId_PR_dM26PositionRouteClearance, "REQUEST WEATHER DEVIATION TO [position] VIA [route clearance]" },
	{ ATCDownlinkMsgElementId_PR_dM27DistanceSpecifiedDirection, "REQUEST WEATHER DEVIATION UP TO [distance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM28Level, "LEAVING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM29Level, "CLIMBING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM30Level, "DESCENDING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM31Position, "PASSING [position]" },
	{ ATCDownlinkMsgElementId_PR_dM32Level, "PRESENT LEVEL [level]" },
	{ ATCDownlinkMsgElementId_PR_dM33Position, "PRESENT POSITION [position]" },
	{ ATCDownlinkMsgElementId_PR_dM34Speed, "PRESENT SPEED [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM35Degrees, "PRESENT HEADING [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM36Degrees, "PRESENT GROUND TRACK [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM37Level, "MAINTAINING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM38Level, "ASSIGNED LEVEL [level]" },
	{ ATCDownlinkMsgElementId_PR_dM39Speed, "ASSIGNED SPEED [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM40RouteClearance, "ASSIGNED ROUTE [route clearance]" },
	{ ATCDownlinkMsgElementId_PR_dM41NULL, "BACK ON ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM42Position, "NEXT WAYPOINT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM43Time, "NEXT WAYPOINT ETA [time]" },
	{ ATCDownlinkMsgElementId_PR_dM44Position, "ENSUING WAYPOINT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM45Position, "REPORTED WAYPOINT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM46Time, "REPORTED WAYPOINT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM47Code, "SQUAWKING [discrete beacon code]" },
	{ ATCDownlinkMsgElementId_PR_dM48PositionReport, "POSITION REPORT [position report]" },
	{ ATCDownlinkMsgElementId_PR_dM49Speed, "WHEN CAN WE EXPECT [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM50SpeedSpeed, "WHEN CAN WE EXPECT [speed] TO [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM51NULL, "WHEN CAN WE EXPECT BACK ON ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM52NULL, "WHEN CAN WE EXPECT LOWER LEVEL" },
	{ ATCDownlinkMsgElementId_PR_dM53NULL, "WHEN CAN WE EXPECT HIGHER LEVEL" },
	{ ATCDownlinkMsgElementId_PR_dM54Level, "WHEN CAN WE EXPECT CRUISE CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM55NULL, "PAN PAN PAN" },
	{ ATCDownlinkMsgElementId_PR_dM56NULL, "MAYDAY MAYDAY MAYDAY" },
	{ ATCDownlinkMsgElementId_PR_dM57RemainingFuelPersonsOnBoard, "[remaining fuel] OF FUEL REMAINING AND [persons on board] PERSONS ON BOARD" },
	{ ATCDownlinkMsgElementId_PR_dM58NULL, "CANCEL EMERGENCY" },
	{ ATCDownlinkMsgElementId_PR_dM59PositionRouteClearance, "DIVERTING TO [position] VIA [route clearance]" },
	{ ATCDownlinkMsgElementId_PR_dM60DistanceSpecifiedDirection, "OFFSETTING [distance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM61Level, "DESCENDING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM62ErrorInformation, "ERROR [error information]" },
	{ ATCDownlinkMsgElementId_PR_dM63NULL, "NOT CURRENT DATA AUTHORITY" },
	{ ATCDownlinkMsgElementId_PR_dM64FacilityDesignation, "[facility designation]" },
	{ ATCDownlinkMsgElementId_PR_dM65NULL, "DUE TO WEATHER" },
	{ ATCDownlinkMsgElementId_PR_dM66NULL, "DUE TO AIRCRAFT PERFORMANCE" },
	{ ATCDownlinkMsgElementId_PR_dM67FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM68FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM69NULL, "REQUEST VMC DESCENT" },
	{ ATCDownlinkMsgElementId_PR_dM70Degrees, "REQUEST HEADING [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM71Degrees, "REQUEST GROUND TRACK [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM72Level, "REACHING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM73Versionnumber, "[version number]" },
	{ ATCDownlinkMsgElementId_PR_dM74NULL, "REQUEST TO MAINTAIN OWN SEPARATION AND VMC" },
	{ ATCDownlinkMsgElementId_PR_dM75NULL, "AT PILOTS DISCRETION" },
	{ ATCDownlinkMsgElementId_PR_dM76LevelLevel, "REACHING BLOCK [level] TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM77LevelLevel, "ASSIGNED BLOCK [level] TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM78TimeDistanceToFromPosition, "AT [time] [distance] [to/from] [position]" },
	{ ATCDownlinkMsgElementId_PR_dM79AtisCode, "ATIS [atis code]" },
	{ ATCDownlinkMsgElementId_PR_dM80DistanceSpecifiedDirection, "DEVIATING UP TO [distance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM81LevelTime, "WE CAN ACCEPT [level] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM82Level, "WE CANNOT ACCEPT [level]" },
	{ ATCDownlinkMsgElementId_PR_dM83SpeedTime, "WE CAN ACCEPT [speed] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM84Speed, "WE CANNOT ACCEPT [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM85DistanceSpecifiedDirectionTime, "WE CAN ACCEPT [distance] [direction] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM86DistanceSpecifiedDirection, "WE CANNOT ACCEPT [distance] [direction]" },
	{ ATCDownlinkMsgElementId_PR_dM87Level, "WHEN CAN WE EXPECT CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM88Level, "WHEN CAN WE EXPECT DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM89UnitnameFrequency, "MONITORING [unit name] [frequency]" },
	{ ATCDownlinkMsgElementId_PR_dM90FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM91FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM92FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM93FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM94FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM95FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM96FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM97FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM98FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM99NULL, "CURRENT DATA AUTHORITY" },
	{ ATCDownlinkMsgElementId_PR_dM100NULL, "LOGICAL ACKNOWLEDGEMENT" },
	{ ATCDownlinkMsgElementId_PR_dM101NULL, "REQUEST END OF SERVICE" },
	{ ATCDownlinkMsgElementId_PR_dM102NULL, "LANDING REPORT" },
	{ ATCDownlinkMsgElementId_PR_dM103NULL, "CANCELLING IFR" },
	{ ATCDownlinkMsgElementId_PR_dM104PositionTime, "ETA [position] [time]" },
	{ ATCDownlinkMsgElementId_PR_dM105Airport, "ALTERNATE AERODROME [airport]" },
	{ ATCDownlinkMsgElementId_PR_dM106Level, "PREFERRED LEVEL [level]" },
	{ ATCDownlinkMsgElementId_PR_dM107NULL, "NOT AUTHORIZED NEXT DATA AUTHORITY" },
	{ ATCDownlinkMsgElementId_PR_dM108NULL, "DE-ICING COMPLETE" },
	{ ATCDownlinkMsgElementId_PR_dM109Time, "TOP OF DESCENT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM110Position, "TOP OF DESCENT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM111TimePosition, "TOP OF DESCENT [time] [position]" },
	{ ATCDownlinkMsgElementId_PR_dM112NULL, "SQUAWKING 7500" },
	{ ATCDownlinkMsgElementId_PR_dM113SpeedTypeSpeedTypeSpeedTypeSpeed, "[speed type] [speed type] [speed type] SPEED [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM114, "CLEAR OF WEATHER" },
	{ ATCDownlinkMsgElementId_PR_dM115, "WE CAN ACCEPT [level] AT [positionR]" },
	{ ATCDownlinkMsgElementId_PR_dM116, "WE CAN ACCEPT [speed] AT [positionR]" },
	{ ATCDownlinkMsgElementId_PR_null117, "Reserved" },
	{ ATCDownlinkMsgElementId_PR_dM118, "REQUEST NORMAL SPEED" },
	{ ATCDownlinkMsgElementId_PR_dM119, "AT [positionR] REQUEST DIRECT TO [positionR]" },
	{ ATCDownlinkMsgElementId_PR_dM120, "REQUEST OCEANIC CLEARANCE ([OCL request])" },
	{ ATCDownlinkMsgElementId_PR_dM121, "WE WILL MAINTAIN [level] AT [positionR]" },
	{ ATCDownlinkMsgElementId_PR_dM122, "REQUEST FREQUENCY CHANGE" },
	{ ATCDownlinkMsgElementId_PR_dM123, "REQUEST RVR ([airport]) ([runway])" },
	{ ATCDownlinkMsgElementId_PR_dM124, "REQUIRED RVR ([airport]) ([runway]) [rvr data]" },
	{ ATCDownlinkMsgElementId_PR_dM125, "REQUEST DEPARTURE CLEARANCE [departure clearance request]" },
	{ ATCDownlinkMsgElementId_PR_null126, "Reserved: DE-ICING STARTED" },
	{ ATCDownlinkMsgElementId_PR_dM127, "FOR DE-ICING" },
	{ ATCDownlinkMsgElementId_PR_dM128, "ABLE INTERSECTION [intersection] FOR DEPARTURE RUNWAY [runway]" },
	{ ATCDownlinkMsgElementId_PR_dM129, "READY FOR [clearance typeR] [assigned time]" },
	{ ATCDownlinkMsgElementId_PR_dM130, "CANCELLING STARTUP" },
	{ ATCDownlinkMsgElementId_PR_dM131, "REQUEST PUSHBACK ([pushback position])" },
	{ ATCDownlinkMsgElementId_PR_dM132, "REQUEST DE-ICING ([ground location])" },
	{ ATCDownlinkMsgElementId_PR_dM133, "NO DE-ICING REQUIRED" },
	{ ATCDownlinkMsgElementId_PR_dM134, "REQUEST STARTUP" },
	{ ATCDownlinkMsgElementId_PR_dM135, "REQUEST TAXI ([taxi request])" },
	{ ATCDownlinkMsgElementId_PR_dM136, "REQUEST EXPECTED TAXI ROUTING ([ground location])" },
	{ ATCDownlinkMsgElementId_PR_dM137, "WE CAN ACCEPT [clearance typeR] [assigned time]" },
	{ ATCDownlinkMsgElementId_PR_dM138, "WE CANNOT ACCEPT [clearance typeR]" },
	{ ATCDownlinkMsgElementId_PR_dM139, "REQUEST [speed schedule] IN THE CLIMB" },
	{ ATCDownlinkMsgElementId_PR_dM140, "REQUEST [speed schedule] IN THE DESCENT" },
	{ ATCDownlinkMsgElementId_PR_dM141, "ITP [reference aircraft distance list]" },
	{ ATCDownlinkMsgElementId_PR_dM142, "TRAFFIC ([aircraft type]) [traffic location] [traffic visibility]" },
	{ ATCDownlinkMsgElementId_PR_dM143, "WE WILL MAINTAIN [level] AT TIME [time]" },
	{ ATCDownlinkMsgElementId_PR_dM144, "RELAY FROM [aircraft identification] [relayed text Response]" },
	{ ATCDownlinkMsgElementId_PR_dM145, "MESSAGE RECEIVED TOO LATE, RESEND MESSAGE OR CONTACT BY VOICE" },
	{ ATCDownlinkMsgElementId_PR_dM146, "AIRCRAFT CPDLC INHIBITED" },
	{ ATCDownlinkMsgElementId_PR_dM147, "ASSIGNED HEADING [degrees]" },
	{ ATCDownlinkMsgElementId_PR_null148, "Reserved: [aircraft flight identification] SELECTED" },
	{ ATCDownlinkMsgElementId_PR_null149, "Reserved: PLANNED FINAL APPROACH SPEED [speed]" },
	{ ATCDownlinkMsgElementId_PR_null150, "Reserved: STARTING INTERVAL SPACING BEHIND [aircraft flight identification]" },
	{ ATCDownlinkMsgElementId_PR_null151, "Reserved: ASSIGNED SPACING INTERVAL [im spacing] BEHIND [aircraft flight identification]" },
	{ ATCDownlinkMsgElementId_PR_null152, "Reserved: CURRENT SPACING INTERVAL [im spacing interval] BEHIND [aircraft flight identification]" },
	{ ATCDownlinkMsgElementId_PR_null153, "Reserved: UNABLE TO CONTINUE INTERVAL SPACING (BEHIND [aircraft flight identification])" },
	{ ATCDownlinkMsgElementId_PR_null154, "Reserved: CONDUCTING INTERVAL SPACING BEHIND [aircraft flight identification]" },
	{ ATCDownlinkMsgElementId_PR_dM11R, "AT [positionR] REQUEST [level]" },
	{ ATCDownlinkMsgElementId_PR_dM15R, "REQUEST OFFSET [distanceR] [direction side] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM21R, "REQUEST VOICE CONTACT ([frequencyR])" },
	{ ATCDownlinkMsgElementId_PR_dM22R, "REQUEST DIRECT TO [positionR]" },
	{ ATCDownlinkMsgElementId_PR_dM23R, "REQUEST [named instruction]" },
	{ ATCDownlinkMsgElementId_PR_dM24R, "REQUEST CLEARANCE ([departure data]) [route clearanceR] ([arrival approach data])" },
	{ ATCDownlinkMsgElementId_PR_dM25R, "REQUEST [clearance type request] CLEARANCE" },
	{ ATCDownlinkMsgElementId_PR_dM27R, "REQUEST WEATHER DEVIATION UP TO [lateral deviation] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM28R, "LEAVING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM29R, "CLIMBING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM30R, "DESCENDING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM31R, "PASSING [positionR]" },
	{ ATCDownlinkMsgElementId_PR_dM37R, "MAINTAINING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM40R, "ASSIGNED ROUTE ([departure data]) [route clearanceR] ([arrival approach data])" },
	{ ATCDownlinkMsgElementId_PR_dM57R, "[remaining fuel] ENDURANCE AND [persons on boardE] PERSONS ON BOARD" },
	{ ATCDownlinkMsgElementId_PR_dM59R, "DIVERTING TO [positionR] VIA [route clearanceR] ([arrival approach data])" },
	{ ATCDownlinkMsgElementId_PR_dM60R, "OFFSETTING [distanceR] [direction side] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM62R, "ERROR [error informationR]" },
	{ ATCDownlinkMsgElementId_PR_dM65R, "DUE TO [reason]" },
	{ ATCDownlinkMsgElementId_PR_dM76R, "REACHING BLOCK [level] TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM80R, "DEVIATING ([deviation]) [direction side] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM81R, "WE CAN ACCEPT [level] AT TIME [time]" },
	{ ATCDownlinkMsgElementId_PR_dM82R, "WE CANNOT ACCEPT [level]" },
	{ ATCDownlinkMsgElementId_PR_dM106R, "PREFERRED LEVEL [level]" },
	{ ATCDownlinkMsgElementId_PR_dM107R, "NOT AUTHORIZED NEXT DATA AUTHORITY [CDA] ([NDA])" },
	{ ATCDownlinkMsgElementId_PR_dM113R, "[speed types] SPEED [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM155, "MESSAGE NOT SUPPORTED BY THIS AIRCRAFT" },
	{ 0, NULL }
};

la_dict const RevisionReason_labels[] = {
	{ RevisionReason_PR_levelChange, "level change" },
	{ RevisionReason_PR_speedChange, "speed change" },
	{ RevisionReason_PR_routeChangeAtPosition, "route change at position" },
	{ RevisionReason_PR_routeChangeAtMultipleWaypoints, "route change at multiple waypoints" },
	{ RevisionReason_PR_entryPointChange, "entry point change" },
	{ RevisionReason_PR_clearanceLimitChange, "clearance limit change" },
	{ RevisionReason_PR_namedInstructionChange, "named instruction change" },
	{ RevisionReason_PR_groundLocationChange, "ground location change" },
	{ RevisionReason_PR_assignedSpacingIntervalChange, "assigned spacing interval change" },
	{ RevisionReason_PR_revisedIntervalSpacingTrafficRoute, "revised interval spacing traffic route" },
	{ RevisionReason_PR_plannedTerminationPointChange, "planned termination point change" },
	{ RevisionReason_PR_achieveByPointChange, "achieve-by point change" },
	{ 0, NULL }
};

la_dict const RunwayUse_labels[] = {
	{ RunwayUse_PR_requestFullLength, "request full length" },
	{ RunwayUse_PR_ableIntersection, "able intersection" }
};

/*************************************************
 * Helper functions used in ASN.1 type formatters
 *************************************************/

static GByteArray *_stringify_ShortTsap(GByteArray *array, ShortTsap_t const *tsap) {
	if(tsap->aRS != NULL) {
		array = g_byte_array_append(array, tsap->aRS->buf, tsap->aRS->size);
	}
	array = g_byte_array_append(array, tsap->locSysNselTsel.buf, tsap->locSysNselTsel.size);
	return array;
}

/************************
 * ASN.1 type formatters
 ************************/

LA_ASN1_FORMATTER_FUNC(asn1_output_acse_as_text) {
	la_asn1_output(p, asn1_acse_formatter_table_text, asn1_acse_formatter_table_text_len, true);
}

LA_ASN1_FORMATTER_FUNC(asn1_output_icao_as_text) {
	la_asn1_output(p, asn1_icao_formatter_table_text, asn1_icao_formatter_table_text_len, true);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SEQUENCE_acse_as_text) {
	la_format_SEQUENCE_as_text(p, asn1_output_acse_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_CHOICE_acse_as_text) {
	la_format_CHOICE_as_text(p, NULL, asn1_output_acse_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Associate_result_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, Associate_result_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Release_request_reason_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, Release_request_reason_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Release_response_reason_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, Release_response_reason_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ABRT_source_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, ABRT_source_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_CHOICE_icao_as_text) {
	la_format_CHOICE_as_text(p, NULL, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SEQUENCE_icao_as_text) {
	la_format_SEQUENCE_as_text(p, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SEQUENCE_OF_icao_as_text) {
	la_format_SEQUENCE_OF_as_text(p, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ATCDownlinkMsgElementId_as_text) {
	la_format_CHOICE_as_text(p, ATCDownlinkMsgElementId_labels, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ATCUplinkMsgElementId_as_text) {
	la_format_CHOICE_as_text(p, ATCUplinkMsgElementId_labels, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Code_as_text) {
	Code_t const *code = p.sptr;
	long **cptr = code->list.array;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %ld%ld%ld%ld\n",
			p.label,
			*cptr[0],
			*cptr[1],
			*cptr[2],
			*cptr[3]
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DateTime_as_text) {
	DateTime_t const *dtg = p.sptr;
	Date_t const *d = &dtg->date;
	Time_t const *t = &dtg->time;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld\n", p.label,
			d->year, d->month, d->day,
			t->hours, t->minutes);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DateTimeGroup_as_text) {
	DateTimeGroup_t const *dtg = p.sptr;
	Date_t const *d = &dtg->date;
	Timehhmmss_t const *t = &dtg->timehhmmss;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n", p.label,
			d->year, d->month, d->day,
			t->hoursminutes.hours, t->hoursminutes.minutes, t->seconds);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Time_as_text) {
	Time_t const *t = p.sptr;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %02ld:%02ld\n", p.label, t->hours, t->minutes);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Latitude_as_text) {
	Latitude_t const *lat = p.sptr;
	long const ldir = lat->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LatitudeDirection, ldir);
	switch(lat->type.present) {
		case LatitudeType_PR_degrees:
			LA_ISPRINTF(p.vstr, p.indent, "%s:   %02ld %s\n",
					p.label,
					lat->type.choice.degrees,
					ldir_name
					);
			break;
		case LatitudeType_PR_degreesMinutes:
			LA_ISPRINTF(p.vstr, p.indent, "%s:   %02ld %05.2f' %s\n",
					p.label,
					lat->type.choice.degreesMinutes.wholeDegrees,
					lat->type.choice.degreesMinutes.minutes / 100.0,
					ldir_name
					);
			break;
		case LatitudeType_PR_dMS:
			LA_ISPRINTF(p.vstr, p.indent, "%s:   %02ld %02ld' %02ld\" %s\n",
					p.label,
					lat->type.choice.dMS.wholeDegrees,
					lat->type.choice.dMS.wholeMinutes,
					lat->type.choice.dMS.seconds,
					ldir_name
					);
			break;
		case LatitudeType_PR_NOTHING:
		default:
			LA_ISPRINTF(p.vstr, p.indent, "%s: none\n", p.label);
			break;
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LatitudeR_as_text) {
	LatitudeR_t const *lat = p.sptr;
	long const ldir = lat->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LatitudeDirection, ldir);
	LatitudeDegreesMinutesR_t const *latdegminR = &lat->degreesMinutes;
	LA_ISPRINTF(p.vstr, p.indent, "%s:   %02ld %04.1f' %s\n",
			p.label,
			latdegminR->degrees,
			latdegminR->minutes / 10.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GroundLatitude_as_text) {
	GroundLatitude_t const *lat = p.sptr;
	long const ldir = lat->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LatitudeDirection, ldir);
	LA_ISPRINTF(p.vstr, p.indent, "%s:   %08.5f %s\n",
			p.label,
			lat->latitude / 10000.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Longitude_as_text) {
	Longitude_t const *lon = p.sptr;
	long const ldir = lon->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LongitudeDirection, ldir);
	switch(lon->type.present) {
		case LongitudeType_PR_degrees:
			LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %s\n",
					p.label,
					lon->type.choice.degrees,
					ldir_name
					);
			break;
		case LongitudeType_PR_degreesMinutes:
			LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %05.2f' %s\n",
					p.label,
					lon->type.choice.degreesMinutes.wholeDegrees,
					lon->type.choice.degreesMinutes.minutes / 100.0,
					ldir_name
					);
			break;
		case LongitudeType_PR_dMS:
			LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %02ld' %02ld\" %s\n",
					p.label,
					lon->type.choice.dMS.wholeDegrees,
					lon->type.choice.dMS.wholeMinutes,
					lon->type.choice.dMS.seconds,
					ldir_name
					);
			break;
		case LongitudeType_PR_NOTHING:
		default:
			LA_ISPRINTF(p.vstr, p.indent, "%s: none\n", p.label);
			break;
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LongitudeR_as_text) {
	LongitudeR_t const *lon = p.sptr;
	long const ldir = lon->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LongitudeDirection, ldir);
	LongitudeDegreesMinutesR_t const *londegminR = &lon->degreesMinutes;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %04.1f' %s\n",
			p.label,
			londegminR->degrees,
			londegminR->minutes / 10.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GroundLongitude_as_text) {
	GroundLongitude_t const *lat = p.sptr;
	long const ldir = lat->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LatitudeDirection, ldir);
	LA_ISPRINTF(p.vstr, p.indent, "%s:  %09.5f %s\n",
			p.label,
			lat->longitude / 10000.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_AircraftWeightEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " lbs", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_AircraftWeightMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kg", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_AltimeterEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " inHg", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_AltimeterMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " hPa", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Deg_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " deg", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DepartureMinimumInterval_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceKm_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km", 0.25, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceNm_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Humidity_as_text) {
	la_format_INTEGER_with_unit_as_text(p, "%%", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceSpecifiedKmR_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceSpecifiedNmR_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Frequencyvhf_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " MHz", 0.005, 3);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Frequencyuhf_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " MHz", 0.025, 3);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Frequencyhf_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kHz", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LegDistanceEnglishR_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LegDistanceMetricR_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LegTime_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LegTimeR_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LevelFeet_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceFeet_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LevelFlightLevelMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Meters_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalDistanceFeet_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft", 500, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalDistanceMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m", 200, 0);
}

// RejectDetails is a CHOICE whose all values are NULLs.  Aliasing them all to
// unique types just to print them with la_asn1_format_label_only_as_text would be an
// unnecessary overengineering.  Handling all values in a single routine is
// simpler, albeit less elegant at first glance.
static LA_ASN1_FORMATTER_FUNC(asn1_format_RejectDetails_as_text) {
	RejectDetails_t const *det = p.sptr;
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	}
	switch(det->present) {
		case RejectDetails_PR_aDS_service_unavailable:
			la_vstring_append_sprintf(p.vstr, "ADS service unavailable\n");
			break;
		case RejectDetails_PR_undefined_reason:
			la_vstring_append_sprintf(p.vstr, "undefined reason\n");
			break;
		case RejectDetails_PR_maximum_capacity_exceeded:
			la_vstring_append_sprintf(p.vstr, "max. capacity exceeded\n");
			break;
		case RejectDetails_PR_reserved:
			la_vstring_append_sprintf(p.vstr, "(reserved)\n");
			break;
		case RejectDetails_PR_waypoint_in_request_not_on_the_route:
			la_vstring_append_sprintf(p.vstr, "requested waypoint not on the route\n");
			break;
		case RejectDetails_PR_aDS_contract_not_supported:
			la_vstring_append_sprintf(p.vstr, "ADS contract not supported\n");
			break;
		case RejectDetails_PR_noneOfReportTypesSupported:
			la_vstring_append_sprintf(p.vstr, "none of report types supported\n");
			break;
		case RejectDetails_PR_noneOfEventTypesSupported:
			la_vstring_append_sprintf(p.vstr, "none of event types supported\n");
			break;
		case RejectDetails_PR_NOTHING:
		default:
			la_vstring_append_sprintf(p.vstr, "none\n");
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ReportingRate_as_text) {
	ReportingRate_t const *rate = p.sptr;
	switch(rate->present) {
		case ReportingRate_PR_reporting_time_seconds_scale:
			p.sptr = &rate->choice.reporting_time_seconds_scale;
			la_format_INTEGER_with_unit_as_text(p, " sec", 1, 0);
			break;
		case ReportingRate_PR_reporting_time_minutes_scale:
			p.sptr = &rate->choice.reporting_time_minutes_scale;
			la_format_INTEGER_with_unit_as_text(p, " min", 1, 0);
			break;
		default:
			break;
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RevisionReason_as_text) {
	la_format_CHOICE_as_text(p, RevisionReason_labels, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RunwayUse_as_text) {
	la_format_CHOICE_as_text(p, RunwayUse_labels, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RTASecTolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " sec", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RTATolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Feet_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km/h", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedIndicated_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedDelta_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedMach_as_text) {
	la_format_INTEGER_with_unit_as_text(p, "", 0.001, 3);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Temperature_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " C", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TimerValue_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " sec", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalRateEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft/min", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalRateMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m/min", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LongTsap_as_text) {
	LongTsap_t const *tsap = p.sptr;
	GByteArray *tmparray = g_byte_array_new();
	tmparray = g_byte_array_append(tmparray, tsap->rDP.buf, tsap->rDP.size);
	tmparray = _stringify_ShortTsap(tmparray, &tsap->shortTsap);

	LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	octet_string_with_ascii_format_text(p.vstr,
			&(octet_string_t){ .buf = tmparray->data, .len = tmparray->len },
			0);
	EOL(p.vstr);
	g_byte_array_free(tmparray, TRUE);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ShortTsap_as_text) {
	ShortTsap_t const *tsap = p.sptr;
	GByteArray *tmparray = g_byte_array_new();
	tmparray = _stringify_ShortTsap(tmparray, tsap);
	LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	octet_string_with_ascii_format_text(p.vstr,
			&(octet_string_t){ .buf = tmparray->data, .len = tmparray->len },
			0);
	EOL(p.vstr);
	g_byte_array_free(tmparray, TRUE);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_UnitName_as_text) {
	UnitName_t const *un = p.sptr;
	char *fdes = XCALLOC(un->designation.size + 1, sizeof(char));
	snprintf(fdes, un->designation.size + 1, "%s", un->designation.buf);
	char *fname = NULL;
	FacilityName_t *fn = un->name;
	if(fn != NULL) {
		fname = XCALLOC(fn->size + 1, sizeof(char));
		snprintf(fname, fn->size + 1, "%s", fn->buf);
	}
	long const ffun = un->function;
	char const *ffun_name = la_asn1_value2enum(&asn_DEF_FacilityFunction, ffun);
	LA_ISPRINTF(p.vstr, p.indent, "%s: %s, %s, %s\n", p.label, fdes, fname ? fname : "", ffun_name);
	XFREE(fdes);
	XFREE(fname);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_UnitNameR_as_text) {
	UnitNameR_t const *un = p.sptr;
	char *fdes = NULL;
	FacilityDesignation_t *fd = un->designation;
	if(fd != NULL) {
		fdes = XCALLOC(fd->size + 1, sizeof(char));
		snprintf(fdes, fd->size + 1, "%s", fd->buf);
	}
	char *fname = NULL;
	FacilityName_t *fn = un->name;
	if(fn != NULL) {
		fname = XCALLOC(fn->size + 1, sizeof(char));
		snprintf(fname, fn->size + 1, "%s", fn->buf);
	}
	long const *ffun = un->function;
	char const *ffun_name = NULL;
	if(ffun != NULL) {
		ffun_name = la_asn1_value2enum(&asn_DEF_FacilityFunction, *ffun);
	}
	LA_ISPRINTF(p.vstr, p.indent, "%s: %s, %s, %s\n", p.label, fdes ? fdes : "", fname ? fname : "", ffun_name ? ffun_name : "");
	XFREE(fdes);
	XFREE(fname);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2DateTimeGroup_as_text) {
	ADSv2DateTimeGroup_t const *dtg = p.sptr;
	Date_t const *d = &dtg->date;
	Timesec_t const *t = &dtg->time;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n", p.label,
			d->year, d->month, d->day,
			t->hours, t->minutes, t->seconds);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EstimatedPositionUncertainty_as_text) {
	EstimatedPositionUncertainty_t const *epu = p.sptr;
	if(*epu == 9900) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: complete-loss\n", p.label);
	} else {
		la_format_INTEGER_with_unit_as_text(p, " nm", 0.01, 2);
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2Latitude_as_text) {
	ADSv2Latitude_t const *lat = p.sptr;
	long const ldir = lat->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LatitudeDirection, ldir);
	LA_ISPRINTF(p.vstr, p.indent, "%s:  %02ld %02ld' %04.1f\" %s\n",
			p.label,
			lat->degrees,
			lat->minutes,
			lat->seconds / 10.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2Longitude_as_text) {
	ADSv2Longitude_t const *lon = p.sptr;
	long const ldir = lon->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LongitudeDirection, ldir);
	LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %02ld' %04.1f\" %s\n",
			p.label,
			lon->degrees,
			lon->minutes,
			lon->seconds / 10.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSAircraftPDUs_as_text) {
	ADSAircraftPDUs_t const *apdus = p.sptr;
	// Omit the timestamp for brevity, print the PDU only
	p.td = &asn_DEF_ADSAircraftPDU;
	p.sptr = &apdus->adsAircraftPdu;
	asn1_output_icao_as_text(p);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2Temperature_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " C", 0.25, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2WindSpeedKts_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2WindSpeedKmh_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km/h", 2, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EmergencyUrgencyStatus_as_text) {
	la_format_BIT_STRING_as_text(p, EmergencyUrgencyStatus_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPTimeInterval_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " minutes", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EventTypeNotSupported_as_text) {
	la_format_BIT_STRING_as_text(p, EventTypeNotSupported_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GrossMass_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kg", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSGroundPDUs_as_text) {
	ADSGroundPDUs_t const *apdus = p.sptr;
	// Omit the timestamp for brevity, print the PDU only
	p.td = &asn_DEF_ADSGroundPDU;
	p.sptr = &apdus->adsGroundPdu;
	asn1_output_icao_as_text(p);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPLimitations_as_text) {
	la_format_BIT_STRING_as_text(p, EPPLimitations_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPTolETA_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPTolGCDistance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPUChangeTolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GroundSpeed_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 0.5, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GroundTrack_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " deg", 0.05, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LateralDeviationThreshold_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_MachNumberTolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, "", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Modulus_as_text) {
	long const *val = p.sptr;
	LA_ISPRINTF(p.vstr, p.indent, "%s: every %ld reports\n", p.label, *val);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ReportTypeNotSupported_as_text) {
	la_format_BIT_STRING_as_text(p, ReportTypeNotSupported_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RNPValue_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TransferConstraints_as_text) {
	la_format_BIT_STRING_as_text(p, TransferConstraints_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurbulenceEDRValue_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m^2/s^3", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurbulenceMinutesInThePast_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.5, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurbulenceObservationWindow_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurnRadius_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Timesec_as_text) {
	Timesec_t const *t = p.sptr;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %02ld:%02ld:%02ld\n", p.label, t->hours, t->minutes, t->seconds);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_CPDLCTimesec_as_text) {
	CPDLCTimesec_t const *t = p.sptr;
	TimeSeconds_t *seconds = t->seconds;
	if(seconds != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: %02ld:%02ld:%02ld\n", p.label, t->hours, t->minutes, *seconds);
	} else {
		LA_ISPRINTF(p.vstr, p.indent, "%s: %02ld:%02ld\n", p.label, t->hours, t->minutes);
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalType_as_text) {
	la_format_BIT_STRING_as_text(p, VerticalType_bit_labels);
}

la_asn1_formatter const asn1_icao_formatter_table_text[] = {
	// atn-b2_cpdlc-v1.asn1
	{ .type = &asn_DEF_ATCDownlinkMessage, .format = asn1_format_SEQUENCE_icao_as_text, .label = "CPDLC Downlink Message" },
	{ .type = &asn_DEF_ATCDownlinkMessageData, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Message data" },
	{ .type = &asn_DEF_ATCDownlinkMsgElementId, .format = asn1_format_ATCDownlinkMsgElementId_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCDownlinkMsgElementIdSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCDownlinkRouteClearanceConstrainedData, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCMessageHeader, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Header" },
	{ .type = &asn_DEF_ATCUplinkMessage, .format = asn1_format_SEQUENCE_icao_as_text, .label = "CPDLC Uplink Message" },
	{ .type = &asn_DEF_ATCUplinkMessageData, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Message data" },
	{ .type = &asn_DEF_ATCUplinkMsgElementId, .format = asn1_format_ATCUplinkMsgElementId_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCUplinkMsgElementIdSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCUplinkRouteClearanceConstrainedData, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATISCode, .format = la_asn1_format_any_as_text, .label = "ATIS code" },
	{ .type = &asn_DEF_ATSRouteDesignator, .format = la_asn1_format_any_as_text, .label = "ATS route" },
	{ .type = &asn_DEF_ATWAlongTrackWaypoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint" },
	{ .type = &asn_DEF_ATWAlongTrackWaypointR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint" },
	{ .type = &asn_DEF_ATWAlongTrackWaypointRSequence, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Along-track waypoints" },
	{ .type = &asn_DEF_ATWAlongTrackWaypointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Along-track waypoints" },
	{ .type = &asn_DEF_ATWDistance, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATWDistanceQualifier, .format = la_asn1_format_ENUM_as_text, .label = "ATW Distance Qualifier" },
	{ .type = &asn_DEF_ATWLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATW Level" },
	{ .type = &asn_DEF_ATWLevelQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Qualifier" },
	{ .type = &asn_DEF_ATWLevelS, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATW level" },
	{ .type = &asn_DEF_ATWLevelSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "ATW Levels" },
	{ .type = &asn_DEF_ATWLevelTolerance, .format = la_asn1_format_ENUM_as_text, .label = "ATW Level Tolerance" },
	{ .type = &asn_DEF_AdditionalInformation, .format = la_asn1_format_any_as_text, .label = "Additional information" },
	{ .type = &asn_DEF_AirInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Air-initiated applications" },
	{ .type = &asn_DEF_AirOnlyInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Air-only-initiated applications" },
	{ .type = &asn_DEF_AircraftAddress, .format = la_asn1_format_any_as_text, .label = "Aircraft address" },
	{ .type = &asn_DEF_AircraftIdType, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AircraftIdentification, .format = la_asn1_format_any_as_text, .label = "Aircraft identification" },
// unused
//	{ .type = &asn_DEF_AircraftIdentificationO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AircraftMovement, .format = la_asn1_format_ENUM_as_text, .label = "Aircraft movement" },
	{ .type = &asn_DEF_AircraftType, .format = la_asn1_format_any_as_text, .label = "Aircraft type" },
	{ .type = &asn_DEF_AircraftTypeOTrafficLocationEtpO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AircraftTypeOTrafficLocationVisibility, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AircraftWeight, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AircraftWeightEnglish, .format = asn1_format_AircraftWeightEnglish_as_text, .label = "Aircraft weight" },
	{ .type = &asn_DEF_AircraftWeightMetric, .format = asn1_format_AircraftWeightMetric_as_text, .label = "Aircraft weight" },
	{ .type = &asn_DEF_Airport, .format = la_asn1_format_any_as_text, .label = "Airport" },
	{ .type = &asn_DEF_AirportDeparture, .format = la_asn1_format_any_as_text, .label = "Departure airport" },
	{ .type = &asn_DEF_AirportDestination, .format = la_asn1_format_any_as_text, .label = "Destination airport" },
	{ .type = &asn_DEF_AirportOATISCode, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AirportORunwayO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AirportORunwayORvr, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Altimeter, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AltimeterEnglish, .format = asn1_format_AltimeterEnglish_as_text, .label = "Altimeter" },
	{ .type = &asn_DEF_AltimeterMetric, .format = asn1_format_AltimeterMetric_as_text, .label = "Altimeter" },
	{ .type = &asn_DEF_AltimeterSetting, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AppArrdata, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Approach and arrival data" },
	{ .type = &asn_DEF_ApproachProcedure, .format = asn1_format_CHOICE_icao_as_text, .label = "Approach procedure" },
	{ .type = &asn_DEF_Apron, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ApronName, .format = la_asn1_format_any_as_text, .label = "Apron name" },
	{ .type = &asn_DEF_ApronNameNone, .format = la_asn1_format_label_only_as_text, .label = "Apron name: <none>" },
	{ .type = &asn_DEF_AssignedNameLong, .format = la_asn1_format_any_as_text, .label = "Name" },
	{ .type = &asn_DEF_AssignedNameShort, .format = la_asn1_format_any_as_text, .label = "Name" },
	{ .type = &asn_DEF_AssignedTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Assigned time" },
	{ .type = &asn_DEF_AssignedTimeO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AssignedTimeONone, .format = la_asn1_format_label_only_as_text, .label = "Assigned time: <none>" },
	{ .type = &asn_DEF_AssignedTimeType, .format = la_asn1_format_ENUM_as_text, .label = "Assigned time type" },
	{ .type = &asn_DEF_BlockLevel, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Block level" },
	{ .type = &asn_DEF_CPDLCSpeedIASMach, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_CPDLCSpeedQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Type" },
	{ .type = &asn_DEF_CPDLCTimesec, .format = asn1_format_CPDLCTimesec_as_text, .label = "Time" },
	{ .type = &asn_DEF_CdaNdaO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ClearanceName, .format = la_asn1_format_any_as_text, .label = "Clearance name" },
	{ .type = &asn_DEF_ClearanceType, .format = la_asn1_format_ENUM_as_text, .label = "Clearance type" },
	{ .type = &asn_DEF_ClearanceTypeR, .format = la_asn1_format_ENUM_as_text, .label = "Clearance type" },
	{ .type = &asn_DEF_ClearanceTypeRAssignedTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ClearanceTypeRequest, .format = la_asn1_format_ENUM_as_text, .label = "Clearance type requested" },
	{ .type = &asn_DEF_Code, .format = asn1_format_Code_as_text, .label = "Code" },
	{ .type = &asn_DEF_ControlledTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_CurrentDataAuthority, .format = la_asn1_format_any_as_text, .label = "Current data authority" },
	{ .type = &asn_DEF_Customs, .format = la_asn1_format_any_as_text, .label = "Customs" },
	{ .type = &asn_DEF_DCLRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DMVersionNumber, .format = la_asn1_format_any_as_text, .label = "Version number" },
	{ .type = &asn_DEF_DateTimeDepartureETD, .format = asn1_format_DateTime_as_text, .label = "Departure time" },
	{ .type = &asn_DEF_DateTimeGroup, .format = asn1_format_DateTimeGroup_as_text, .label = "Timestamp" },
	{ .type = &asn_DEF_DegreeIncrement, .format = asn1_format_Deg_as_text, .label = "Degree increment" },
	{ .type = &asn_DEF_Degrees, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
// Used only in LatitudeDegreesMinutesR -> LatitudeR, handled by asn1_format_LatitudeR_as_text
//	{ .type = &asn_DEF_DegreesLat, .format = asn1_format_Deg_as_text, .label = NULL },
// Used only in LongitudeDegreesMinutesR -> LongitudeR, handled by asn1_format_LongitudeR_as_text
//	{ .type = &asn_DEF_DegreesLong, .format = asn1_format_Deg_as_text, .label = NULL },
	{ .type = &asn_DEF_DegreesMagnetic, .format = asn1_format_Deg_as_text, .label = "Degrees (magnetic)" },
	{ .type = &asn_DEF_DegreesTrue, .format = asn1_format_Deg_as_text, .label = "Degrees (true)" },
	{ .type = &asn_DEF_DeicingPosition, .format = asn1_format_CHOICE_icao_as_text, .label = "Deicing position" },
	{ .type = &asn_DEF_DeicingPositionNone, .format = la_asn1_format_label_only_as_text, .label = "Position: <none>" },
	{ .type = &asn_DEF_DeicingPositionStr, .format = la_asn1_format_any_as_text, .label = "Position" },
	{ .type = &asn_DEF_DeicingStopPosition, .format = asn1_format_CHOICE_icao_as_text, .label = "Deicing stop position" },
	{ .type = &asn_DEF_DepartureAdditionalInformation, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Additional information" },
	{ .type = &asn_DEF_DepartureClearance, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DepartureClearanceR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DepartureFrequency, .format = asn1_format_CHOICE_icao_as_text, .label = "Departure frequency" },
	{ .type = &asn_DEF_DepartureHeading, .format = asn1_format_CHOICE_icao_as_text, .label = "Departure heading" },
	{ .type = &asn_DEF_DepartureLevels, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Departure levels" },
// Used only in aliased types: InitialLevel, ExpectLevel
//	{ .type = &asn_DEF_DepartureLevelValue, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DepartureLocation, .format = asn1_format_CHOICE_icao_as_text, .label = "Departure location" },
	{ .type = &asn_DEF_DepartureMinimumInterval, .format = asn1_format_DepartureMinimumInterval_as_text, .label = "Minimum interval of departures" },
	{ .type = &asn_DEF_DeparturePilotPreferences, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Pilot preferences" },
	{ .type = &asn_DEF_DeparturePreferredRoute, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Preferred route" },
	{ .type = &asn_DEF_DepartureProcedure, .format = asn1_format_CHOICE_icao_as_text, .label = "Departure procedure" },
	{ .type = &asn_DEF_DepartureRoute, .format = asn1_format_CHOICE_icao_as_text, .label = "Departure route" },
	{ .type = &asn_DEF_DepartureRouteData, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DepartureRunwayRequested, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Runway requested" },
	{ .type = &asn_DEF_DepartureSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Departure speed" },
	{ .type = &asn_DEF_DepartureUntilConstraint, .format = asn1_format_CHOICE_icao_as_text, .label = "Speed constraint until" },
	{ .type = &asn_DEF_Depdata, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Departure data" },
	{ .type = &asn_DEF_DepdataOAppArrdata, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DepdataOAppArrdataO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DeviationSpecified, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DeviationSpecifiedODirectionSide, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DeviationType, .format = la_asn1_format_ENUM_as_text, .label = "Deviation type" },
	{ .type = &asn_DEF_Direction, .format = la_asn1_format_ENUM_as_text, .label = "Direction" },
	{ .type = &asn_DEF_DirectionCompass, .format = la_asn1_format_ENUM_as_text, .label = "Compass direction" },
	{ .type = &asn_DEF_DirectionDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DirectionNumberOfDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DirectionPreposition, .format = la_asn1_format_ENUM_as_text, .label = "Direction preposition" },
	{ .type = &asn_DEF_DirectionSide, .format = la_asn1_format_ENUM_as_text, .label = "Side" },
	{ .type = &asn_DEF_DirectionSideDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DirectionSideNumberOfDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Distance, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceFeet, .format = asn1_format_DistanceFeet_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceGround, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceKm, .format = asn1_format_DistanceKm_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceMeter, .format = asn1_format_Meters_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceNm, .format = asn1_format_DistanceNm_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceSpecified, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedDirection, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedDirectionTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedKm, .format = asn1_format_DistanceMetric_as_text, .label = "Offset" },
	{ .type = &asn_DEF_DistanceSpecifiedKmR, .format = asn1_format_DistanceSpecifiedKmR_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceSpecifiedNm, .format = asn1_format_DistanceEnglish_as_text, .label = "Offset" },
	{ .type = &asn_DEF_DistanceSpecifiedNmR, .format = asn1_format_DistanceSpecifiedNmR_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceSpecifiedR, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedRDirectionSide, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ErrorInformation, .format = la_asn1_format_ENUM_as_text, .label = "Error information" },
	{ .type = &asn_DEF_ErrorInformationR, .format = la_asn1_format_ENUM_as_text, .label = "Error information" },
// Used only in aliased types: NormalExit, RapidExit
//	{ .type = &asn_DEF_Exit, .format = asn1_format_, .label = NULL },
	{ .type = &asn_DEF_ExpectLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Expect level" },
	{ .type = &asn_DEF_FIRLocationIndicator, .format = la_asn1_format_any_as_text, .label = "FIR" },
	{ .type = &asn_DEF_Facility, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignation, .format = la_asn1_format_any_as_text, .label = "Facility designation" },
	{ .type = &asn_DEF_FacilityDesignationATISCode, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignationAltimeter, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignationOAltimeterTimeO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
// Handled by asn1_format_UnitNameR_as_text
//	{ .type = &asn_DEF_FacilityFunctionR, .format = la_asn1_format_ENUM_as_text, .label = "Facility function" },
	{ .type = &asn_DEF_FacilityName, .format = la_asn1_format_any_as_text, .label = "Facility name" },
	{ .type = &asn_DEF_FacingDirection, .format = la_asn1_format_ENUM_as_text, .label = "Facing direction" },
	{ .type = &asn_DEF_Fix, .format = la_asn1_format_any_as_text, .label = "Fix" },
	{ .type = &asn_DEF_FixName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FixNext, .format = asn1_format_CHOICE_icao_as_text, .label = "Next fix" },
	{ .type = &asn_DEF_FixNextPlusOne, .format = asn1_format_CHOICE_icao_as_text, .label = "Next+1 fix" },
	{ .type = &asn_DEF_FlightInformation, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FlightPhase, .format = la_asn1_format_ENUM_as_text, .label = "Flight phase" },
	{ .type = &asn_DEF_FlightPhaseO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FreeText, .format = la_asn1_format_any_as_text, .label = NULL },
	{ .type = &asn_DEF_Frequency, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FrequencyO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FrequencyR, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Frequencyhf, .format = asn1_format_Frequencyhf_as_text, .label = "HF" },
	{ .type = &asn_DEF_Frequencysatchannel, .format = la_asn1_format_any_as_text, .label = "Satcom channel" },
	{ .type = &asn_DEF_FrequencysatchannelR, .format = la_asn1_format_any_as_text, .label = "Satcom channel" },
	{ .type = &asn_DEF_Frequencyuhf, .format = asn1_format_Frequencyuhf_as_text, .label = "UHF" },
	{ .type = &asn_DEF_Frequencyvhf, .format = asn1_format_Frequencyvhf_as_text, .label = "VHF" },
	{ .type = &asn_DEF_FurtherInstructions, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Gate, .format = la_asn1_format_any_as_text, .label = "Gate" },
	{ .type = &asn_DEF_GroundInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Ground-initiated applications" },
	{ .type = &asn_DEF_GroundLatLong, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_GroundLatitude, .format = asn1_format_GroundLatitude_as_text, .label = "Latitude" },
// Handled by asn1_format_GroundLatitude_as_text
//	{ .type = &asn_DEF_GroundLatitudeDegrees, .format = asn1_format_, .label = NULL },
	{ .type = &asn_DEF_GroundLocation, .format = asn1_format_CHOICE_icao_as_text, .label = "Ground location" },
	{ .type = &asn_DEF_GroundLocationFrom, .format = asn1_format_CHOICE_icao_as_text, .label = "From ground location" },
	{ .type = &asn_DEF_GroundLocationNone, .format = la_asn1_format_label_only_as_text, .label = "Ground location: <none>" },
	{ .type = &asn_DEF_GroundLocationO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_GroundLocationTo, .format = asn1_format_CHOICE_icao_as_text, .label = "To ground location" },
	{ .type = &asn_DEF_GroundLongitude, .format = asn1_format_GroundLongitude_as_text, .label = "Longitude" },
// Handled by asn1_format_GroundLongitude_as_text
//	{ .type = &asn_DEF_GroundLongitudeDegrees, .format = asn1_format_, .label = NULL },
	{ .type = &asn_DEF_GroundOnlyInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Ground-only-initiated applications" },
	{ .type = &asn_DEF_HZWXSpecification, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Hangar, .format = la_asn1_format_any_as_text, .label = "Hangar" },
	{ .type = &asn_DEF_HoldClearance, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_HoldClearanceR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_HoldSpeedHigh, .format = asn1_format_CHOICE_icao_as_text, .label = "Min speed" },
	{ .type = &asn_DEF_HoldSpeedLow, .format = asn1_format_CHOICE_icao_as_text, .label = "Max speed" },
	{ .type = &asn_DEF_Holdatwaypoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_HoldatwaypointR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Holding point" },
	{ .type = &asn_DEF_HoldatwaypointRSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Holding points" },
	{ .type = &asn_DEF_HoldatwaypointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Holding points" },
	{ .type = &asn_DEF_HoldatwaypointSpeedHigh, .format = asn1_format_CHOICE_icao_as_text, .label = "Max speed" },
	{ .type = &asn_DEF_HoldatwaypointSpeedLow, .format = asn1_format_CHOICE_icao_as_text, .label = "Min speed" },
	{ .type = &asn_DEF_HoldingBay, .format = la_asn1_format_any_as_text, .label = "Holding bay" },
	{ .type = &asn_DEF_HoldingPoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Holding point" },
	{ .type = &asn_DEF_HoldingPointCategory, .format = la_asn1_format_ENUM_as_text, .label = "Category" },
	{ .type = &asn_DEF_HoldingPointName, .format = la_asn1_format_any_as_text, .label = "Name" },
	{ .type = &asn_DEF_Humidity, .format = asn1_format_Humidity_as_text, .label = "Humidity" },
	{ .type = &asn_DEF_ITPReferenceAircraft, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ITP reference aircraft" },
	{ .type = &asn_DEF_ITPReferenceAircraftDistance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ITP reference aircraft distance" },
	{ .type = &asn_DEF_ITPReferenceAircraftDistanceList, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ITPReferenceAircraftList, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Icing, .format = la_asn1_format_ENUM_as_text, .label = "Icing" },
	{ .type = &asn_DEF_InitialLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Initial level" },
	{ .type = &asn_DEF_InterceptCourseFrom, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_InterceptCourseFromSelection, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_InterceptCourseFromSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Intercept courses" },
	{ .type = &asn_DEF_Intersection, .format = asn1_format_CHOICE_icao_as_text, .label = "Intersection" },
	{ .type = &asn_DEF_IntersectionRunway, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_IntersectionRunwayDistanceGroundO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LackStatus, .format = asn1_format_CHOICE_icao_as_text, .label = "Logical acknowledgement status" },
	{ .type = &asn_DEF_LatLonReportingPoints, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LateralDeviation, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Latitude, .format = asn1_format_Latitude_as_text, .label = "Latitude" },
	{ .type = &asn_DEF_LatitudeAndLongitudeR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
// Handled by asn1_format_LatitudeR_as_text
//	{ .type = &asn_DEF_LatitudeDegreesMinutesR, .format = asn1_format_, .label = NULL },
	{ .type = &asn_DEF_LatitudeDirection, .format = la_asn1_format_ENUM_as_text, .label = "Direction" },
	{ .type = &asn_DEF_LatitudeLongitude, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LatitudeLongitudeR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LatitudeR, .format = asn1_format_LatitudeR_as_text, .label = "Latitude" },
	{ .type = &asn_DEF_LatitudeReportingPoints, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LatitudeType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LeaveInstruction, .format = asn1_format_CHOICE_icao_as_text, .label = "Leave instruction" },
	{ .type = &asn_DEF_LeaveInstructionClimbing, .format = la_asn1_format_label_only_as_text, .label = "climbing" },
	{ .type = &asn_DEF_LeaveInstructionDescending, .format = la_asn1_format_label_only_as_text, .label = "descending" },
	{ .type = &asn_DEF_LeaveInstructionNone, .format = la_asn1_format_label_only_as_text, .label = "<none>" },
	{ .type = &asn_DEF_LeaveInstructionO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LegDistance, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LegDistanceEnglish, .format = asn1_format_DistanceEnglish_as_text, .label = "Leg distance" },
	{ .type = &asn_DEF_LegDistanceEnglishR, .format = asn1_format_LegDistanceEnglishR_as_text, .label = "Leg distance" },
	{ .type = &asn_DEF_LegDistanceMetric, .format = asn1_format_DistanceMetric_as_text, .label = "Leg distance" },
	{ .type = &asn_DEF_LegDistanceMetricR, .format = asn1_format_LegDistanceMetricR_as_text, .label = "Leg distance" },
	{ .type = &asn_DEF_LegDistanceR, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LegTime, .format = asn1_format_LegTime_as_text, .label = "Leg time" },
	{ .type = &asn_DEF_LegTimeR, .format = asn1_format_LegTimeR_as_text, .label = "Leg time" },
	{ .type = &asn_DEF_LegType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LegTypeR, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Level, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelFeet, .format = asn1_format_LevelFeet_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelFlightLevel, .format = la_asn1_format_any_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelFlightLevelMetric, .format = asn1_format_LevelFlightLevelMetric_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelLevel, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelMeters, .format = asn1_format_Meters_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelProcedureName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelS, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSLateralDeviation, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSLevelS, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSNumberOfMinutes, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSPositionATW, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSPositionR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSProcedureNameR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSpeedSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSsOfFlightR, .format = asn1_format_CHOICE_icao_as_text, .label = "Levels of flight" },
	{ .type = &asn_DEF_LevelTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelsOfFlight, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LocationQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Location qualifier" },
	{ .type = &asn_DEF_LogicalAck, .format = la_asn1_format_ENUM_as_text, .label = "Logical ACK" },
	{ .type = &asn_DEF_Longitude, .format = asn1_format_Longitude_as_text, .label = "Longitude" },
// Handled by asn1_format_LongitudeR_as_text
//	{ .type = &asn_DEF_LongitudeDegreesMinutesR, .format = asn1_format_, .label = NULL },
	{ .type = &asn_DEF_LongitudeDirection, .format = la_asn1_format_ENUM_as_text, .label = "Direction" },
	{ .type = &asn_DEF_LongitudeR, .format = asn1_format_LongitudeR_as_text, .label = "Longitude" },
	{ .type = &asn_DEF_LongitudeReportingPoints, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LongitudeType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_MaximumLevelS, .format = asn1_format_CHOICE_icao_as_text, .label = "Maximum level" },
// Handled by asn1_format_L{at,ong}itudeR_as_text
//	{ .type = &asn_DEF_MinutesLatLonR, .format = asn1_format_, .label = NULL },
	{ .type = &asn_DEF_MsgIdentificationNumber, .format = la_asn1_format_any_as_text, .label = "Msg ID" },
	{ .type = &asn_DEF_MsgReferenceNumber, .format = la_asn1_format_any_as_text, .label = "Msg Ref" },
	{ .type = &asn_DEF_NULL, .format = NULL, .label = NULL },
	{ .type = &asn_DEF_NamedIdentifierName, .format = la_asn1_format_any_as_text, .label = "Identifier name" },
	{ .type = &asn_DEF_NamedIdentifierR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_NamedInstruction, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_NamedPoint, .format = la_asn1_format_any_as_text, .label = "Point" },
	{ .type = &asn_DEF_Navaid, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_NavaidName, .format = la_asn1_format_any_as_text, .label = "Navaid" },
	{ .type = &asn_DEF_NextDataAuthority, .format = la_asn1_format_any_as_text, .label = "Next data authority" },
	{ .type = &asn_DEF_NoDelayExpected, .format = la_asn1_format_label_only_as_text, .label = "No delay expected" },
	{ .type = &asn_DEF_NoFlightPhase, .format = la_asn1_format_label_only_as_text, .label = "Flight phase: not specified" },
	{ .type = &asn_DEF_NoPushbackDirection, .format = la_asn1_format_label_only_as_text, .label = "Pushback direction: not specified" },
	{ .type = &asn_DEF_NormalExit, .format = la_asn1_format_any_as_text, .label = "Normal exit" },
	{ .type = &asn_DEF_NumberOfDegrees, .format = la_asn1_format_any_as_text, .label = "Degrees" },
	{ .type = &asn_DEF_NumberOfMinutes, .format = la_asn1_format_any_as_text, .label = "Number of minutes" },
	{ .type = &asn_DEF_OCLRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_OCLRequestNone, .format = la_asn1_format_label_only_as_text, .label = "<none>" },
	{ .type = &asn_DEF_OCLRequestO, .format = asn1_format_CHOICE_icao_as_text, .label = "Oceanic clearance request" },
	{ .type = &asn_DEF_OceanicEntryPoint, .format = asn1_format_CHOICE_icao_as_text, .label = "Oceanic entry point" },
	{ .type = &asn_DEF_PMCPDLCProviderAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "CPDLC Provider Abort Reason" },
	{ .type = &asn_DEF_PMCPDLCUserAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "CPDLC User Abort Reason" },
	{ .type = &asn_DEF_PersonsOnBoard, .format = la_asn1_format_any_as_text, .label = "Persons on board" },
	{ .type = &asn_DEF_PersonsOnBoardEnhanced, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PersonsOnBoardUnknown, .format = la_asn1_format_label_only_as_text, .label = "Persons on board: <unknown>" },
	{ .type = &asn_DEF_PlaceBearing, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Place and bearing" },
	{ .type = &asn_DEF_PlaceBearingDistance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Place, bearing and distance" },
	{ .type = &asn_DEF_PlaceBearingPlaceBearing, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PlaceBearingR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Place and bearing" },
	{ .type = &asn_DEF_PlaceBearingRDistance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Place, bearing and distance" },
	{ .type = &asn_DEF_PosReportHeading, .format = asn1_format_CHOICE_icao_as_text, .label = "Heading" },
	{ .type = &asn_DEF_PosReportTrackAngle, .format = asn1_format_CHOICE_icao_as_text, .label = "Track angle" },
	{ .type = &asn_DEF_Position, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATW, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Position ATW" },
	{ .type = &asn_DEF_PositionATWAppArrdata, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWDistanceSpecifiedRDirectionSide, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWLevelS, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWLevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWPositionR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWRTATimesec, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWRTATimesecLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWRTATimesecLevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWRTATimesecRTATimesec, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWRTATimesecSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionATWSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionDistanceSpecifiedDirection, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionGAUnitNameRFrequencyO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionGroundair, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionLevelLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionLevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionPosition, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionProcedureName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionR, .format = asn1_format_CHOICE_icao_as_text, .label = "Position" },
	{ .type = &asn_DEF_PositionRAppArrdataO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRDepdataO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRDirectionCompassO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRLevelS, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRNamedInstruction, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRPositionR, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRProcedureNameR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRouteClearanceIndex, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionSpeedSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionTimeLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionTimeTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionUnitNameFrequency, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PreferredLevelS, .format = asn1_format_CHOICE_icao_as_text, .label = "Preferred level" },
	{ .type = &asn_DEF_Procedure, .format = la_asn1_format_any_as_text, .label = "Procedure" },
	{ .type = &asn_DEF_ProcedureAdditionalInformation, .format = la_asn1_format_any_as_text, .label = "Additional information" },
	{ .type = &asn_DEF_ProcedureApproach, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Approach procedure" },
	{ .type = &asn_DEF_ProcedureApproachR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Approach procedure" },
	{ .type = &asn_DEF_ProcedureArrival, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Arrival procedure" },
	{ .type = &asn_DEF_ProcedureArrivalR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Arrival procedure" },
	{ .type = &asn_DEF_ProcedureDeparture, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Departure procedure" },
	{ .type = &asn_DEF_ProcedureName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ProcedureNameR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Procedure name" },
	{ .type = &asn_DEF_ProcedureTransition, .format = la_asn1_format_any_as_text, .label = "Procedure transition" },
	{ .type = &asn_DEF_ProcedureType, .format = la_asn1_format_ENUM_as_text, .label = "Procedure type" },
	{ .type = &asn_DEF_ProtectedAircraftPDUs, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ProtectedGroundPDUs, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PublishedIdentifier, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PushbackDirection, .format = asn1_format_CHOICE_icao_as_text, .label = "Pushback direction" },
	{ .type = &asn_DEF_PushbackPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Pushback position" },
	{ .type = &asn_DEF_PushbackPositionO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PushbackPositionOAssignedTimeO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RTARequiredTimeArrival, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA" },
	{ .type = &asn_DEF_RTARequiredTimeArrivalSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Required arrival times" },
	{ .type = &asn_DEF_RTATime, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA time" },
	{ .type = &asn_DEF_RTATimesec, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA Time" },
	{ .type = &asn_DEF_RTATimesecRTATimesec, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RTATolerance, .format = asn1_format_RTATolerance_as_text, .label = "RTA Tolerance" },
	{ .type = &asn_DEF_RTAsecRequiredTimeArrival, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA" },
	{ .type = &asn_DEF_RTAsecRequiredTimeArrivalSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Required arrival times" },
	{ .type = &asn_DEF_RVR, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RVREnd, .format = asn1_format_CHOICE_icao_as_text, .label = "RVR at end" },
	{ .type = &asn_DEF_RVRFeet, .format = asn1_format_Feet_as_text, .label = "RVR" },
	{ .type = &asn_DEF_RVRMeters, .format = asn1_format_Meters_as_text, .label = "RVR" },
	{ .type = &asn_DEF_RVRMiddle, .format = asn1_format_CHOICE_icao_as_text, .label = "RVR at middle" },
	{ .type = &asn_DEF_RVRSection, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RVRTouchdown, .format = asn1_format_CHOICE_icao_as_text, .label = "RVR at touchdown" },
	{ .type = &asn_DEF_RVRValue, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Ramp, .format = asn1_format_CHOICE_icao_as_text, .label = "Ramp" },
	{ .type = &asn_DEF_RapidExit, .format = la_asn1_format_any_as_text, .label = "Rapid exit" },
	{ .type = &asn_DEF_RelayInstruction, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RelayResponse, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RemainingFuel, .format = asn1_format_Time_as_text, .label = "Remaining fuel" },
	{ .type = &asn_DEF_RemainingFuelPersonsOnBoard, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RemainingFuelPersonsOnBoardE, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ReportedWaypointLevel, .format = asn1_format_CHOICE_icao_as_text, .label = "Reported waypoint level" },
	{ .type = &asn_DEF_ReportedWaypointPosition, .format = asn1_format_CHOICE_icao_as_text, .label = "Reported waypoint position" },
	{ .type = &asn_DEF_ReportedWaypointTime, .format = asn1_format_Time_as_text, .label = "Reported waypoint time" },
	{ .type = &asn_DEF_ReportingPoints, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RevisionNumber, .format = la_asn1_format_any_as_text, .label = "Revision number" },
	{ .type = &asn_DEF_RevisionReason, .format = asn1_format_RevisionReason_as_text, .label = "Revision reason" },
	{ .type = &asn_DEF_RevisionReasonO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RevisionReasonSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteAndLevels, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteAsFiled, .format = la_asn1_format_label_only_as_text, .label = "Route: as filed" },
	{ .type = &asn_DEF_RouteClearance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Route clearance" },
	{ .type = &asn_DEF_RouteClearanceIndex, .format = la_asn1_format_any_as_text, .label = "Route clearance index" },
	{ .type = &asn_DEF_RouteClearanceR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Route clearance" },
	{ .type = &asn_DEF_RouteClearanceSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteInformation, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteInformationAdditional, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Additional route information" },
	{ .type = &asn_DEF_RouteInformationAdditionalR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Additional route information" },
	{ .type = &asn_DEF_RouteInformationR, .format = asn1_format_CHOICE_icao_as_text, .label = "Route information" },
	{ .type = &asn_DEF_RouteInformationSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Route" },
	{ .type = &asn_DEF_RouteOfFlight, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Route of flight" },
	{ .type = &asn_DEF_Runway, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RunwayArrival, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Arrival runway" },
	{ .type = &asn_DEF_RunwayConfiguration, .format = la_asn1_format_ENUM_as_text, .label = "Runway configuration" },
	{ .type = &asn_DEF_RunwayDeparture, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Departure runway" },
	{ .type = &asn_DEF_RunwayDirection, .format = la_asn1_format_any_as_text, .label = "Runway direction" },
	{ .type = &asn_DEF_RunwayO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RunwayOIntersectionDistanceGroundO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RunwayRVR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RunwaySpecifiedRVR, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RunwayUse, .format = asn1_format_RunwayUse_as_text, .label = "Runway usage" },
	{ .type = &asn_DEF_SARSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SARSpecification, .format = asn1_format_SEQUENCE_icao_as_text, .label = "SAR" },
	{ .type = &asn_DEF_SIGMETIdentifier, .format = asn1_format_SEQUENCE_icao_as_text, .label = "SIGMET" },
	{ .type = &asn_DEF_SIGMETIdentifierSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL},
	{ .type = &asn_DEF_SIGMETSequenceNumber, .format = la_asn1_format_any_as_text, .label = "Sequence number" },
	{ .type = &asn_DEF_SpecialInstructions, .format = la_asn1_format_any_as_text, .label = "Special instructions" },
	{ .type = &asn_DEF_SpecifiedReasonDownlink, .format = la_asn1_format_ENUM_as_text, .label = "Reason" },
	{ .type = &asn_DEF_SpecifiedReasonUplink, .format = la_asn1_format_ENUM_as_text, .label = "Reason" },
	{ .type = &asn_DEF_Speed, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedDelta, .format = asn1_format_SpeedDelta_as_text, .label = "Speed delta" },
	{ .type = &asn_DEF_SpeedGround, .format = asn1_format_SpeedEnglish_as_text, .label = "Ground speed" },
	{ .type = &asn_DEF_SpeedGroundMetric, .format = asn1_format_SpeedMetric_as_text, .label = "Ground speed" },
	{ .type = &asn_DEF_SpeedIAS, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedIndicated, .format = asn1_format_SpeedIndicated_as_text, .label = "Indicated airspeed" },
	{ .type = &asn_DEF_SpeedIndicatedMetric, .format = asn1_format_SpeedMetric_as_text, .label = "Indicated airspeed" },
	{ .type = &asn_DEF_SpeedLimit, .format = la_asn1_format_ENUM_as_text, .label = "Speed limit" },
	{ .type = &asn_DEF_SpeedMach, .format = asn1_format_SpeedMach_as_text, .label = "Mach" },
	{ .type = &asn_DEF_SpeedSchedule, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Speed schedule" },
	{ .type = &asn_DEF_SpeedSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTrue, .format = asn1_format_SpeedEnglish_as_text, .label = "True airspeed" },
	{ .type = &asn_DEF_SpeedTrueMetric, .format = asn1_format_SpeedMetric_as_text, .label = "True airspeed" },
	{ .type = &asn_DEF_SpeedType, .format = la_asn1_format_ENUM_as_text, .label = "Speed type" },
	{ .type = &asn_DEF_SpeedTypeR, .format = la_asn1_format_ENUM_as_text, .label = "Speed type" },
	{ .type = &asn_DEF_SpeedTypeRSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTypeSpeedTypeSpeedType, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTypeSpeedTypeSpeedTypeSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTypes, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Speed types" },
	{ .type = &asn_DEF_SpeedTypesSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Stand, .format = la_asn1_format_any_as_text, .label = "Stand" },
	{ .type = &asn_DEF_TailToDirection, .format = la_asn1_format_ENUM_as_text, .label = "Tail direction" },
	{ .type = &asn_DEF_TargetStartupApprovalTime, .format = asn1_format_Time_as_text, .label = "Target startup approval time" },
	{ .type = &asn_DEF_TaxiAfterAircraft, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Taxi after aircraft" },
	{ .type = &asn_DEF_TaxiBeforeAircraft, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Taxi before aircraft" },
	{ .type = &asn_DEF_TaxiDuration, .format = asn1_format_CHOICE_icao_as_text, .label = "Taxi duration" },
	{ .type = &asn_DEF_TaxiDurationO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TaxiElement, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TaxiElementStandard, .format = la_asn1_format_any_as_text, .label = "Taxi element" },
	{ .type = &asn_DEF_TaxiRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Taxi request" },
	{ .type = &asn_DEF_TaxiRequestO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
//	Handled by TaxiBeforeAircraft/TaxiAfterAircraft
//	{ .type = &asn_DEF_TaxiResumeCondition, .format = asn1_format_, .label = NULL },
	{ .type = &asn_DEF_TaxiResumeConditionO, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TaxiRoute, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Taxi route" },
	{ .type = &asn_DEF_TaxiRouteElement, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Taxi route element" },
	{ .type = &asn_DEF_Taxilane, .format = la_asn1_format_any_as_text, .label = "Taxi lane" },
	{ .type = &asn_DEF_Taxiway, .format = la_asn1_format_any_as_text, .label = "Taxiway" },
	{ .type = &asn_DEF_Temperature, .format = asn1_format_Temperature_as_text, .label = "Temperature" },
	{ .type = &asn_DEF_Terminal, .format = asn1_format_CHOICE_icao_as_text, .label = "Terminal" },
	{ .type = &asn_DEF_ThenAsFiled, .format = la_asn1_format_label_only_as_text, .label = "Then: as filed" },
	{ .type = &asn_DEF_Time, .format = asn1_format_Time_as_text, .label = "Time" },
	{ .type = &asn_DEF_TimeDepAllocated, .format = asn1_format_Time_as_text, .label = "Allocated departure time" },
	{ .type = &asn_DEF_TimeDepClearanceExpected, .format = asn1_format_Time_as_text, .label = "Expected departure clearance time" },
	{ .type = &asn_DEF_TimeDeparture, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeDistanceSpecifiedDirection, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeDistanceSpecifiedRDirectionSide, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeDistanceToFromPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeETAatDest, .format = asn1_format_Time_as_text, .label = "ETA at destination" },
	{ .type = &asn_DEF_TimeETAatFixNext, .format = asn1_format_Time_as_text, .label = "ETA at next fix" },
	{ .type = &asn_DEF_TimeLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePositionLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePositionLevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePositionR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePreposition, .format = la_asn1_format_ENUM_as_text, .label = "Time preposition" },
	{ .type = &asn_DEF_TimeQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Time qualifier" },
	{ .type = &asn_DEF_TimeSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeSpeedSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeTime, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeToFromPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeTolerance, .format = la_asn1_format_ENUM_as_text, .label = "Time tolerance" },
	{ .type = &asn_DEF_TimeUnitNameFrequency, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeUnitNameRFrequencyO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
// Handled by asn1_format_DateTimeGroup_as_text
//	{ .type = &asn_DEF_Timehhmmss, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimerValue, .format = asn1_format_TimerValue_as_text, .label = "Timer value" },
	{ .type = &asn_DEF_ToFrom, .format = la_asn1_format_ENUM_as_text, .label = "To/From" },
	{ .type = &asn_DEF_ToFromPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TrafficDescription, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TrafficLocation, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Traffic location" },
	{ .type = &asn_DEF_TrafficLocationQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Location qualifier" },
	{ .type = &asn_DEF_TrafficType, .format = la_asn1_format_ENUM_as_text, .label = "Traffic type" },
	{ .type = &asn_DEF_TrafficVisibility, .format = la_asn1_format_ENUM_as_text, .label = "Traffic visibility" },
	{ .type = &asn_DEF_TransferConstraints, .format = asn1_format_TransferConstraints_as_text, .label = "Transfer constraints" },
	{ .type = &asn_DEF_TransferConstraintsUnitNameR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Turbulence, .format = la_asn1_format_ENUM_as_text, .label = "Turbulence" },
	{ .type = &asn_DEF_UnitName, .format = asn1_format_UnitName_as_text, .label = "Unit name" },
	{ .type = &asn_DEF_UnitNameFrequency, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_UnitNameR, .format = asn1_format_UnitNameR_as_text, .label = "Unit name" },
	{ .type = &asn_DEF_UnitNameRFrequencyO, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_UnitNameRFrequencyR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_UnitNameRUnitNameR, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_UseOfLackProhibited, .format = la_asn1_format_label_only_as_text, .label = "Use of Logical ACK prohibited" },
	{ .type = &asn_DEF_VerticalChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_VerticalDirection, .format = la_asn1_format_ENUM_as_text, .label = "Vertical direction" },
	{ .type = &asn_DEF_VerticalDistance, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_VerticalDistanceFeet, .format = asn1_format_VerticalDistanceFeet_as_text, .label = "Vertical distance" },
	{ .type = &asn_DEF_VerticalDistanceMetric, .format = asn1_format_VerticalDistanceMetric_as_text, .label = "Vertical distance" },
	{ .type = &asn_DEF_VerticalRate, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_VerticalRateEnglish, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Vertical rate" },
	{ .type = &asn_DEF_VerticalRateMetric, .format = asn1_format_VerticalRateMetric_as_text, .label = "Vertical rate" },
	{ .type = &asn_DEF_WaypointLevelConstraint, .format = asn1_format_CHOICE_icao_as_text, .label = "Level constraint" },
	{ .type = &asn_DEF_WaypointSpeedConstraint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Speed constraint" },
	{ .type = &asn_DEF_WaypointSpeedLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint, speed and level" },
	{ .type = &asn_DEF_WaypointSpeedLevelR, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint, speed and level" },
	{ .type = &asn_DEF_WaypointSpeedLevelRSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Waypoints, speeds and levels" },
	{ .type = &asn_DEF_WaypointSpeedLevelSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Waypoints, speeds and levels" },
	{ .type = &asn_DEF_WindDirection, .format = asn1_format_Deg_as_text, .label = "Wind direction" },
	{ .type = &asn_DEF_WindSpeed, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_WindSpeedEnglish, .format = asn1_format_SpeedEnglish_as_text, .label = "Wind speed" },
	{ .type = &asn_DEF_WindSpeedMetric, .format = asn1_format_SpeedMetric_as_text, .label = "Wind speed" },
	{ .type = &asn_DEF_Winds, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	// atn-b1_cm.asn1
	{ .type = &asn_DEF_APAddress, .format = asn1_format_CHOICE_icao_as_text, .label = "AP Address" },
	{ .type = &asn_DEF_AEQualifier, .format = la_asn1_format_any_as_text, .label = "Application Entity Qualifier" },
	{ .type = &asn_DEF_AEQualifierVersion, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AEQualifierVersionAddress, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ARS, .format = la_asn1_format_any_as_text, .label = "ARS" },
	{ .type = &asn_DEF_AircraftFlightIdentification, .format = la_asn1_format_any_as_text, .label = "Flight ID" },
	{ .type = &asn_DEF_CMAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "ATN Context Management - Abort Reason" },
	{ .type = &asn_DEF_CMAircraftMessage, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_CMGroundMessage, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_CMContactRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Contact Request" },
	{ .type = &asn_DEF_CMContactResponse, .format = la_asn1_format_ENUM_as_text, .label = "ATN Context Management - Contact Response" },
	{ .type = &asn_DEF_CMForwardRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Forward Request" },
	{ .type = &asn_DEF_CMForwardResponse, .format = la_asn1_format_ENUM_as_text, .label = "ATN Context Management - Forward Response" },
	{ .type = &asn_DEF_CMLogonRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Logon Request" },
	{ .type = &asn_DEF_CMLogonResponse, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Logon Response" },
	{ .type = &asn_DEF_CMUpdate, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Update" },
	// Handled by asn_DEF_ShortTsap formatter
	//{ .type = &asn_DEF_LocSysNselTsel, .format = la_asn1_format_any_as_text, .label = "LOC/SYS/NSEL/TSEL" },
	{ .type = &asn_DEF_LongTsap, .format = asn1_format_LongTsap_as_text, .label = "Long TSAP" },
	{ .type = &asn_DEF_OCTET_STRING, .format = la_asn1_format_any_as_text, .label = NULL },
	{ .type = &asn_DEF_RDP, .format = la_asn1_format_any_as_text, .label = "RDP" },
	{ .type = &asn_DEF_ShortTsap, .format = asn1_format_ShortTsap_as_text, .label = "Short TSAP" },
	{ .type = &asn_DEF_VersionNumber, .format = la_asn1_format_any_as_text, .label = "Version number" },
	// atn-b1_pmadsc.asn1
	{ .type = &asn_DEF_ADSAircraftPDU, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSAircraftPDUs, .format = asn1_format_ADSAircraftPDUs_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSGroundPDU, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSGroundPDUs, .format = asn1_format_ADSGroundPDUs_as_text, .label = NULL },
	{ .type = &asn_DEF_CancelAllContracts, .format = la_asn1_format_label_only_as_text, .label = "ADS-C v2 Cancel All Contracts" },
	{ .type = &asn_DEF_CancelContract, .format = asn1_format_CHOICE_icao_as_text, .label = "ADS-C v2 Cancel Contract" },
	{ .type = &asn_DEF_CancelPositiveAcknowledgement, .format = la_asn1_format_ENUM_as_text, .label = "ADS-C v2 Cancel ACK" },
	{ .type = &asn_DEF_CancelRejectReason, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Cancel NAK" },
	{ .type = &asn_DEF_ProviderAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "ADS-C v2 Provider Abort" },
	{ .type = &asn_DEF_RejectReason, .format = la_asn1_format_ENUM_as_text, .label = "Reject reason" },
	{ .type = &asn_DEF_RequestType, .format = la_asn1_format_ENUM_as_text, .label = "Request type" },
	{ .type = &asn_DEF_UserAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "ADS-C v2 User Abort" },
	// atn-b2_adsc_v2.asn1
	{ .type = &asn_DEF_AAISAvailability, .format = la_asn1_format_any_as_text, .label = "AAIS available" },
	{ .type = &asn_DEF_ADSAccept, .format = asn1_format_CHOICE_icao_as_text, .label = "ADS-C v2 Contract Request Accept" },
	{ .type = &asn_DEF_ADSDataReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report data" },
	{ .type = &asn_DEF_ADSEmergencyUrgencyStatus, .format = asn1_format_EmergencyUrgencyStatus_as_text, .label = "Emergency/urgency status" },
	{ .type = &asn_DEF_ADSNonCompliance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Non-Compliance Notification" },
	{ .type = &asn_DEF_ADSPositiveAcknowledgement, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 ACK" },
	{ .type = &asn_DEF_ADSReject, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Reject" },
	{ .type = &asn_DEF_ADSReport, .format = asn1_format_CHOICE_icao_as_text, .label = "ADS-C v2 Report" },
	{ .type = &asn_DEF_ADSRequestContract, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSv2DateTimeGroup, .format = asn1_format_ADSv2DateTimeGroup_as_text, .label = "Timestamp" },
	{ .type = &asn_DEF_ADSv2Latitude, .format = asn1_format_ADSv2Latitude_as_text, .label = "Lat" },
	{ .type = &asn_DEF_ADSv2LatitudeLongitude, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSv2Level, .format = asn1_format_LevelFeet_as_text, .label = "Alt" },
	{ .type = &asn_DEF_ADSv2Longitude, .format = asn1_format_ADSv2Longitude_as_text, .label = "Lon" },
	{ .type = &asn_DEF_ADSv2RequestType, .format = la_asn1_format_ENUM_as_text, .label = "Request type" },
	{ .type = &asn_DEF_ADSv2Temperature, .format = asn1_format_ADSv2Temperature_as_text, .label = "Temperature" },
	{ .type = &asn_DEF_ADSv2Turbulence, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Turbulence" },
	{ .type = &asn_DEF_ADSv2VerticalRate, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Vertical rate" },
	{ .type = &asn_DEF_ADSv2WindSpeed, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSv2WindSpeedKmh, .format = asn1_format_ADSv2WindSpeedKmh_as_text, .label = "Wind speed" },
	{ .type = &asn_DEF_ADSv2WindSpeedKts, .format = asn1_format_ADSv2WindSpeedKts_as_text, .label = "Wind speed" },
	{ .type = &asn_DEF_ATSUListHiPrio, .format = la_asn1_format_any_as_text, .label = "High priority" },
	{ .type = &asn_DEF_ATSUListMedPrio, .format = la_asn1_format_any_as_text, .label = "Medium priority" },
	{ .type = &asn_DEF_ATSUListLoPrio, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Low priority" },
	{ .type = &asn_DEF_AirVector, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Air vector" },
	{ .type = &asn_DEF_AirVectorModulus, .format = asn1_format_Modulus_as_text, .label = "Report air vector" },
	{ .type = &asn_DEF_Airspeed, .format = asn1_format_CHOICE_icao_as_text, .label = "Airspeed" },
	{ .type = &asn_DEF_AirspeedChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report airspeed changes" },
	{ .type = &asn_DEF_AirspeedChangeTolerance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Airspeed" },
	{ .type = &asn_DEF_AirspeedRangeChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report airspeed range changes" },
	{ .type = &asn_DEF_ClimbSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Climb speed" },
	{ .type = &asn_DEF_ConnectedATSUList, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Connected ATSU list" },
	{ .type = &asn_DEF_ContractDetailsNotSupporting, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ContractNumber, .format = la_asn1_format_any_as_text, .label = "Contract number" },
	{ .type = &asn_DEF_DCRAirVector, .format = la_asn1_format_label_only_as_text, .label = "Report air vector" },
	{ .type = &asn_DEF_DCRGroundVector, .format = la_asn1_format_label_only_as_text, .label = "Report ground vector" },
	{ .type = &asn_DEF_DCRPlannedFinalApproachSpeed, .format = la_asn1_format_label_only_as_text, .label = "Report planned final approach speed" },
	{ .type = &asn_DEF_DCRProjectedProfile, .format = la_asn1_format_label_only_as_text, .label = "Report projected profile" },
	{ .type = &asn_DEF_DCRRNPProfile, .format = la_asn1_format_label_only_as_text, .label = "Report RNP profile" },
	{ .type = &asn_DEF_DCRSpeedScheduleProfile, .format = la_asn1_format_label_only_as_text, .label = "Report speed schedule profile" },
	{ .type = &asn_DEF_DemandContractRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Demand Contract Request" },
	{ .type = &asn_DEF_DemandReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "On-demand Report" },
	{ .type = &asn_DEF_ECRRNPNotMet, .format = la_asn1_format_label_only_as_text, .label = "Report when RNP not met" },
	{ .type = &asn_DEF_ECRRTAStatusChange, .format = la_asn1_format_label_only_as_text, .label = "Report RTA status changes" },
	{ .type = &asn_DEF_ECRWaypointChange, .format = la_asn1_format_label_only_as_text, .label = "Report waypoint changes" },
	{ .type = &asn_DEF_DescentSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Descent speed" },
	{ .type = &asn_DEF_EPPEventChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report EPP changes" },
	{ .type = &asn_DEF_EPPFlightPlanChangeRequest, .format = la_asn1_format_label_only_as_text, .label = "Report EPP flight plan changes" },
	{ .type = &asn_DEF_EPPLevel, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_EPPLimitations, .format = asn1_format_EPPLimitations_as_text, .label = "EPP limitations" },
	{ .type = &asn_DEF_EPPNextWptInHorizonRequest, .format = la_asn1_format_label_only_as_text, .label = "Report next waypoint in horizon" },
	{ .type = &asn_DEF_EPPTolGCDistance, .format = asn1_format_EPPTolGCDistance_as_text, .label = "Great circle distance" },
	{ .type = &asn_DEF_EPPTolLevel, .format = asn1_format_LevelFeet_as_text, .label = "Altitude" },
	{ .type = &asn_DEF_EPPTolETA, .format = asn1_format_EPPTolETA_as_text, .label = "ETA" },
	{ .type = &asn_DEF_EPPToleranceChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report EPP tolerance changes" },
	{ .type = &asn_DEF_EPPTolerancesValues, .format = asn1_format_SEQUENCE_icao_as_text, .label = "EPP tolerances" },
	{ .type = &asn_DEF_EPPNumWaypoints, .format = la_asn1_format_any_as_text, .label = "Number of waypoints" },
	{ .type = &asn_DEF_EPPTimeInterval, .format = asn1_format_EPPTimeInterval_as_text, .label = "Time interval" },
	{ .type = &asn_DEF_EPPRequest, .format = asn1_format_CHOICE_icao_as_text, .label = "Report extended projected profile" },
	{ .type = &asn_DEF_EPPWindow, .format = asn1_format_CHOICE_icao_as_text, .label = "EPP window" },
	{ .type = &asn_DEF_EPUChangeTolerance, .format = asn1_format_EPUChangeTolerance_as_text, .label = "Report FoM changes exceeding" },
	{ .type = &asn_DEF_ETA, .format = asn1_format_Timesec_as_text, .label = "ETA" },
	{ .type = &asn_DEF_EstimatedPositionUncertainty, .format = asn1_format_EstimatedPositionUncertainty_as_text, .label = "Estimated position uncertainty" },
	{ .type = &asn_DEF_EventContractRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Event Contract Request" },
	{ .type = &asn_DEF_EventReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Event Report" },
	{ .type = &asn_DEF_EventTypeNotSupported, .format = asn1_format_EventTypeNotSupported_as_text, .label = "Unsupported events" },
	{ .type = &asn_DEF_EventTypeReported, .format = la_asn1_format_ENUM_as_text, .label = "Reported event" },
	{ .type = &asn_DEF_ExtendedProjectedProfile, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Extended projected profile" },
	{ .type = &asn_DEF_ExtendedProjectedProfileModulus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report extended projected profile" },
	{ .type = &asn_DEF_ExtendedWayPointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Waypoint sequence" },
	{ .type = &asn_DEF_ExtendedWayPointSequenceElement, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint data" },
	{ .type = &asn_DEF_FigureOfMerit, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Figure of merit" },
	{ .type = &asn_DEF_FinalApproachSpeedChange, .format = asn1_format_SpeedIndicated_as_text, .label = "Report planned final approach speed changes" },
	{ .type = &asn_DEF_FinalCruiseSpeedAtToD, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Final cruise speed at top of descent" },
	{ .type = &asn_DEF_GrossMass, .format = asn1_format_GrossMass_as_text, .label = "Gross mass" },
	{ .type = &asn_DEF_GroundSpeed, .format = asn1_format_GroundSpeed_as_text, .label = "Ground speed" },
	{ .type = &asn_DEF_GroundSpeedChange, .format = asn1_format_SpeedIndicated_as_text, .label = "Report ground speed changes" },
	{ .type = &asn_DEF_GroundTrack, .format = asn1_format_GroundTrack_as_text, .label = "Ground track" },
	{ .type = &asn_DEF_GroundVector, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Ground vector" },
	{ .type = &asn_DEF_GroundVectorModulus, .format = asn1_format_Modulus_as_text, .label = "Report ground vector" },
	{ .type = &asn_DEF_Heading, .format = asn1_format_GroundTrack_as_text, .label = "Heading" },
	{ .type = &asn_DEF_Ias, .format = asn1_format_SpeedIndicated_as_text, .label = "IAS" },
	{ .type = &asn_DEF_IasTolerance, .format = asn1_format_SpeedIndicated_as_text, .label = "IAS" },
	{ .type = &asn_DEF_IasChange, .format = asn1_format_SpeedIndicated_as_text, .label = "IAS change" },
	{ .type = &asn_DEF_InitialCruiseSpeedAtToC, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Initial cruise speed at top of climb" },
	{ .type = &asn_DEF_LateralFlightManaged, .format = la_asn1_format_any_as_text, .label = "Lateral flight managed" },
	{ .type = &asn_DEF_LateralDeviationChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report lateral deviation changes" },
	{ .type = &asn_DEF_LateralDeviationOffsetTag, .format = la_asn1_format_label_only_as_text, .label = "Offset tag" /* ? */ },
	{ .type = &asn_DEF_LateralDeviationThresholdLeft, .format = asn1_format_LateralDeviationThreshold_as_text, .label = "Left threshold" },
	{ .type = &asn_DEF_LateralDeviationThresholdRight, .format = asn1_format_LateralDeviationThreshold_as_text, .label = "Right threshold" },
	{ .type = &asn_DEF_LateralType, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Lateral type" },
	{ .type = &asn_DEF_LateralTypeFlyby, .format = asn1_format_CHOICE_icao_as_text, .label = "Fly-by" },
	{ .type = &asn_DEF_LateralTypeFixedRadiusTransition, .format = asn1_format_CHOICE_icao_as_text, .label = "Fixed radius transition" },
	{ .type = &asn_DEF_LateralTypeOffsetStart, .format = la_asn1_format_label_only_as_text, .label = "Offset start" },
	{ .type = &asn_DEF_LateralTypeOffsetReached, .format = la_asn1_format_label_only_as_text, .label = "Offset reached" },
	{ .type = &asn_DEF_LateralTypeReturnToParentPathInitiation, .format = la_asn1_format_label_only_as_text, .label = "Return to parent path initiation" },
	{ .type = &asn_DEF_LateralTypeOffsetEnd, .format = la_asn1_format_label_only_as_text, .label = "Offset end" },
	{ .type = &asn_DEF_LateralTypeOffset, .format = la_asn1_format_label_only_as_text, .label = "Offset" },
	{ .type = &asn_DEF_LateralTypeOverfly, .format = la_asn1_format_label_only_as_text, .label = "Overfly" },
	{ .type = &asn_DEF_LateralTypeFlightPlanWayPoint, .format = la_asn1_format_label_only_as_text, .label = "Flight plan waypoint" },
	{ .type = &asn_DEF_LateralTypeFollowedByDisco, .format = la_asn1_format_label_only_as_text, .label = "Followed by discontinuity" },
	{ .type = &asn_DEF_LevelChange, .format = asn1_format_LevelFeet_as_text, .label = "Report level changes exceeding" },
	{ .type = &asn_DEF_LevelConstraint, .format = asn1_format_CHOICE_icao_as_text, .label = "Level constraint" },
	{ .type = &asn_DEF_LevelConstraintQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Level constraint type" },
	{ .type = &asn_DEF_LevelRangeDeviation, .format = asn1_format_CHOICE_icao_as_text, .label = "Report level range deviation" },
	{ .type = &asn_DEF_LevelRangeDeviationBoth, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelRangeDeviationCeiling, .format = asn1_format_LevelFeet_as_text, .label = "Upper limit" },
	{ .type = &asn_DEF_LevelRangeDeviationFloor, .format = asn1_format_LevelFeet_as_text, .label = "Lower limit" },
	{ .type = &asn_DEF_MachAndIas, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_MachNumberChange, .format = asn1_format_SpeedMach_as_text, .label = "Mach number change" },
	{ .type = &asn_DEF_MachNumberTolerance, .format = asn1_format_MachNumberTolerance_as_text, .label = "Mach number" },
	{ .type = &asn_DEF_MetInfo, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Meteo data" },
	{ .type = &asn_DEF_MinMaxIAS, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Min/max IAS" },
	{ .type = &asn_DEF_MinMaxMach, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Min/max Mach" },
	{ .type = &asn_DEF_MinMaxSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Min/max speed" },
	{ .type = &asn_DEF_MetInfoModulus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report meteo info" },
	{ .type = &asn_DEF_MetInfoRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report meteo info" },
	{ .type = &asn_DEF_Modulus, .format = asn1_format_Modulus_as_text, .label = "Reporting frequency" },
	{ .type = &asn_DEF_MSLAltitude, .format = asn1_format_LevelFeet_as_text, .label = "Alt (MSL)" },
	{ .type = &asn_DEF_MultipleNavigationalUnitsOperating, .format = la_asn1_format_any_as_text, .label = "Multiple NAV units operating" },
	{ .type = &asn_DEF_NominalSpeed, .format = asn1_format_CHOICE_icao_as_text, .label = "Nominal speed" },
	{ .type = &asn_DEF_PeriodicContractRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Periodic Contract Request" },
	{ .type = &asn_DEF_PeriodicReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Periodic Report" },
	{ .type = &asn_DEF_PlannedFinalAppSpeedModulus, .format = asn1_format_Modulus_as_text, .label = "Report planned final approach speed" },
	{ .type = &asn_DEF_PredictedGrossMassAtToD, .format = asn1_format_GrossMass_as_text, .label = "Predicted gross mass at top of descent" },
	{ .type = &asn_DEF_ProjectedProfile, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Projected profile" },
	{ .type = &asn_DEF_ProjectedProfileModulus, .format = asn1_format_Modulus_as_text, .label = "Report projected profile" },
	{ .type = &asn_DEF_QNEAltitude, .format = asn1_format_LevelFeet_as_text, .label = "Alt (QNE)" },
	{ .type = &asn_DEF_QNHAltitude, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Alt (QNH)" },
	{ .type = &asn_DEF_RejectDetails, .format = asn1_format_RejectDetails_as_text, .label = "Reject reason" },
	{ .type = &asn_DEF_RNPProfile, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "RNP profile" },
	{ .type = &asn_DEF_RNPProfileModulus, .format = asn1_format_Modulus_as_text, .label = "Report RNP profile" },
	{ .type = &asn_DEF_RNPSegment, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RNP segment" },
	{ .type = &asn_DEF_RNPSegmentEndPoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "End" },
	{ .type = &asn_DEF_RNPSegmentStartPoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Start" },
	{ .type = &asn_DEF_RNPValue, .format = asn1_format_RNPValue_as_text, .label = "RNP value" },
	{ .type = &asn_DEF_RTA, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA" },
	{ .type = &asn_DEF_RTASecTolerance, .format = asn1_format_RTASecTolerance_as_text, .label = "Tolerance" },
	{ .type = &asn_DEF_RTAStatus, .format = la_asn1_format_ENUM_as_text, .label = "Status" },
	{ .type = &asn_DEF_RTAStatusData, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA status data" },
	{ .type = &asn_DEF_RTAType, .format = la_asn1_format_ENUM_as_text, .label = "Type" },
	{ .type = &asn_DEF_ReportTypeAndPeriodNotSupported, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ReportTypeNotSupported, .format = asn1_format_ReportTypeNotSupported_as_text, .label = "Unsupported reports" },
	{ .type = &asn_DEF_ReportingRate, .format = asn1_format_ReportingRate_as_text, .label = "Reporting rate" },
	{ .type = &asn_DEF_SingleLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Single level" },
	{ .type = &asn_DEF_SingleLevelSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Single level sequence" },
	{ .type = &asn_DEF_SpeedConstraint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Speed constraint" },
	{ .type = &asn_DEF_SpeedIASMach, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedManaged, .format = la_asn1_format_any_as_text, .label = "Speed managed" },
	{ .type = &asn_DEF_SpeedQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Type" },
	{ .type = &asn_DEF_SpeedScheduleBlock, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Block speed schedule" },
	{ .type = &asn_DEF_SpeedScheduleProfile, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Speed schedule profile" },
	{ .type = &asn_DEF_SpeedScheduleProfileModulus, .format = asn1_format_Modulus_as_text, .label = "Report speed schedule profile" },
	{ .type = &asn_DEF_SpeedScheduleSingle, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Single speed schedule" },
	{ .type = &asn_DEF_TimeManaged, .format = la_asn1_format_any_as_text, .label = "Time managed" },
	{ .type = &asn_DEF_TOAComputationTime, .format = asn1_format_Timesec_as_text, .label = "Computation time" },
	{ .type = &asn_DEF_TOARange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ToA range" },
	{ .type = &asn_DEF_TOARangeEarliestETA, .format = asn1_format_Timesec_as_text, .label = "ETA (earliest)" },
	{ .type = &asn_DEF_TOARangeLatestETA, .format = asn1_format_Timesec_as_text, .label = "ETA (latest)" },
	{ .type = &asn_DEF_TOARangeRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report ToA range" },
	{ .type = &asn_DEF_TOARangeRequestModulus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report ToA range" },
	{ .type = &asn_DEF_ThreeDPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Position" },
	{ .type = &asn_DEF_Timesec, .format = asn1_format_Timesec_as_text, .label = "Time" },
	{ .type = &asn_DEF_TrajectoryIntentStatus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Trajectory intent status" },
	{ .type = &asn_DEF_TurbulenceDeviation, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report turbulence deviation" },
	{ .type = &asn_DEF_TurbulenceEDRAverage, .format = asn1_format_TurbulenceEDRValue_as_text, .label = "Average EDR value" },
	{ .type = &asn_DEF_TurbulenceEDRPeak, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Peak EDR value" },
	{ .type = &asn_DEF_TurbulenceEDRValue, .format = asn1_format_TurbulenceEDRValue_as_text, .label = "EDR value" },
	{ .type = &asn_DEF_TurbulenceMinutesInPast, .format = asn1_format_TurbulenceMinutesInThePast_as_text, .label = "Time ago" },
	{ .type = &asn_DEF_TurbulenceObservationWindow, .format = asn1_format_TurbulenceObservationWindow_as_text, .label = "Observation window" },
	{ .type = &asn_DEF_TurbulencePeakThreshold, .format = asn1_format_TurbulenceEDRValue_as_text, .label = "Peak EDR threshold" },
	{ .type = &asn_DEF_TurnRadius, .format = asn1_format_TurnRadius_as_text, .label = "Turn radius" },
	{ .type = &asn_DEF_TurnRadiusNotAvailable, .format = la_asn1_format_label_only_as_text, .label = "Turn radius not available" },
	{ .type = &asn_DEF_VerticalClearanceDeviation, .format = asn1_format_LevelFeet_as_text, .label = "Report vertical clearance deviation exceeding" },
	{ .type = &asn_DEF_VerticalFlightManaged, .format = la_asn1_format_any_as_text, .label = "Vertical flight managed" },
	{ .type = &asn_DEF_VerticalRateDeviation, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report vertical rate deviation" },
	{ .type = &asn_DEF_VerticalRateDeviationLower, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Lower limit" },
	{ .type = &asn_DEF_VerticalRateDeviationUpper, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Upper limit" },
	{ .type = &asn_DEF_VerticalType, .format = asn1_format_VerticalType_as_text, .label = "Vertical type" },
	{ .type = &asn_DEF_Waypoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint" },
	{ .type = &asn_DEF_WaypointName, .format = la_asn1_format_any_as_text, .label = "Wpt name" },
	{ .type = &asn_DEF_WayPointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Waypoint sequence" },
	{ .type = &asn_DEF_WayPointSequenceElement, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint data" },
	{ .type = &asn_DEF_WindErrorModelUsed, .format = la_asn1_format_ENUM_as_text, .label = "Wind error model" },
	{ .type = &asn_DEF_WindQualityFlag, .format = la_asn1_format_ENUM_as_text, .label = "Wind quality flag" },
};

size_t asn1_icao_formatter_table_text_len = sizeof(asn1_icao_formatter_table_text) / sizeof(la_asn1_formatter);

la_asn1_formatter const asn1_acse_formatter_table_text[] = {
	{ .type = &asn_DEF_AARE_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Associate Response" },
	{ .type = &asn_DEF_AARQ_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Associate Request" },
	{ .type = &asn_DEF_ABRT_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Abort" },
	{ .type = &asn_DEF_ABRT_diagnostic, .format = la_asn1_format_ENUM_as_text, .label = "Cause" },
	{ .type = &asn_DEF_ABRT_source, .format = asn1_format_ABRT_source_as_text, .label = "Source" },
	{ .type = &asn_DEF_ACSE_apdu, .format = asn1_format_CHOICE_acse_as_text, .label = NULL },
	{ .type = &asn_DEF_AE_qualifier, .format = asn1_format_CHOICE_acse_as_text, .label = NULL },
	{ .type = &asn_DEF_AE_qualifier_form2, .format = la_asn1_format_any_as_text, .label = "AE qualifier" },
	{ .type = &asn_DEF_AP_title, .format = asn1_format_CHOICE_acse_as_text, .label = NULL },
	{ .type = &asn_DEF_AP_title_form2, .format = la_asn1_format_any_as_text, .label = "AP title" },
	{ .type = &asn_DEF_Application_context_name, .format = la_asn1_format_any_as_text, .label = "Application context name" },
	{ .type = &asn_DEF_Associate_result, .format = asn1_format_Associate_result_as_text, .label = "Associate result" },
	{ .type = &asn_DEF_Release_request_reason, .format = asn1_format_Release_request_reason_as_text, .label = "Reason" },
	{ .type = &asn_DEF_Release_response_reason, .format = asn1_format_Release_response_reason_as_text, .label = "Reason" },
	{ .type = &asn_DEF_RLRE_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Release Response" },
	{ .type = &asn_DEF_RLRQ_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Release Request" },
	// Supported in ATN ULCS, but not included in text output
	{ .type = &asn_DEF_ACSE_requirements, .format = NULL, .label = NULL },
	{ .type = &asn_DEF_Associate_source_diagnostic, .format = NULL, .label = NULL },
	{ .type = &asn_DEF_Association_information, .format = NULL, .label = NULL },
	{ .type = &asn_DEF_Authentication_value, .format = NULL, .label = NULL }
	// Not supported in ATN ULCS
	// { .type = &asn_DEF_AE_invocation_identifier, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AE_qualifier_form1, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AP_invocation_identifier, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AP_title_form1, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Application_context_name_list, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AttributeTypeAndValue, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_EXTERNALt, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Implementation_data, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Mechanism_name, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Name, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_RDNSequence, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_RelativeDistinguishedName, .format = NULL, .label = NULL },
};

size_t asn1_acse_formatter_table_text_len = sizeof(asn1_acse_formatter_table_text) / sizeof(la_asn1_formatter);
