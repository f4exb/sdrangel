///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef FCDLIB_EXPORT_H_
#define FCDLIB_EXPORT_H_

// cmake's WINDOWS_EXPORT_ALL_SYMBOLS only supports functions, so we need dllexport/import for global data
#ifdef _MSC_VER
#ifdef FCDLIB_EXPORTS
#define FCDLIB_API __declspec(dllexport)
#else
#define FCDLIB_API __declspec(dllimport)
#endif
#else
#define FCDLIB_API
#endif

#endif /* FCDLIB_EXPORT_H_ */
