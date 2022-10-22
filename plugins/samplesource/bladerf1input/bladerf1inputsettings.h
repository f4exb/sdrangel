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

#ifndef _BLADERF_BLADERFINPUTSETTINGS_H_
#define _BLADERF_BLADERFINPUTSETTINGS_H_

#include <QtGlobal>
#include <QString>
#include <libbladeRF.h>

struct BladeRF1InputSettings {
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

	quint64 m_centerFrequency;
    qint32 m_devSampleRate;
	qint32 m_lnaGain;
	qint32 m_vga1;
	qint32 m_vga2;
	qint32 m_bandwidth;
	quint32 m_log2Decim;
	fcPos_t m_fcPos;
	bool m_xb200;
	bladerf_xb200_path m_xb200Path;
	bladerf_xb200_filter m_xb200Filter;
	bool m_dcBlock;
	bool m_iqCorrection;
    bool m_iqOrder;
	QString m_fileRecordName;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

	BladeRF1InputSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const BladeRF1InputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* _BLADERF_BLADERFINPUTSETTINGS_H_ */
