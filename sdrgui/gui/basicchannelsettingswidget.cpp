#include <QPainter>
#include <QColorDialog>
#include "gui/basicchannelsettingswidget.h"
#include "dsp/channelmarker.h"
#include "ui_basicchannelsettingswidget.h"

BasicChannelSettingsWidget::BasicChannelSettingsWidget(ChannelMarker* marker, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::BasicChannelSettingsWidget),
	m_channelMarker(marker),
	m_hasChanged(false)
{
	ui->setupUi(this);
	ui->title->setText(m_channelMarker->getTitle());
	ui->address->setText(m_channelMarker->getUDPAddress());
	ui->port->setText(QString("%1").arg(m_channelMarker->getUDPReceivePort()));
	paintColor();
}

BasicChannelSettingsWidget::~BasicChannelSettingsWidget()
{
	delete ui;
}

void BasicChannelSettingsWidget::on_title_textChanged(const QString& text)
{
	m_channelMarker->setTitle(text);
	m_hasChanged = true;
}

void BasicChannelSettingsWidget::on_colorBtn_clicked()
{
	QColor c = m_channelMarker->getColor();
	c = QColorDialog::getColor(c, this, tr("Select Color for Channel"));
	if(c.isValid()) {
		m_channelMarker->setColor(c);
		paintColor();
	    m_hasChanged = true;
	}
}

void BasicChannelSettingsWidget::on_address_textEdited(const QString& arg1)
{
    m_channelMarker->setUDPAddress(arg1);
    m_hasChanged = true;
}

void BasicChannelSettingsWidget::on_port_textEdited(const QString& arg1)
{
    bool ok;
    int udpPort = arg1.toInt(&ok);

    if((!ok) || (udpPort < 1024) || (udpPort > 65535))
    {
        udpPort = 9999;
    }

    m_channelMarker->setUDPReceivePort(udpPort);
    m_hasChanged = true;
}

void BasicChannelSettingsWidget::paintColor()
{
	QColor c(m_channelMarker->getColor());
	QPixmap pm(24, 24);
	pm.fill(c);
	ui->colorBtn->setIcon(pm);
	ui->colorText->setText(tr("#%1%2%3")
		.arg(c.red(), 2, 16, QChar('0'))
		.arg(c.green(), 2, 16, QChar('0'))
		.arg(c.blue(), 2, 16, QChar('0')));
}

void BasicChannelSettingsWidget::setUDPDialogVisible(bool visible)
{
    if (visible)
    {
        ui->udpWidget->show();
    }
    else
    {
        ui->udpWidget->hide();
    }
}
