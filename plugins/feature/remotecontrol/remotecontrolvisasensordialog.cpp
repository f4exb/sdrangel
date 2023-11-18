///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon <jon@beniston.com>                                     //
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

#include "remotecontrolvisasensordialog.h"

#include <QDebug>
#include <QPushButton>

RemoteControlVISASensorDialog::RemoteControlVISASensorDialog(RemoteControlSettings *settings, RemoteControlDevice *device, VISADevice::VISASensor *sensor, bool add, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::RemoteControlVISASensorDialog),
    m_settings(settings),
    m_device(device),
    m_sensor(sensor),
    m_add(add),
    m_userHasEditedId(false)
{
    ui->setupUi(this);
    ui->name->setText(m_sensor->m_name);
    ui->id->setText(m_sensor->m_id);
    ui->type->setCurrentText(DeviceDiscoverer::m_typeStrings[(int)m_sensor->m_type]);
    ui->units->setText(m_sensor->m_units);
    ui->getState->setPlainText(m_sensor->m_getState);
    validate();
}

RemoteControlVISASensorDialog::~RemoteControlVISASensorDialog()
{
    delete ui;
}

void RemoteControlVISASensorDialog::accept()
{
    QDialog::accept();

    m_sensor->m_name = ui->name->text();
    m_sensor->m_id = ui->id->text();
    m_sensor->m_type = (DeviceDiscoverer::Type)DeviceDiscoverer::m_typeStrings.indexOf(ui->type->currentText());
    m_sensor->m_units = ui->units->text();
    m_sensor->m_getState = ui->getState->toPlainText();
}

void RemoteControlVISASensorDialog::on_name_textChanged(const QString &text)
{
    if (m_add && !m_userHasEditedId)
    {
        // Set Id to lower case version of name
        ui->id->setText(text.trimmed().toLower().replace(" ", ""));
    }
}

void RemoteControlVISASensorDialog::on_id_textChanged(const QString &text)
{
    (void)text;
    validate();
}

void RemoteControlVISASensorDialog::on_id_textEdited(const QString &text)
{
    (void)text;
    m_userHasEditedId = true;
}

void RemoteControlVISASensorDialog::on_getState_textChanged()
{
    validate();
}

void RemoteControlVISASensorDialog::validate()
{
    bool valid = true;

    QString id = ui->id->text().trimmed();
    if (id.isEmpty()) {
        valid = false;
    } else if (m_add) {
        if (m_device->getSensor(id)) {
            valid = false;
        }
    }
    if (ui->getState->toPlainText().trimmed().isEmpty()) {
        valid = false;
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}
