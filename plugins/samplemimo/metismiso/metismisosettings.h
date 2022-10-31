///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef _METISMISO_METISMISOSETTINGS_H_
#define _METISMISO_METISMISOSETTINGS_H_

#include <QString>

struct MetisMISOSettings {
    static const int m_maxReceivers = 8;
    unsigned int m_nbReceivers;
    bool m_txEnable;
    quint64 m_rxCenterFrequencies[m_maxReceivers];
    unsigned int m_rxSubsamplingIndexes[m_maxReceivers];
    quint64 m_txCenterFrequency;
    bool    m_rxTransverterMode;
    qint64  m_rxTransverterDeltaFrequency;
    bool    m_txTransverterMode;
    qint64  m_txTransverterDeltaFrequency;
    bool    m_iqOrder;
	unsigned int m_sampleRateIndex;
    unsigned int m_log2Decim;
    int  m_LOppmTenths;
    bool m_preamp;
    bool m_random;
    bool m_dither;
    bool m_duplex;
    bool m_dcBlock;
    bool m_iqCorrection;
    unsigned int m_txDrive;
    int m_streamIndex;
    int m_spectrumStreamIndex; //!< spectrum source
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

	MetisMISOSettings();
    MetisMISOSettings(const MetisMISOSettings& other);
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    MetisMISOSettings& operator=(const MetisMISOSettings&) = default;
    void applySettings(const QStringList& settingsKeys, const MetisMISOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    static int getSampleRateFromIndex(unsigned int index);
};


#endif /* _METISMISO_METISMISOSETTINGS_H_ */
