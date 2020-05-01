///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "wsspectrumsettingsdialog.h"
#include "ui_wsspectrumsettingsdialog.h"

WebsocketSpectrumSettingsDialog::WebsocketSpectrumSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WebsocketSpectrumSettingsDialog),
    m_hasChanged(false)
{
    ui->setupUi(this);
    setAddress("127.0.0.1");
    setPort(8887);
}

WebsocketSpectrumSettingsDialog::~WebsocketSpectrumSettingsDialog()
{
    delete ui;
}

void WebsocketSpectrumSettingsDialog::setAddress(const QString& address)
{
    m_address = address;
    ui->address->setText(m_address);
}

void WebsocketSpectrumSettingsDialog::setPort(uint16_t port)
{
    if (port < 1024) {
        return;
    } else {
        m_port = port;
    }

    ui->port->setText(tr("%1").arg(m_port));
}

void WebsocketSpectrumSettingsDialog::on_address_editingFinished()
{
    m_address = ui->address->text();
}

void WebsocketSpectrumSettingsDialog::on_port_editingFinished()
{
    bool dataOk;
    int port = ui->port->text().toInt(&dataOk);

    if ((!dataOk) || (port < 1024) || (port > 65535)) {
        return;
    } else {
        m_port = port;
    }
}

void WebsocketSpectrumSettingsDialog::accept()
{
    m_hasChanged = true;
    QDialog::accept();
}
