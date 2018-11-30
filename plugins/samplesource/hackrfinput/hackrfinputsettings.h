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

#ifndef _HACKRF_HACKRFINPUTSETTINGS_H_
#define _HACKRF_HACKRFINPUTSETTINGS_H_

#include <QtGlobal>
#include <QString>

struct HackRFInputSettings {
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

	quint64 m_centerFrequency;
	qint32  m_LOppmTenths;
	quint32 m_bandwidth;
	quint32 m_lnaGain;
	quint32 m_vgaGain;
	quint32 m_log2Decim;
	fcPos_t m_fcPos;
	quint64 m_devSampleRate;
	bool m_biasT;
	bool m_lnaExt;
	bool m_dcBlock;
	bool m_iqCorrection;
	bool m_linkTxFrequency;
	QString m_fileRecordName;

	HackRFInputSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
};

#endif /* _HACKRF_HACKRFINPUTSETTINGS_H_ */
