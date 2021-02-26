///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "wrappingdatetimeedit.h"

WrappingDateTimeEdit::WrappingDateTimeEdit(QWidget *parent) :
    QDateTimeEdit(parent)
{
    setWrapping(true);
}

void WrappingDateTimeEdit::stepBy(int steps)
{
    if (currentSection() == QDateTimeEdit::MonthSection)
        setDate(date().addMonths(steps));
    else if (currentSection() == QDateTimeEdit::DaySection)
        setDate(date().addDays(steps));
    else if (currentSection() == QDateTimeEdit::HourSection)
    {
        QTime t = time();
        int h = t.hour();

        setTime(time().addSecs(steps*3600));

        if ((h < -steps) && (steps < 0))
            setDate(date().addDays(-1));
        else if ((h + steps > 23) && (steps > 0))
            setDate(date().addDays(1));
    }
    else if (currentSection() == QDateTimeEdit::MinuteSection)
    {
        QTime t = time();
        int h = t.hour();
        int m = t.minute();

        setTime(time().addSecs(steps*60));

        if ((m < -steps) && (steps < 0) && (h == 0))
            setDate(date().addDays(-1));
        else if ((m + steps > 59) && (steps > 0) && (h == 23))
            setDate(date().addDays(1));
    }
    else if (currentSection() == QDateTimeEdit::SecondSection)
    {
        QTime t = time();
        int h = t.hour();
        int m = t.minute();
        int s = t.second();

        setTime(time().addSecs(steps));

        if ((s < -steps) && (steps < 0) && (h == 0) && (m == 0))
            setDate(date().addDays(-1));
        else if ((s + steps > 59) && (steps > 0) && (h == 23) && (m == 59))
            setDate(date().addDays(1));
    }
}
