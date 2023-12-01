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

#include <limits>

#include <QValidator>
#include <QRegularExpression>

// Like QIntValidator but for qint64
class Int64Validator : public QValidator
{
    Q_OBJECT

public:

    Int64Validator(QObject *parent = nullptr) :
        QValidator(parent),
        m_bottom(-std::numeric_limits<qint64>::max()),
        m_top(std::numeric_limits<qint64>::max())
    {
    }

    Int64Validator(qint64 bottom, qint64 top, QObject *parent = nullptr) :
        QValidator(parent),
        m_bottom(bottom),
        m_top(top)
    {
    }

    void setBottom(qint64 bottom)
    {
        m_bottom = bottom;
    }

    void setTop(qint64 top)
    {
        m_top = top;
    }

    void setRange(qint64 bottom, qint64 top)
    {
        m_bottom = bottom;
        m_top = top;
    }

    qint64 bottom() const
    {
        return m_bottom;
    }

    qint64 top() const
    {
        return m_top;
    }

    QValidator::State validate(QString& input, int &pos) const;

private:
    qint64 m_bottom;
    qint64 m_top;
};
