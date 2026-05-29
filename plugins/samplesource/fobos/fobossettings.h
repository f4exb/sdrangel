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

#ifndef _FOBOS_FOBOSSETTINGS_H_
#define _FOBOS_FOBOSSETTINGS_H_

#include <QString>

struct FOBOSSettings {
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

    typedef enum {
        AutoCorrNone,
        AutoCorrDC,
        AutoCorrDCAndIQ,
        AutoCorrLast,
    } AutoCorrOptions;

    typedef enum {
        InputRF = 0,
        InputIQDirect,
        InputHF1Direct,
        InputHF2Direct
    } InputMode;

    // Reuse this legacy field as Fobos VGA gain index 0..31.
    // Keep the old enum name to avoid larger SDRangel/WebAPI changes in this UI-only iteration.
    typedef enum {
        ModulationNone = 0,
        ModulationAM = 1,
        ModulationFM = 2,
        ModulationPattern0 = 3,
        ModulationPattern1 = 4,
        ModulationPattern2 = 5,
        ModulationLast = 31
    } Modulation;

    QString m_title;
    quint64 m_centerFrequency;
	qint32 m_frequencyShift;
	quint32 m_sampleRate;
    quint32 m_log2Decim;
    fcPos_t m_fcPos;
	quint32 m_sampleSizeIndex;
	qint32 m_amplitudeBits;
    AutoCorrOptions m_autoCorrOptions;
    Modulation m_modulation;
    int m_modulationTone;   //!< 10'Hz
    int m_amModulation;     //!< percent
    int m_fmDeviation;      //!< 100'Hz
    float m_dcFactor;       //!< -1.0 < x < 1.0
    float m_iFactor;        //!< -1.0 < x < 1.0
    float m_qFactor;        //!< -1.0 < x < 1.0
    float m_phaseImbalance; //!< -1.0 < x < 1.0

    // uSDR-style Fobos operator settings.
    // These fields are applied through the Agile API either live or through controlled restart,
    // depending on whether the setting can be safely changed while streaming.
    InputMode m_inputMode;
    quint32 m_bandwidthPercent; //!< 0 = Auto, otherwise relative bandwidth percent such as 20/50/80/90
    quint32 m_gpoMask;          //!< bit mask for GPO 0..7
    bool m_externalClock;

    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

	FOBOSSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const FOBOSSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};





#endif /* _FOBOS_FOBOSSETTINGS_H_ */
