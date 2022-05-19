#include <QPainter>
#include "gui/buttonswitch.h"

ButtonSwitch::ButtonSwitch(QWidget* parent) :
	QToolButton(parent)
{
	setCheckable(true);
    setStyleSheet(QString("QToolButton{ background-color: %1; } QToolButton:checked{ background-color: %2; }")
        .arg(palette().button().color().name())
        .arg(palette().highlight().color().darker(150).name()));
}

void ButtonSwitch::doToggle(bool checked)
{
    setChecked(checked);
}

void ButtonSwitch::setColor(QColor color)
{
    setStyleSheet(QString("QToolButton{ background-color: %1; }").arg(color.name()));
}

void ButtonSwitch::resetColor()
{
    setStyleSheet(QString("QToolButton{ background-color: %1; } QToolButton:checked{ background-color: %2; }")
        .arg(palette().button().color().name())
        .arg(palette().highlight().color().darker(150).name()));
}
