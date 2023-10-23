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

#include <QLineEdit>

#include "frequencydelegate.h"

FrequencyDelegate::FrequencyDelegate(const QString& units, int precision, bool group) :
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
        QLocale l(locale);
        if (m_group) {
            l.setNumberOptions(l.numberOptions() & ~QLocale::OmitGroupSeparator);
        }
        else {
            l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);
        }

        if (m_units == "Auto")
        {
            if (v == 0)
            {
                return "0 Hz";
            }
            else
            {
                QString s = QString::number(v);
                int scale = 1;
                while (s.endsWith("000"))
                {
                    s.chop(3);
                    scale *= 1000;
                }
                v /= scale;
                double d = v;
                if ((abs(v) >= 1000) && (m_precision >= 3))
                {
                    scale *= 1000;
                    d /= 1000.0;
                }
                QString units;
                if (scale == 1) {
                    units = "Hz";
                } else if (scale == 1000) {
                    units = "kHz";
                } else if (scale == 1000000) {
                    units = "MHz";
                } else if (scale == 1000000000) {
                    units = "GHz";
                }
                if (scale == 1) {
                    s = l.toString(d, 'f', 0);
                } else {
                    s = l.toString(d, 'f', m_precision);
                }

                return QString("%1 %2").arg(s).arg(units);
            }
        }
        else
        {
            double d;
            if (m_units == "GHz") {
                d = v / 1000000000.0;
            }
            else if (m_units == "MHz") {
                d = v / 1000000.0;
            }
            else if (m_units == "kHz") {
                d = v / 1000.0;
            }
            else {
                d = v;
            }


            QString s = l.toString(d, 'f', m_precision);

            return QString("%1 %2").arg(s).arg(m_units);
        }
    }
    else
    {
        return value.toString();
    }
}

QWidget* FrequencyDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    (void) option;
    (void) index;

    QLineEdit* editor = new QLineEdit(parent);
    QIntValidator* validator = new QIntValidator();
    validator->setBottom(0);
    editor->setValidator(validator);
    return editor;
}

void FrequencyDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* line = static_cast<QLineEdit*>(editor);
    line->setText(value);
}

void FrequencyDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    QLineEdit* line = static_cast<QLineEdit*>(editor);
    QString value = line->text();
    model->setData(index, value);
}

void FrequencyDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    (void) index;
    editor->setGeometry(option.rect);
}
