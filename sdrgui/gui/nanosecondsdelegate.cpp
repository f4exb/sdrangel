///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "nanosecondsdelegate.h"

NanoSecondsDelegate::NanoSecondsDelegate()
{
}

QString NanoSecondsDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    (void) locale;

    if (value.toString() == "")
    {
        return "";
    }
    else
    {
        double timeInNanoSec = value.toDouble();
        QString s;

        if (timeInNanoSec < 1e3) {
            s = QString("%1 ns").arg(timeInNanoSec, 0, 'f', 3);
        } else if (timeInNanoSec < 1e6) {
            s = QString("%1 us").arg(timeInNanoSec/1e3, 0, 'f', 3);
        } else if (timeInNanoSec < 1e9) {
            s = QString("%1 ms").arg(timeInNanoSec/1e6, 0, 'f', 3);
        } else {
            s = QString("%1 s").arg(timeInNanoSec/1e9, 0, 'f', 3);
        }
        return s;
    }
}
