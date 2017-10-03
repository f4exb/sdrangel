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

#ifndef _RTLSDR_RTLSDRSETTINGS_H_
#define _RTLSDR_RTLSDRSETTINGS_H_

struct RTLSDRSettings {
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

	int m_devSampleRate;
	bool m_lowSampleRate;
	quint64 m_centerFrequency;
	qint32 m_gain;
	qint32 m_loPpmCorrection;
	quint32 m_log2Decim;
	fcPos_t m_fcPos;
	bool m_dcBlock;
	bool m_iqImbalance;
	bool m_agc;
	bool m_noModMode;
    bool m_transverterMode;
	qint64 m_transverterDeltaFrequency;

	RTLSDRSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
};





#endif /* _RTLSDR_RTLSDRSETTINGS_H_ */
