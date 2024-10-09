///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <cmath>

#include "perioddial.h"
#include "wrappingdial.h"
#include "wrappingspinbox.h"

#include <QComboBox>
#include <QHBoxLayout>

PeriodDial::PeriodDial(QWidget *parent) :
    QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_dial = new WrappingDial();
    m_dial->setMinimum(1);
    m_dial->setMaximum(999);
    m_dial->setMaximumSize(24, 24);

    m_spinBox = new WrappingSpinBox();
    m_spinBox->setMinimum(1);
    m_spinBox->setMaximum(999);
    m_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);

    m_units = new QComboBox();
    m_units->addItem(QString("%1s").arg(QChar(0x3bc)));
    m_units->addItem("ms");
    m_units->addItem("s");

    m_layout->addWidget(m_dial);
    m_layout->addWidget(m_spinBox);
    m_layout->addWidget(m_units);

    connect(m_dial, &WrappingDial::valueChanged, this, &PeriodDial::on_dial_valueChanged);
    connect(m_dial, &WrappingDial::wrapUp, this, &PeriodDial::on_wrapUp);
    connect(m_dial, &WrappingDial::wrapDown, this, &PeriodDial::on_wrapDown);
    connect(m_spinBox, QOverload<int>::of(&WrappingSpinBox::valueChanged), this, &PeriodDial::on_spinBox_valueChanged);
    connect(m_spinBox, &WrappingSpinBox::wrapUp, this, &PeriodDial::on_wrapUp);
    connect(m_spinBox, &WrappingSpinBox::wrapDown, this, &PeriodDial::on_wrapDown);
    connect(m_units, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PeriodDial::on_units_currentIndexChanged);
}

void PeriodDial::on_dial_valueChanged(int dialValue)
{
    m_spinBox->setValue(dialValue);
    emit valueChanged(value());
}

void PeriodDial::on_spinBox_valueChanged(int boxValue)
{
    m_dial->setValue(boxValue);
}

void PeriodDial::on_units_currentIndexChanged(int index)
{
    (void) index;

    emit valueChanged(value());
}

void PeriodDial::on_wrapUp()
{
    int index = m_units->currentIndex();
    if (index < m_units->count() - 1) {
        m_units->setCurrentIndex(index + 1);
    }
}

void PeriodDial::on_wrapDown()
{
    int index = m_units->currentIndex();
    if (index > 0) {
        m_units->setCurrentIndex(index - 1);
    }
}

void PeriodDial::setValue(double newValue)
{
    double oldValue = value();
    int index;
    if (newValue < 1e-3) {
        index = 0;
    } else if (newValue < 1.0) {
        index = 1;
    } else {
        index = 2;
    }
    double scale = std::pow(10.0, 3 * (2 - index));
    int mantissa = std::round(newValue * scale);

    bool blocked = blockSignals(true);
    m_dial->setValue(mantissa);
    m_units->setCurrentIndex(index);
    blockSignals(blocked);

    if (newValue != oldValue) {
        emit valueChanged(value());
    }
}

double PeriodDial::value()
{
    int index = -3 * (2 - m_units->currentIndex()); // 0=s -3=ms -6=us
    double scale = std::pow(10.0, index);
    int mantissa = m_dial->value();
    return mantissa * scale;
}
