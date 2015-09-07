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

#ifndef PLUGINS_SAMPLESOURCE_FCD_FCDPROSERIALIZER_H_
#define PLUGINS_SAMPLESOURCE_FCD_FCDPROSERIALIZER_H_

#include "util/samplesourceserializer.h"

class FCDProSerializer
{
public:
	struct FCDData
	{
		SampleSourceSerializer::Data m_data;
		qint32 m_LOppmTenths;
		bool m_biasT;
		qint32 m_lnaGainIndex;
		qint32 m_rfFilterIndex;
		qint32 m_lnaEnhanceIndex;
		qint32 m_bandIndex;
		qint32 m_mixerGainIndex;
		qint32 m_mixerFilterIndex;
		qint32 m_biasCurrentIndex;
		qint32 m_modeIndex;
		qint32 m_gain1Index;
		qint32 m_rcFilterIndex;
		qint32 m_gain2Index;
		qint32 m_gain3Index;
		qint32 m_gain4Index;
		qint32 m_ifFilterIndex;
		qint32 m_gain5Index;
		qint32 m_gain6Index;
	};

	static void writeSerializedData(const FCDData& data, QByteArray& serializedData);
	static bool readSerializedData(const QByteArray& serializedData, FCDData& data);
	static void setDefaults(FCDData& data);
};



#endif /* PLUGINS_SAMPLESOURCE_FCD_FCDPROSERIALIZER_H_ */
