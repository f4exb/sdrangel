/***************************************************************************
 *  This file is part of Qthid.
 *
 *  Copyright (C) 2010  Howard Long, G6LVB
 *  CopyRight (C) 2011  Alexandru Csete, OZ9AEC
 *                      Mario Lorenz, DL5MLO
 *
 *  Qthid is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Qthid is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Qthid.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************/

#ifndef _QTHID_H_
#define _QTHID_H_

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "fcdhidcmd.h"
#include "hidapi.h"

#include <inttypes.h>

#define FALSE 0
#define TRUE 1
typedef int BOOL;

/** \brief FCD mode enumeration. */
typedef enum {
    FCD_MODE_NONE,  /*!< No FCD detected. */
    FCD_MODE_DEAD,
    FCD_MODE_BL,    /*!< FCD present in bootloader mode. */
    FCD_MODE_APP    /*!< FCD present in aplpication mode. */
} FCD_MODE_ENUM; // The current mode of the FCD: none inserted, in bootloader mode or in normal application mode

/** \brief FCD capabilities that depend on both hardware and firmware. */
typedef struct {
    unsigned char hasBiasT;     /*!< Whether FCD has hardware bias tee (1=yes, 0=no) */
    unsigned char hasCellBlock; /*!< Whether FCD has cellular blocking. */
} FCD_CAPS_STRUCT;


//#define FCDPP // FIXME: the Pro / Pro+ switch should be handled better than this!
//const unsigned short _usVID=0x04D8;  /*!< USB vendor ID. */
//#ifdef FCDPP
//const unsigned short _usPID=0xFB31;  /*!< USB product ID. */
//#else
//const unsigned short _usPID=0xFB56;  /*!< USB product ID. */
//#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Open FCD device.
 * \return Pointer to the FCD HID device or NULL if none found
 *
 * This function looks for FCD devices connected to the computer and
 * opens the first one found.
 */
hid_device *fcdOpen(uint16_t usVID, uint16_t usPID, int whichdongle);

/** \brief Close FCD HID device. */
void fcdClose(hid_device *phd);

/** \brief Get FCD mode.
 * \return The current FCD mode.
 * \sa FCD_MODE_ENUM
 */
FCD_MODE_ENUM fcdGetMode(hid_device *phd);

/** \brief Get FCD firmware version as string.
 * \param str The returned vesion number as a 0 terminated string (must be pre-allocated)
 * \return The current FCD mode.
 * \sa FCD_MODE_ENUM
 */
FCD_MODE_ENUM fcdGetFwVerStr(hid_device *phd, char *str);

/** \brief Get hardware and firmware dependent FCD capabilities.
 * \param fcd_caps Pointer to an FCD_CAPS_STRUCT
 * \return The current FCD mode.
 *
 * This function queries the FCD and extracts the hardware and firmware dependent
 * capabilities. Currently these capabilities are:
 *  - Bias T (available since S/N TBD)
 *  - Cellular block (the certified version of the FCD)
 * When the FCD is in application mode, the string returned by the query command is
 * (starting at index 2):
 *    FCDAPP 18.08 Brd 1.0 No blk
 * 1.0 means no bias tee, 1.1 means there is a bias tee
 * 'No blk' means it is not cellular blocked.
 *
 * Ref: http://uk.groups.yahoo.com/group/FCDevelopment/message/303
 */
FCD_MODE_ENUM fcdGetCaps(hid_device *phd, FCD_CAPS_STRUCT *fcd_caps);

/** \brief Get hardware and firmware dependent FCD capabilities as string.
 * \param caps_str Pointer to a pre-allocated string buffer where the info will be copied.
 * \return The current FCD mode.
 *
 * This function queries the FCD and copies the returned string into the caps_str parameter.
 * THe return buffer must be at least 28 characters.
 * When the FCD is in application mode, the string returned by the query command is
 * (starting at index 2):
 *    FCDAPP 18.08 Brd 1.0 No blk
 * 1.0 means no bias tee, 1.1 means there is a bias tee
 * 'No blk' means it is not cellular blocked.
 *
 * Ref: http://uk.groups.yahoo.com/group/FCDevelopment/message/303
 */
FCD_MODE_ENUM fcdGetCapsStr(hid_device *phd, char *caps_str);

/** \brief Reset FCD to bootloader mode.
 * \return FCD_MODE_NONE
 *
 * This function is used to switch the FCD into bootloader mode in which
 * various firmware operations can be performed.
 */
FCD_MODE_ENUM fcdAppReset(hid_device *phd);

/** \brief Set FCD frequency with kHz resolution.
 * \param nFreq The new frequency in kHz.
 * \return The FCD mode.
 *
 * This function sets the frequency of the FCD with 1 kHz resolution. The parameter
 * nFreq must already contain any necessary frequency correction.
 *
 * \sa fcdAppSetFreq
 */
FCD_MODE_ENUM fcdAppSetFreqkHz(hid_device *phd, int nFreq);

/** \brief Set FCD frequency with Hz resolution.
 * \param nFreq The new frequency in Hz.
 * \return The FCD mode.
 *
 * This function sets the frequency of the FCD with 1 Hz resolution. The parameter
 * nFreq must already contain any necessary frequency correction.
 *
 * \sa fcdAppSetFreq
 */
FCD_MODE_ENUM fcdAppSetFreq(hid_device *phd, int nFreq);

/** \brief Reset FCD to application mode.
 * \return FCD_MODE_NONE
 *
 * This function is used to switch the FCD from bootloader mode
 * into application mode.
 */
FCD_MODE_ENUM fcdBlReset(hid_device *phd);

/** \brief Erase firmware from FCD.
 * \return The FCD mode
 *
 * This function deletes the firmware from the FCD. This is required
 * before writing new firmware into the FCD.
 *
 * \sa fcdBlWriteFirmware
 */
FCD_MODE_ENUM fcdBlErase(hid_device *phd);

/** \brief Write new firmware into the FCD.
 * \param pc Pointer to the new firmware data
 * \param n64size The number of bytes in the data
 * \return The FCD mode
 *
 * This function is used to upload new firmware into the FCD flash.
 *
 * \sa fcdBlErase
 */
FCD_MODE_ENUM fcdBlWriteFirmware(hid_device *phd, char *pc, int64_t n64Size);

/** \brief Verify firmware in FCd flash.
 * \param pc Pointer to firmware data to verify against.
 * \param n64Size Size of the data in pc.
 * \return The FCD_MODE_BL if verification was succesful.
 *
 * This function verifies the firmware currently in the FCd flash against the firmware
 * image pointed to by pc. The function return FCD_MODE_BL if the verification is OK and
 * FCD_MODE_APP otherwise.
 */
FCD_MODE_ENUM fcdBlVerifyFirmware(hid_device *phd, char *pc, int64_t n64Size);

/** \brief Write FCD parameter (e.g. gain or filter)
 * \param u8Cmd The command byte / parameter ID, see FCD_CMD_APP_SET_*
 * \param pu8Data The parameter value to be written
 * \param u8len Length of pu8Data in bytes
 * \return One of FCD_MODE_NONE, FCD_MODE_APP or FCD_MODE_BL (see description)
 *
 * This function can be used to set the value of a parameter in the FCD for which there is no
 * high level API call. It gives access to the low level API of the FCD and the caller is expected
 * to be aware of the various FCD commands, since they are required to be supplied as parameter
 * to this function.
 *
 * The return value can be used to determine the success or failure of the command execution:
 * - FCD_MODE_APP : Reply from FCD was as expected (nominal case).
 * - FCD_MODE_BL : Reply from FCD was not as expected.
 * - FCD_MODE_NONE : No FCD was found
 */
FCD_MODE_ENUM fcdAppSetParam(hid_device *phd, uint8_t u8Cmd, uint8_t *pu8Data, uint8_t u8len);

/** \brief Read FCD parameter (e.g. gain or filter)
 * \param u8Cmd The command byte / parameter ID, see FCD_CMD_APP_GET_*
 * \param pu8Data TPointer to buffer where the parameter value(s) will be written
 * \param u8len Length of pu8Data in bytes
 * \return One of FCD_MODE_NONE, FCD_MODE_APP or FCD_MODE_BL (see description)
 *
 * This function can be used to read the value of a parameter in the FCD for which there is no
 * high level API call. It gives access to the low level API of the FCD and the caller is expected
 * to be aware of the various FCD commands, since they are required to be supplied as parameter
 * to this function.
 *
 * The return value can be used to determine the success or failure of the command execution:
 * - FCD_MODE_APP : Reply from FCD was as expected (nominal case).
 * - FCD_MODE_BL : Reply from FCD was not as expected.
 * - FCD_MODE_NONE : No FCD was found
 */
FCD_MODE_ENUM fcdAppGetParam(hid_device *phd, uint8_t u8Cmd, uint8_t *pu8Data, uint8_t u8len);

#ifdef __cplusplus
}
#endif

#endif // _QTHID_H_
