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

#ifndef SDRBASE_LIMERFE_LIMERFEUSBCALIB_H_
#define SDRBASE_LIMERFE_LIMERFEUSBCALIB_H_

#include <QMap>

class QByteArray;

class LimeRFEUSBCalib
{
public:
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    enum ChannelRange
    {
        WidebandLow,  //!< 1 - 1000 MHz
        WidebandHigh, //!< 1000 - 4000 MHz
        HAM_30MHz,    //!< Up to 30 MHz
        HAM_50_70MHz,
        HAM_144_146MHz,
        HAM_220_225MHz,
        HAM_430_440MHz,
        HAM_902_928MHz,
        HAM_1240_1325MHz,
        HAM_2300_2450MHz,
        HAM_3300_3500MHz,
        CellularBand1,
        CellularBand2,
        CellularBand3,
        CellularBand7,
        CellularBand38
    };

    QMap<int, double> m_calibrations; //!< Channel range to calibration value in floating point decibels

private:
    void serializeCalibMap(QByteArray& data) const;
    void deserializeCalibMap(QByteArray& data);
};

#endif // SDRBASE_LIMERFE_LIMERFEUSBCALIB_H_
