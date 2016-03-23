/**
 * @file backend_config.h
 *
 * @brief Compile-time backend selection
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef BLADERF_BACKEND_CONFIG_H__
#define BLADERF_BACKEND_CONFIG_H__

#define ENABLE_BACKEND_USB
#define ENABLE_BACKEND_LIBUSB
/* #undef ENABLE_BACKEND_CYAPI */
/* #undef ENABLE_BACKEND_DUMMY */
/* #undef ENABLE_BACKEND_LINUX_DRIVER */

#include "backend/backend.h"
#include "backend/usb/usb.h"

#ifdef ENABLE_BACKEND_DUMMY
    extern const struct backend_fns backend_fns_dummy;
#   define BACKEND_DUMMY &backend_fns_dummy,
#else
#   define BACKEND_DUMMY
#endif

#ifdef ENABLE_BACKEND_USB
    extern const struct backend_fns backend_fns_usb;
#   define BACKEND_USB  &backend_fns_usb,

#   ifdef ENABLE_BACKEND_LIBUSB
        extern const struct usb_driver usb_driver_libusb;
#       define BACKEND_USB_LIBUSB &usb_driver_libusb,
#   else
#       define BACKEND_USB_LIBUSB
#   endif

#   ifdef ENABLE_BACKEND_CYAPI
        extern const struct usb_driver usb_driver_cypress;
#       define BACKEND_USB_CYAPI &usb_driver_cypress,
#   else
#       define BACKEND_USB_CYAPI
#   endif


#   define BLADERF_USB_BACKEND_LIST { \
            BACKEND_USB_LIBUSB \
            BACKEND_USB_CYAPI \
    }

#   if !defined(ENABLE_BACKEND_LIBUSB) && !defined(ENABLE_BACKEND_CYAPI)
#       error "No USB backends are enabled. One or more must be enabled."
#   endif
#else
#   define BACKEND_USB
#endif

#if !defined(ENABLE_BACKEND_USB) && \
    !defined(ENABLE_BACKEND_DUMMY)
    #error "No backends are enabled. One more more must be enabled."
#endif

/* This list should be ordered by preference (highest first) */
#define BLADERF_BACKEND_LIST { \
    BACKEND_USB \
    BACKEND_DUMMY \
}

#endif
