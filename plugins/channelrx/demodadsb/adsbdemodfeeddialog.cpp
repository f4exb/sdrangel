///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QLineEdit>

#include "adsbdemodfeeddialog.h"
#include "adsbdemodsettings.h"

ADSBDemodFeedDialog::ADSBDemodFeedDialog(QString& feedHost, int feedPort, ADSBDemodSettings::FeedFormat feedFormat, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ADSBDemodFeedDialog)
{
    ui->setupUi(this);
    ui->host->lineEdit()->setText(feedHost);
    ui->port->setValue(feedPort);
    ui->format->setCurrentIndex((int)feedFormat);
}

ADSBDemodFeedDialog::~ADSBDemodFeedDialog()
{
    delete ui;
}

void ADSBDemodFeedDialog::accept()
{
    m_feedHost = ui->host->currentText();
    m_feedPort = ui->port->value();
    m_feedFormat = (ADSBDemodSettings::FeedFormat )ui->format->currentIndex();
    QDialog::accept();
}

void ADSBDemodFeedDialog::on_host_currentIndexChanged(int value)
{
    if (value == 0)
    {
        ui->host->lineEdit()->setText("feed.adsbexchange.com");
        ui->port->setValue(30005);
        ui->format->setCurrentIndex(0);
    }
    else if (value == 1)
    {
        ui->host->lineEdit()->setText("data.adsbhub.org");
        ui->port->setValue(5002);
        ui->format->setCurrentIndex(1);
    }
}
