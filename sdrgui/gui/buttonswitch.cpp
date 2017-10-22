#include <QPainter>
#include "gui/buttonswitch.h"

ButtonSwitch::ButtonSwitch(QWidget* parent) :
	QToolButton(parent)
{
	setCheckable(true);
	m_originalPalette = palette();
	connect(this, SIGNAL(toggled(bool)), this, SLOT(onToggled(bool)));
}

void ButtonSwitch::onToggled(bool checked)
{
	if(checked) {
		QPalette p = m_originalPalette;
		p.setColor(QPalette::Button, QColor(0x80, 0x46, 0x00));
		setPalette(p);
	} else {
		setPalette(m_originalPalette);
	}
}

void ButtonSwitch::doToggle(bool checked)
{
    onToggled(checked);
}
