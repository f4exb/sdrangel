///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef _SDRPLAY_SDRPLAYSETTINGS_H_
#define _SDRPLAY_SDRPLAYSETTINGS_H_

#include <stdint.h>
#include <QByteArray>
#include <QString>
#include <QDebug>

struct SDRPlaySettings {
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

	uint64_t m_centerFrequency;
	qint32 m_tunerGain;
	int32_t  m_LOppmTenths;
    uint32_t m_frequencyBandIndex;
    uint32_t m_ifFrequencyIndex;
    uint32_t m_bandwidthIndex;
	uint32_t m_devSampleRateIndex;
	uint32_t m_log2Decim;
	fcPos_t m_fcPos;
	bool m_dcBlock;
	bool m_iqCorrection;
	bool m_tunerGainMode; // true: tuner (table) gain, false: manual (LNA, Mixer, BB) gain
	bool m_lnaOn;
	bool m_mixerAmpOn;
	int  m_basebandGain;
    QString m_fileRecordName;

	SDRPlaySettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void debug(const char *title) const
	{
	    qDebug() << QString(title)
	            << ": m_centerFrequency: " << m_centerFrequency
	            << " m_tunerGain: " << m_tunerGain
	            << " m_LOppmTenths: " << m_LOppmTenths
	            << " m_frequencyBandIndex: " << m_frequencyBandIndex
	            << " m_ifFrequencyIndex: " << m_ifFrequencyIndex
	            << " m_bandwidthIndex: " << m_bandwidthIndex
	            << " m_devSampleRateIndex: " << m_devSampleRateIndex
	            << " m_log2Decim: " << m_log2Decim
	            << " m_fcPos: " << m_fcPos
	            << " m_dcBlock: " << m_dcBlock
	            << " m_iqCorrection: " << m_iqCorrection
	            << " m_tunerGainMode: " << m_tunerGainMode
	            << " m_lnaOn: " << m_lnaOn
	            << " m_mixerAmpOn: " << m_mixerAmpOn
	            << " m_basebandGain: " << m_basebandGain;
	}
};

#endif /* _SDRPLAY_SDRPLAYSETTINGS_H_ */
