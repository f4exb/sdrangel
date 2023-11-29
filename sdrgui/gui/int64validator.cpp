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

#include "int64validator.h"

QValidator::State Int64Validator::validate(QString& input, int &pos) const
{
    if (input == "") {
        return QValidator::Acceptable;
    }

    if ((m_bottom < 0) && (input == "-")) {
        return QValidator::Intermediate;
    }

    QRegularExpression re("-?\\d+");
    QRegularExpressionMatch match = re.match(input);
    if (match.hasMatch())
    {
        qint64 value = input.toLongLong();
        if (value < m_bottom) {
            return QValidator::Invalid;
        }
        if (value > m_top) {
            return QValidator::Invalid;
        }
        return QValidator::Acceptable;
    }
    else
    {
        return QValidator::Invalid;
    }
}
