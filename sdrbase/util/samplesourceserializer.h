///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_UTIL_SAMPLESOURCESERIALIZER_H_
#define INCLUDE_UTIL_SAMPLESOURCESERIALIZER_H_

#include "util/simpleserializer.h"
#include "export.h"

class SDRBASE_API SampleSourceSerializer
{
public:
	struct Data
	{
		quint64 m_frequency; //!< RF center frequency
		qint32 m_correction; //!< LO correction factor
		qint32 m_rate;       //!< RF sampler sample rate
		quint32 m_log2Decim; //!< Decimation ratio log2
		qint32 m_bandwidth;  //!< RF bandwidth
		qint32 m_fcPosition; //!< Decimated band placement (infradyne, supradyne, centered)
		qint32 m_lnaGain;    //!< RF LNA gain
		qint32 m_RxGain1;    //!< Rx first stage amplifier gain
		qint32 m_RxGain2;    //!< Rx second stage amplifier gain
		qint32 m_RxGain3;    //!< Rx third stage amplifier gain
	};

	static void writeSerializedData(const Data& data, QByteArray& serializedData);
	static bool readSerializedData(const QByteArray& serializedData, Data& data);
	static void setDefaults(Data& data);
	static uint getSerializerVersion() { return m_version; }

protected:
	static const uint m_version;
};



#endif /* INCLUDE_UTIL_SAMPLESOURCESERIALIZER_H_ */
