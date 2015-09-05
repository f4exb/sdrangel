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
#ifndef FCDHIDCMD_H
#define FCDHIDCMD_H

/* Commands applicable in bootloader mode */
#define FCD_CMD_BL_QUERY                1  /*!< Returns string with "FCDAPP version". */
#define FCD_CMD_BL_RESET                8  /*!< Reset to application mode. */
#define FCD_CMD_BL_ERASE               24  /*!< Erase firmware from FCD flash. */
#define FCD_CMD_BL_SET_BYTE_ADDR       25  /*!< TBD */
#define FCD_CMD_BL_GET_BYTE_ADDR_RANGE 26  /*!< Get address range. */
#define FCD_CMD_BL_WRITE_FLASH_BLOCK   27  /*!< Write flash block. */
#define FCD_CMD_BL_READ_FLASH_BLOCK    28  /*!< Read flash block. */

/* Commands applicable in application mode */
#define FCD_CMD_APP_SET_FREQ_KHZ      100 /*!< Send with 3 byte unsigned little endian frequency in kHz. */
#define FCD_CMD_APP_SET_FREQ_HZ       101 /*!< Send with 4 byte unsigned little endian frequency in Hz, returns with actual frequency set in Hz */
#define FCD_CMD_APP_GET_FREQ_HZ       102 /*!< Returns 4 byte unsigned little endian frequency in Hz. */

#define FCD_CMD_APP_RESET             255 //!< Reset to bootloader

#include "fcdprohidcmd.h"
#include "fcdproplushidcmd.h"

#endif // FCDHIDCMD_H
