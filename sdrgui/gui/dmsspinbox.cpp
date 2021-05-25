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

#include <QLineEdit>

#include "util/units.h"

#include "dmsspinbox.h"

DMSSpinBox::DMSSpinBox(QWidget *parent) :
    QAbstractSpinBox(parent),
    m_value(0.0),
    m_minimum(0.0),
    m_maximum(360.0),
    m_units(DM)
{
    setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    connect(lineEdit(), SIGNAL(editingFinished()), this, SLOT(on_lineEdit_editingFinished()));
}

bool DMSSpinBox::hasValue() const
{
    return m_text.isNull();
}

double DMSSpinBox::value() const
{
    return m_value;
}

void DMSSpinBox::setValue(double degrees)
{
    if (m_value != degrees)
    {
        m_value = degrees;
        if (m_value < m_minimum) {
            m_value = m_minimum;
        }
        if (m_value > m_maximum) {
            m_value = m_maximum;
        }
        m_text = QString();
        emit valueChanged(m_value);
    }
    lineEdit()->setText(convertDegreesToText(m_value));
}

void DMSSpinBox::setRange(double minimum, double maximum)
{
    m_minimum = minimum;
    m_maximum = maximum;
}

void DMSSpinBox::setUnits(DisplayUnits units)
{
    m_units = units;
    if (hasValue()) {
        lineEdit()->setText(convertDegreesToText(m_value));
    }
}

void DMSSpinBox::setText(QString text)
{
    m_text = text;
    lineEdit()->setText(m_text);
}

void DMSSpinBox::stepBy(int steps)
{
    if (hasValue()) {
        setValue(m_value + (double)steps);
    }
}

QAbstractSpinBox::StepEnabled DMSSpinBox::stepEnabled() const
{
    QAbstractSpinBox::StepEnabled enabled = QAbstractSpinBox::StepNone;

    if (hasValue() && (m_value < m_maximum)) {
        enabled |= QAbstractSpinBox::StepUpEnabled;
    }
    if (hasValue() && (m_value > m_minimum)) {
        enabled |= QAbstractSpinBox::StepDownEnabled;
    }

    return enabled;
}

QString DMSSpinBox::convertDegreesToText(double degrees)
{
    if (m_units == DMS) {
        return Units::decimalDegreesToDegreeMinutesAndSeconds(degrees);
    } else if (m_units == DM) {
        return Units::decimalDegreesToDegreesAndMinutes(degrees);
    } else if (m_units == D) {
        return Units::decimalDegreesToDegrees(degrees);
    } else {
        return QString("%1").arg(degrees, 0, 'f', 2);
    }
}

void DMSSpinBox::on_lineEdit_editingFinished()
{
    QString text = lineEdit()->text().trimmed();
    float decimal;
    if (Units::degreeMinuteAndSecondsToDecimalDegrees(text, decimal)) {
        setValue(decimal);
    } else {
        qDebug() << "DMSSpinBox::on_lineEdit_editingFinished: Invalid format: " << text;
    }
}

