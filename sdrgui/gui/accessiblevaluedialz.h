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

#ifndef GUI_ACCESSIBLEVALUEDIALZ_H
#define GUI_ACCESSIBLEVALUEDIALZ_H

#include <QAccessibleWidget>

#include "gui/valuedialz.h"

class SDRGUI_API AccessibleValueDialZ : public QAccessibleWidget, public QAccessibleValueInterface {
public:
    AccessibleValueDialZ(ValueDialZ *valueDialZ) :
        QAccessibleWidget(valueDialZ)
    {
        addControllingSignal(QLatin1String("changed(qint64)"));
    }

    void *interface_cast(QAccessible::InterfaceType t) override
    {
        if (t == QAccessible::ValueInterface)
            return static_cast<QAccessibleValueInterface*>(this);
        return QAccessibleWidget::interface_cast(t);
    }

    QAccessible::Role role() const override
    {
        //return QAccessible::Dial; // This results in reader saying "custom" and not reading the value
        return QAccessible::Slider;
    }

    QString text(QAccessible::Text t) const override
    {
        switch (t)
        {
        case QAccessible::Name:
            return valueDialZ()->toolTip();  // Use tooltip until accessibleName field is set to something in .ui files
        case QAccessible::Value:
            return QString::number(valueDialZ()->getValueNew());
        default:
            return QAccessibleWidget::text(t);
        }
    }

    static QAccessibleInterface* factory(const QString &classname, QObject *object)
    {
        QAccessibleInterface *iface = nullptr;

        if (classname == QLatin1String("ValueDialZ") && object && object->isWidgetType()) {
            iface = static_cast<QAccessibleInterface*>(new AccessibleValueDialZ(static_cast<ValueDialZ *>(object)));
        }

        return iface;
    }

    // QAccessibleValueInterface

    QVariant currentValue() const override
    {
        return valueDialZ()->getValueNew();
    }

    void setCurrentValue(const QVariant &value) override
    {
        valueDialZ()->setValue(value.toInt());
    }

    QVariant maximumValue() const override
    {
        return valueDialZ()->m_valueMax;
    }

    QVariant minimumValue() const override
    {
        return valueDialZ()->m_valueMin;
    }

    QVariant minimumStepSize() const override
    {
        return 1;
    }

protected:

    ValueDialZ *valueDialZ() const
    {
        return static_cast<ValueDialZ*>(object());
    }
};

#endif /* GUI_ACCESSIBLEVALUEDIALZ_H */
