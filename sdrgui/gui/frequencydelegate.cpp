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

#include "frequencydelegate.h"

FrequencyDelegate::FrequencyDelegate(QString units, int precision, bool group) :
    m_units(units),
    m_precision(precision),
    m_group(group)
{
}

QString FrequencyDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    bool ok;
    qlonglong v = value.toLongLong(&ok);
    if (ok)
    {
        double d;
        if (m_units == "GHz") {
            d = v / 1000000000.0;
        } else if (m_units == "MHz") {
            d = v / 1000000.0;
        } else if (m_units == "kHz") {
            d = v / 1000.0;
        } else {
            d = v;
        }

        QLocale l(locale);
        if (m_group) {
            l.setNumberOptions(l.numberOptions() & ~QLocale::OmitGroupSeparator);
        } else {
            l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);
        }
        QString s = l.toString(d, 'f', m_precision);

        return QString("%1 %2").arg(s).arg(m_units);
    }
    else
    {
        return value.toString();
    }
}
