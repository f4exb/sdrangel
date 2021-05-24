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

#ifndef SDRGUI_GUI_DMSSPINBOX_H
#define SDRGUI_GUI_DMSSPINBOX_H

#include <QAbstractSpinBox>

#include "export.h"

// Spin box for displaying degrees in decimal or degrees, minutes and seconds format
// Can also be assigned a text string
class SDRGUI_API DMSSpinBox : public QAbstractSpinBox {
    Q_OBJECT

public:
    enum DisplayUnits {DMS, DM, D, Decimal};

    explicit DMSSpinBox(QWidget *parent = nullptr);
    bool hasValue() const;
    double value() const;
    void setValue(double degrees);
    void setRange(double minimum, double maximum);
    void setUnits(DisplayUnits units);
    void setText(QString text);
    virtual void stepBy(int steps) override;

protected:

    virtual QAbstractSpinBox::StepEnabled stepEnabled() const override;
    QString convertDegreesToText(double degrees);

private:
    QString m_text;
    double m_value;
    double m_minimum;
    double m_maximum;
    DisplayUnits m_units;

signals:
    void valueChanged(double newValue);

private slots:
    void on_lineEdit_editingFinished();

};

#endif // SDRGUI_GUI_DMSSPINBOX_H
