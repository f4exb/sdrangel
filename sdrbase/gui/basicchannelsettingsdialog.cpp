#include <QColorDialog>

#include "dsp/channelmarker.h"

#include "basicchannelsettingsdialog.h"
#include "ui_basicchannelsettingsdialog.h"

BasicChannelSettingsDialog::BasicChannelSettingsDialog(ChannelMarker* marker, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BasicChannelSettingsDialog),
    m_channelMarker(marker)
{
    ui->setupUi(this);
    ui->title->setText(m_channelMarker->getTitle());
    m_color = m_channelMarker->getColor();
    ui->udpAddress->setText(m_channelMarker->getUDPAddress());
    ui->udpPort->setText(QString("%1").arg(m_channelMarker->getUDPPort()));
    paintColor();
}

BasicChannelSettingsDialog::~BasicChannelSettingsDialog()
{
    delete ui;
}

void BasicChannelSettingsDialog::paintColor()
{
    QPixmap pm(24, 24);
    pm.fill(m_color);
    ui->colorBtn->setIcon(pm);
    ui->colorText->setText(tr("#%1%2%3")
        .arg(m_color.red(), 2, 16, QChar('0'))
        .arg(m_color.green(), 2, 16, QChar('0'))
        .arg(m_color.blue(), 2, 16, QChar('0')));
}

void BasicChannelSettingsDialog::on_colorBtn_clicked()
{
    QColor c = m_color;
    c = QColorDialog::getColor(c, this, tr("Select Color for Channel"));
    if(c.isValid()) {
        m_color = c;
        paintColor();
    }
}

void BasicChannelSettingsDialog::accept()
{
    m_channelMarker->setTitle(ui->title->text());

    if(m_color.isValid()) {
        m_channelMarker->setColor(m_color);
    }

    m_channelMarker->setUDPAddress(ui->udpAddress->text());

    bool ok;
    int udpPort = ui->udpPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024) || (udpPort > 65535))
    {
        udpPort = 9999;
    }

    m_channelMarker->setUDPPort(udpPort);

    QDialog::accept();
}
