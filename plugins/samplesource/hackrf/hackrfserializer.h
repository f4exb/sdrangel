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

#ifndef PLUGINS_SAMPLESOURCE_HACKRF_HACKRFSERIALIZER_H_
#define PLUGINS_SAMPLESOURCE_HACKRF_HACKRFSERIALIZER_H_

#include "util/samplesourceserializer.h"

class HackRFSerializer
{
public:
	struct AirspyData
	{
		SampleSourceSerializer::Data m_data;
		qint32 m_LOppmTenths;
		quint32 m_sampleRateIndex;
		quint32 m_log2Decim;
		qint32 m_fcPos;
		quint32 m_lnaGain;
		quint32 m_imjRejFilterIndex;
		quint32 m_bandwidthIndex;
		quint32 m_vgaGain;
		bool m_biasT;
		bool m_lnaExt;
	};

	static void writeSerializedData(const AirspyData& data, QByteArray& serializedData);
	static bool readSerializedData(const QByteArray& serializedData, AirspyData& data);
	static void setDefaults(AirspyData& data);
};


#endif /* PLUGINS_SAMPLESOURCE_HACKRF_HACKRFSERIALIZER_H_ */
