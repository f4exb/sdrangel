///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SPECTRUMCALIBRATIONPOINT_H
#define INCLUDE_SPECTRUMCALIBRATIONPOINT_H

#include <QtGlobal>

#include "export.h"

struct SDRBASE_API SpectrumCalibrationPoint
{
    qint64 m_frequency; //!< frequency in Hz
    float m_powerRelativeReference;   //!< relative power level on 0..1 scale
    float m_powerCalibratedReference; //!< calibrated power level on 0..x scale. x limited to 10000 (40dB) in the GUI.

    SpectrumCalibrationPoint() :
        m_frequency(0),
        m_powerRelativeReference(1.0f),
        m_powerCalibratedReference(1.0f)
    {}

    SpectrumCalibrationPoint(
        quint64 frequency,
        float powerRelativeReference,
        float powerAbsoluteReference
    ) :
        m_frequency(frequency),
        m_powerRelativeReference(powerRelativeReference),
        m_powerCalibratedReference(powerAbsoluteReference)
    {}

    SpectrumCalibrationPoint(const SpectrumCalibrationPoint& other) = default;
    SpectrumCalibrationPoint& operator=(const SpectrumCalibrationPoint&) = default;

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};


#endif // INCLUDE_SPECTRUMCALIBRATIONPOINTs_H
