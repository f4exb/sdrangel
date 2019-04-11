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

// This is an interface to an elementary GUI item used to get/set setting from the GUI

#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_ITEMSETTINGGUI_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_ITEMSETTINGGUI_H_

#include <QWidget>

#include "export.h"

class SDRGUI_API ItemSettingGUI : public QWidget
{
    Q_OBJECT
public:
    explicit ItemSettingGUI(QWidget *parent = 0);
    virtual ~ItemSettingGUI() {}
    virtual double getCurrentValue() = 0;
    virtual void setValue(double value) = 0;

signals:
    void valueChanged(double value);
};



#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_ITEMSETTINGGUI_H_ */
