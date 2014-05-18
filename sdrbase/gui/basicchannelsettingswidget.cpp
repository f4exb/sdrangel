#include <QPainter>
#include <QColorDialog>
#include "gui/basicchannelsettingswidget.h"
#include "dsp/channelmarker.h"
#include "ui_basicchannelsettingswidget.h"

BasicChannelSettingsWidget::BasicChannelSettingsWidget(ChannelMarker* marker, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::BasicChannelSettingsWidget),
	m_channelMarker(marker)
{
	ui->setupUi(this);
	ui->title->setText(m_channelMarker->getTitle());
	paintColor();
	ui->red->setValue(m_channelMarker->getColor().red());
	ui->green->setValue(m_channelMarker->getColor().green());
	ui->blue->setValue(m_channelMarker->getColor().blue());
}

BasicChannelSettingsWidget::~BasicChannelSettingsWidget()
{
	delete ui;
}

void BasicChannelSettingsWidget::on_title_textChanged(const QString& text)
{
	m_channelMarker->setTitle(text);
}

void BasicChannelSettingsWidget::on_colorBtn_clicked()
{
	QColor c = m_channelMarker->getColor();
	c = QColorDialog::getColor(c, this, tr("Select Color for Channel"));
	if(c.isValid()) {
		m_channelMarker->setColor(c);
		paintColor();
		ui->red->setValue(m_channelMarker->getColor().red());
		ui->green->setValue(m_channelMarker->getColor().green());
		ui->blue->setValue(m_channelMarker->getColor().blue());
	}
}

void BasicChannelSettingsWidget::paintColor()
{
	QColor c(m_channelMarker->getColor());
	QPixmap pm(24, 24);
	pm.fill(c);
	ui->colorBtn->setIcon(pm);
	ui->color->setText(tr("#%1%2%3")
		.arg(c.red(), 2, 16, QChar('0'))
		.arg(c.green(), 2, 16, QChar('0'))
		.arg(c.blue(), 2, 16, QChar('0')));
}

void BasicChannelSettingsWidget::on_red_valueChanged(int value)
{
	QColor c(m_channelMarker->getColor());
	c.setRed(value);
	m_channelMarker->setColor(c);
	paintColor();
}

void BasicChannelSettingsWidget::on_green_valueChanged(int value)
{
	QColor c(m_channelMarker->getColor());
	c.setGreen(value);
	m_channelMarker->setColor(c);
	paintColor();
}

void BasicChannelSettingsWidget::on_blue_valueChanged(int value)
{
	QColor c(m_channelMarker->getColor());
	c.setBlue(value);
	m_channelMarker->setColor(c);
	paintColor();
}
