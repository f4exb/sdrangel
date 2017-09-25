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

#ifndef _PLUTOSDR_PLUTOSDROUTPUTSETTINGS_H_
#define _PLUTOSDR_PLUTOSDROUTPUTSETTINGS_H_

#include <QtGlobal>
#include <stdint.h>

struct PlutoSDROutputSettings {
    enum RFPath
    {
        RFPATH_A = 0,
        RFPATH_B,
        RFPATH_END
    };

    // global settings to be saved
	quint64 m_centerFrequency;
	// common device settings
    quint64 m_devSampleRate;      //!< Host interface sample rate
    qint32  m_LOppmTenths;        //!< XO correction
    bool    m_lpfFIREnable;       //!< enable digital lowpass FIR filter
    quint32 m_lpfFIRBW;           //!< digital lowpass FIR filter bandwidth (Hz)
    quint32 m_lpfFIRlog2Interp;   //!< digital lowpass FIR filter log2 of interpolation factor (0..2)
    int     m_lpfFIRGain;         //!< digital lowpass FIR filter gain (dB)
    // individual channel settings
    quint32 m_log2Interp;
    quint32 m_lpfBW;              //!< analog lowpass filter bandwidth (Hz)
    qint32  m_att;                //!< "hardware" attenuation in dB fourths
    RFPath  m_antennaPath;
    bool m_transverterMode;
    qint64 m_transverterDeltaFrequency;


    PlutoSDROutputSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    static void translateRFPath(RFPath path, QString& s);
};

#endif /* _PLUTOSDR_PLUTOSDROUTPUTSETTINGS_H_ */
