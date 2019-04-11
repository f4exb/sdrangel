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

#ifndef SDRGUI_SOAPYGUI_STRINGRANGEGUI_H_
#define SDRGUI_SOAPYGUI_STRINGRANGEGUI_H_

#include <QWidget>

#include "export.h"

namespace Ui {
    class DiscreteRangeGUI;
}

class SDRGUI_API StringRangeGUI : public QWidget
{
    Q_OBJECT
public:
    explicit StringRangeGUI(QWidget* parent = 0);
    virtual ~StringRangeGUI();

    void setLabel(const QString& text);
    void setUnits(const QString& units);
    void addItem(const QString& itemStr, const std::string& itemValue);
    const std::string& getCurrentValue();
    void setValue(const std::string& value);

signals:
    void valueChanged();

private slots:
    void on_rangeCombo_currentIndexChanged(int index);

private:
    Ui::DiscreteRangeGUI* ui;
    std::vector<std::string> itemValues;
};



#endif /* SDRGUI_SOAPYGUI_STRINGRANGEGUI_H_ */
