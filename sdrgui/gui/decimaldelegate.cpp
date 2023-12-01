///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QDoubleValidator>

#include "decimaldelegate.h"

// Allow "" or double
class DoubleOrEmptyValidator : public QDoubleValidator {
public:
    DoubleOrEmptyValidator(double bottom, double top, int decimals, QObject *parent = nullptr) :
        QDoubleValidator(bottom, top, decimals, parent)
    {
    }

    QValidator::State validate(QString& input, int &pos) const
    {
        if (input == "") {
            return QValidator::Acceptable;
        } else {
            return QDoubleValidator::validate(input, pos);
        }
    }
};

DecimalDelegate::DecimalDelegate(int precision) :
    m_precision(precision),
    m_min(-std::numeric_limits<double>::max()),
    m_max(std::numeric_limits<double>::max())
{
}

DecimalDelegate::DecimalDelegate(int precision, double min, double max) :
    m_precision(precision),
    m_min(min),
    m_max(max)
{
}

QString DecimalDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    (void) locale;
    bool ok;
    double d = value.toDouble(&ok);
    if (ok) {
        return QString::number(d, 'f', m_precision);
    } else {
        return value.toString();
    }
}

QWidget *DecimalDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    (void) option;
    (void) index;

    QLineEdit* editor = new QLineEdit(parent);
    DoubleOrEmptyValidator *validator = new DoubleOrEmptyValidator(m_min, m_max, m_precision);
    validator->setBottom(m_min);
    validator->setTop(m_max);
    editor->setValidator(validator);
    return editor;
}

void DecimalDelegate::setRange(double min, double max)
{
    m_min = min;
    m_max = max;
}
