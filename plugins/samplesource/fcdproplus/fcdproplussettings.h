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

#ifndef _FCDPROPLUS_FCDPROPLUSSETTINGS_H_
#define _FCDPROPLUS_FCDPROPLUSSETTINGS_H_

struct FCDProPlusSettings {
	quint64 m_centerFrequency;
	bool m_rangeLow;
	bool m_lnaGain;
	bool m_mixGain;
	bool m_biasT;
	quint32 m_ifGain;
	qint32 m_ifFilterIndex;
	qint32 m_rfFilterIndex;
	qint32 m_LOppmTenths;
	bool m_dcBlock;
	bool m_iqImbalance;
    bool m_transverterMode;
    qint64 m_transverterDeltaFrequency;

	FCDProPlusSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
};

#endif /* _FCDPROPLUS_FCDPROPLUSSETTINGS_H_ */
