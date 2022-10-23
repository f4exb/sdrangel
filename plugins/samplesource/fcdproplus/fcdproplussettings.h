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

#ifndef _FCDPROPLUS_FCDPROPLUSSETTINGS_H_
#define _FCDPROPLUS_FCDPROPLUSSETTINGS_H_

#include <QString>

struct FCDProPlusSettings {
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

	quint64 m_centerFrequency;
	bool m_rangeLow;
	bool m_lnaGain;
	bool m_mixGain;
	bool m_biasT;
	quint32 m_ifGain;
	qint32 m_ifFilterIndex;
	qint32 m_rfFilterIndex;
	qint32 m_LOppmTenths;
	quint32 m_log2Decim;
	fcPos_t m_fcPos;
	bool m_dcBlock;
	bool m_iqImbalance;
    bool m_transverterMode;
    qint64 m_transverterDeltaFrequency;
    bool m_iqOrder;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

	FCDProPlusSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const FCDProPlusSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* _FCDPROPLUS_FCDPROPLUSSETTINGS_H_ */
