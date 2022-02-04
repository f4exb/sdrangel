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

#include "util/simpleserializer.h"
#include "spectrumcalibrationpoint.h"

QByteArray SpectrumCalibrationPoint::serialize() const
{
    SimpleSerializer s(1);

    s.writeS64(1, m_frequency);
    s.writeFloat(2, m_powerRelativeReference);
    s.writeFloat(3, m_powerCalibratedReference);

    return s.final();
}

bool SpectrumCalibrationPoint::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid()) {
		return false;
	}

    if (d.getVersion() == 1)
    {
        d.readS64(1, &m_frequency, 0);
        d.readFloat(2, &m_powerRelativeReference, 1.0f);
        d.readFloat(3, &m_powerCalibratedReference, 1.0f);

        return true;
    }
    else
    {
        return false;
    }

}
