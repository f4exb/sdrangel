///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015, 2017-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#ifndef _AIRSPYHFF_AIRSPYHFSETTINGS_H_
#define _AIRSPYHFF_AIRSPYHFSETTINGS_H_

#include <QString>

struct AirspyHFSettings
{
	quint64 m_centerFrequency;
    qint32  m_LOppmTenths;
	quint32 m_devSampleRateIndex;
	quint32 m_log2Decim;
    bool m_transverterMode;
    qint64 m_transverterDeltaFrequency;
    bool m_iqOrder;
    quint32 m_bandIndex;
    bool m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    bool m_useDSP;
    bool m_useAGC;
    bool m_agcHigh;
    bool m_useLNA;
    quint32  m_attenuatorSteps;
	bool m_dcBlock;
	bool m_iqCorrection;
	float m_replayOffset; //!< Replay offset in seconds
	float m_replayLength; //!< Replay buffer size in seconds
	float m_replayStep;   //!< Replay forward/back step size in seconds
	bool m_replayLoop;    //!< Replay buffer repeatedly without recording new data

    AirspyHFSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const AirspyHFSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* _AIRSPYHFF_AIRSPYHFSETTINGS_H_ */
