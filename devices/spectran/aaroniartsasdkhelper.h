///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Frank Zosso, HB9FXQ                                        //
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef DEVICE_SPECTRAN_AARONIARTSASDKHELPER_H_
#define DEVICE_SPECTRAN_AARONIARTSASDKHELPER_H_

int LoadRTSAAPI_with_searchpath();

#define CFG_AARONIA_RTSA_INSTALL_DIRECTORY L"/opt/aaronia-rtsa-suite/Aaronia-RTSA-Suite-PRO"
#define CFG_AARONIA_XML_LOOKUP_DIRECTORY L"/opt/aaronia-rtsa-suite/Aaronia-RTSA-Suite-PRO"
#define CFG_CPP_RTSAAPI_DLIB L"libAaroniaRTSAAPI.so"
#define CFG_AARONIA_SDK_DIRECTORY L"\\opt\\aaronia-rtsa-suite\\Aaronia-RTSA-Suite-PRO"

#endif // DEVICE_SPECTRAN_AARONIARTSASDKHELPER_H_
