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

#ifndef GUI_ACCESSIBLEVALUEDIAL_H
#define GUI_ACCESSIBLEVALUEDIAL_H

#include <QAccessibleWidget>

#include "gui/valuedial.h"

class SDRGUI_API AccessibleValueDial : public QAccessibleWidget, public QAccessibleValueInterface {
public:
    AccessibleValueDial(ValueDial *valueDial) :
        QAccessibleWidget(valueDial)
    {
        addControllingSignal(QLatin1String("changed(quint64)"));
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
            return valueDial()->toolTip();  // Use tooltip until accessibleName field is set to something in .ui files
        case QAccessible::Value:
            return QString::number(valueDial()->getValueNew());
        default:
            return QAccessibleWidget::text(t);
        }
    }

    static QAccessibleInterface* factory(const QString &classname, QObject *object)
    {
        QAccessibleInterface *iface = nullptr;

        if (classname == QLatin1String("ValueDial") && object && object->isWidgetType()) {
            iface = static_cast<QAccessibleInterface*>(new AccessibleValueDial(static_cast<ValueDial *>(object)));
        }

        return iface;
    }

    // QAccessibleValueInterface

    QVariant currentValue() const override
    {
        return valueDial()->getValueNew();
    }

    void setCurrentValue(const QVariant &value) override
    {
        valueDial()->setValue(value.toInt());
    }

    QVariant maximumValue() const override
    {
        return valueDial()->m_valueMax;
    }

    QVariant minimumValue() const override
    {
        return valueDial()->m_valueMin;
    }

    QVariant minimumStepSize() const override
    {
        return 1;
    }

protected:

    ValueDial *valueDial() const
    {
        return static_cast<ValueDial*>(object());
    }
};

#endif /* GUI_ACCESSIBLEVALUEDIAL_H */
