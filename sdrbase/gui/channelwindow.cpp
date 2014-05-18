#include <QBoxLayout>
#include <QSpacerItem>
#include <QPainter>
#include <QResizeEvent>
#include "gui/channelwindow.h"
#include "gui/rollupwidget.h"

ChannelWindow::ChannelWindow(QWidget* parent) :
	QScrollArea(parent)
{
	m_container = new QWidget(this);
	m_layout = new QBoxLayout(QBoxLayout::TopToBottom, m_container);
	setWidget(m_container);
	setWidgetResizable(true);
	setBackgroundRole(QPalette::Base);
	m_layout->setMargin(3);
	m_layout->setSpacing(3);
}

void ChannelWindow::addRollupWidget(QWidget* rollupWidget)
{
	rollupWidget->setParent(m_container);
	m_container->layout()->addWidget(rollupWidget);
}

void ChannelWindow::resizeEvent(QResizeEvent* event)
{
	if(event->size().height() > event->size().width()) {
		m_layout->setDirection(QBoxLayout::TopToBottom);
		m_layout->setAlignment(Qt::AlignTop);
	} else {
		m_layout->setDirection(QBoxLayout::LeftToRight);
		m_layout->setAlignment(Qt::AlignLeft);
	}
	QScrollArea::resizeEvent(event);
}
