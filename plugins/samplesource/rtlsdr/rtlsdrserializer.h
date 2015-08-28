///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_RTLSDR_RTLSDRSERIALIZER_H_
#define PLUGINS_SAMPLESOURCE_RTLSDR_RTLSDRSERIALIZER_H_

#include "util/samplesourceserializer.h"

class RTLSDRSerializer
{
public:
	SampleSourceSerializer::Data m_data;

	static void writeSerializedData(const SampleSourceSerializer::Data& data, QByteArray& serializedData);
	static bool readSerializedData(const QByteArray& serializedData, SampleSourceSerializer::Data& data);
	static void setDefaults(SampleSourceSerializer::Data& data);
};

#endif /* PLUGINS_SAMPLESOURCE_RTLSDR_RTLSDRSERIALIZER_H_ */
