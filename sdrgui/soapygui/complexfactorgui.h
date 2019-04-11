///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

// This is an interface to an elementary GUI item used to get/set a normalized complex value from the GUI
// There is an automatic check to activate/deactivate possible automatic setting
// It is intended to be used primarily for DC offset and IQ imbalance corrections

#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_COMPLEXFACTORGUI_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_COMPLEXFACTORGUI_H_

#include <QWidget>
#include <QString>

#include "export.h"

namespace Ui {
    class ComplexFactorGUI;
}

class SDRGUI_API ComplexFactorGUI : public QWidget
{
    Q_OBJECT
public:
    explicit ComplexFactorGUI(QWidget *parent = 0);
    ~ComplexFactorGUI();

    double getModule() const;
    double getArgument() const;
    bool getAutomatic() const;

    void setModule(double value);
    void setArgument(double value);
    void setAutomatic(bool automatic);
    void setAutomaticEnable(bool enable);

    void setLabel(const QString& text);
    void setToolTip(const QString& text);

signals:
    void moduleChanged(double value);
    void argumentChanged(double value);
    void automaticChanged(bool value);

private slots:
    void on_automatic_toggled(bool set);
    void on_module_valueChanged(int value);
    void on_arg_valueChanged(int value);

private:
    Ui::ComplexFactorGUI* ui;
};

#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_COMPLEXFACTORGUI_H_ */
