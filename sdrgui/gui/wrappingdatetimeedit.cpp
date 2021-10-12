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
    // We need to set this true in order for stepBy to be called
    // when minutes are at 59
    // However, this also causes dates/times to wrap from max to min
    // which we don't want, so in stepBy, we clip to max/min ourselves
    setWrapping(true);
}

void WrappingDateTimeEdit::stepBy(int steps)
{
    if (currentSection() == QDateTimeEdit::MonthSection)
    {
        clipAndSetDate(date().addMonths(steps));
    }
    else if (currentSection() == QDateTimeEdit::DaySection)
    {
        clipAndSetDate(date().addDays(steps));
    }
    else if (currentSection() == QDateTimeEdit::HourSection)
    {
        clipAndSetDateTime(dateTime().addSecs(steps*3600));
    }
    else if (currentSection() == QDateTimeEdit::MinuteSection)
    {
        clipAndSetDateTime(dateTime().addSecs(steps*60));
    }
    else if (currentSection() == QDateTimeEdit::SecondSection)
    {
        clipAndSetDateTime(dateTime().addSecs(steps));
    }
}

void WrappingDateTimeEdit::clipAndSetDate(QDate date)
{
    QDate max = maximumDate();
    QDate min = minimumDate();
    if (date > max) {
        setDate(max);
    } else if (date < min) {
        setDate(min);
    } else {
        setDate(date);
    }
}

void WrappingDateTimeEdit::clipAndSetDateTime(QDateTime dateTime)
{
    // We have set wrapping as described in the constructor, but we don't want
    // to wrap from max to min, so clip to this outself
    QDateTime max = maximumDateTime();
    QDateTime min = minimumDateTime();
    if (dateTime > max) {
        setDateTime(max);
    } else if (dateTime < min) {
        setDateTime(min);
    } else {
        setDateTime(dateTime);
    }
}
