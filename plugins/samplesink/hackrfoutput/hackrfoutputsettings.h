///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef _HACKRF_HACKRFOUTPUTSETTINGS_H_
#define _HACKRF_HACKRFOUTPUTSETTINGS_H_

#include <QtGlobal>

struct HackRFOutputSettings {
	quint64 m_centerFrequency;
	qint32  m_LOppmTenths;
	quint32 m_bandwidth;
	quint32 m_vgaGain;
	quint32 m_log2Interp;
	quint64 m_devSampleRate;
	bool m_biasT;
	bool m_lnaExt;

	HackRFOutputSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
};

#endif /* _HACKRF_HACKRFOUTPUTSETTINGS_H_ */
