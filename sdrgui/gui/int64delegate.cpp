///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QLineEdit>

#include "int64delegate.h"
#include "int64validator.h"

Int64Delegate::Int64Delegate() :
    m_min(-std::numeric_limits<qint64>::max()),
    m_max(std::numeric_limits<qint64>::max())
{
}

Int64Delegate::Int64Delegate(qint64 min, qint64 max) :
    m_min(min),
    m_max(max)
{
}

QString Int64Delegate::displayText(const QVariant &value, const QLocale &locale) const
{
    (void) locale;

    return value.toString();
}

QWidget *Int64Delegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    (void) option;
    (void) index;

    QLineEdit* editor = new QLineEdit(parent);
    Int64Validator* validator = new Int64Validator();
    validator->setBottom(m_min);
    validator->setTop(m_max);
    editor->setValidator(validator);
    return editor;
}

void Int64Delegate::setRange(qint64 min, qint64 max)
{
    m_min = min;
    m_max = max;
}
