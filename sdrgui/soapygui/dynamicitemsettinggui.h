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

#include <QObject>
#include <QString>

#include "export.h"
#include "itemsettinggui.h"

class SDRGUI_API DynamicItemSettingGUI : public QObject
{
    Q_OBJECT
public:
    DynamicItemSettingGUI(ItemSettingGUI *itemSettingGUI, const QString& name, QObject *parent = 0);
    ~DynamicItemSettingGUI();

    const QString& getName() const { return m_name; }
    double getValue() const { return m_itemSettingGUI->getCurrentValue(); }
    void setValue(double value) { m_itemSettingGUI->setValue(value); }

signals:
    void valueChanged(QString itemName, double value);

private slots:
    void processValueChanged(double value);

private:
    ItemSettingGUI *m_itemSettingGUI;
    QString m_name;
};
