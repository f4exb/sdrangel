/*
 * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SDRANGELOVE_EXPORT_H
#define __SDRANGELOVE_EXPORT_H

#if defined __GNUC__
#  if __GNUC__ >= 4
#    define __SDR_EXPORT   __attribute__((visibility("default")))
#    define __SDR_IMPORT   __attribute__((visibility("default")))
#  else
#    define __SDR_EXPORT
#    define __SDR_IMPORT
#  endif
#elif _MSC_VER
#  define __SDR_EXPORT     __declspec(dllexport)
#  define __SDR_IMPORT     __declspec(dllimport)
#else
#  define __SDR_EXPORT
#  define __SDR_IMPORT
#endif

#ifndef sdrangelove_STATIC
#	ifdef sdrangelove_EXPORTS
#	define SDRANGELOVE_API __SDR_EXPORT
#	else
#	define SDRANGELOVE_API __SDR_IMPORT
#	endif
#else
#define SDRANGELOVE_API
#endif
#endif /* __SDRANGELOVE_EXPORT_H */
