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

#ifndef PLUGINS_SAMPLESOURCE_BLADERF_BLADERFSERIALIZER_H_
#define PLUGINS_SAMPLESOURCE_BLADERF_BLADERFSERIALIZER_H_

#include "util/samplesourceserializer.h"

class BladeRFSerializer
{
public:
	struct BladeRFData
	{
		SampleSourceSerializer::Data m_data;
		bool m_xb200;
		quint32 m_xb200Path;
		quint32 m_xb200Filter;
	};

	static void writeSerializedData(const BladeRFData& data, QByteArray& serializedData);
	static bool readSerializedData(const QByteArray& serializedData, BladeRFData& data);
	static void setDefaults(BladeRFData& data);
};


#endif /* PLUGINS_SAMPLESOURCE_BLADERF_BLADERFSERIALIZER_H_ */
