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
    unsigned int m_nbReceivers;
    quint64 m_rx1CenterFrequency;
    quint64 m_rx2CenterFrequency;
    quint64 m_rx3CenterFrequency;
    quint64 m_rx4CenterFrequency;
    quint64 m_rx5CenterFrequency;
    quint64 m_rx6CenterFrequency;
    quint64 m_rx7CenterFrequency;
    quint64 m_rx8CenterFrequency; //!< Hermes lite-2 or Pavel's Red Pitaya extension
    quint64 m_txCenterFrequency;
	unsigned int m_sampleRateIndex;
    unsigned int m_log2Decim;
    bool m_preamp;
    bool m_random;
    bool m_dither;
    bool m_duplex;
    bool m_dcBlock;
    bool m_iqCorrection;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    static const int m_maxReceivers = 8;

	MetisMISOSettings();
    MetisMISOSettings(const MetisMISOSettings& other);
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

    static int getSampleRateFromIndex(unsigned int index);
};


#endif /* _METISMISO_METISMISOSETTINGS_H_ */
