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

#include "remotecontrolvisacontroldialog.h"

#include <QDebug>
#include <QPushButton>

RemoteControlVISAControlDialog::RemoteControlVISAControlDialog(RemoteControlSettings *settings, RemoteControlDevice *device, VISADevice::VISAControl *control, bool add, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::RemoteControlVISAControlDialog),
    m_settings(settings),
    m_device(device),
    m_control(control),
    m_add(add),
    m_userHasEditedId(false)
{
    ui->setupUi(this);
    ui->name->setText(m_control->m_name);
    ui->id->setText(m_control->m_id);
    DeviceDiscoverer::Type type = m_control->m_type == DeviceDiscoverer::AUTO ? DeviceDiscoverer::BOOL : m_control->m_type;
    ui->type->setCurrentText(DeviceDiscoverer::m_typeStrings[(int)type]);
    ui->widgetType->setCurrentText(DeviceDiscoverer::m_widgetTypeStrings[(int)m_control->m_widgetType]);
    ui->min->setValue(m_control->m_min);
    ui->max->setValue(m_control->m_max);
    ui->scale->setValue(m_control->m_scale);
    ui->precision->setValue(m_control->m_precision);
    ui->values->insertItems(0, m_control->m_values);
    if (m_control->m_values.size() > 0) {
        ui->label->setText(m_control->m_values[0]);
    }
    ui->units->setText(m_control->m_units);
    ui->setState->setPlainText(m_control->m_setState);
    ui->getState->setPlainText(m_control->m_getState);
    on_type_currentIndexChanged(ui->type->currentIndex());
    validate();
}

RemoteControlVISAControlDialog::~RemoteControlVISAControlDialog()
{
    delete ui;
}

void RemoteControlVISAControlDialog::accept()
{
    QDialog::accept();

    m_control->m_name = ui->name->text();
    m_control->m_id = ui->id->text();
    m_control->m_type = (DeviceDiscoverer::Type)DeviceDiscoverer::m_typeStrings.indexOf(ui->type->currentText());
    m_control->m_widgetType = (DeviceDiscoverer::WidgetType)DeviceDiscoverer::m_widgetTypeStrings.indexOf(ui->widgetType->currentText());
    m_control->m_min = ui->min->value();
    m_control->m_max = ui->max->value();
    m_control->m_scale = ui->scale->value();
    m_control->m_precision = ui->precision->value();
    m_control->m_values.clear();
    if (m_control->m_type == DeviceDiscoverer::BUTTON)
    {
        m_control->m_values.append(ui->label->text());
    }
    else
    {
        for (int i = 0; i < ui->values->count(); i++) {
            m_control->m_values.append(ui->values->itemText(i));
        }
    }
    m_control->m_units = ui->units->text();
    m_control->m_setState = ui->setState->toPlainText();
    m_control->m_getState = ui->getState->toPlainText();
}

void RemoteControlVISAControlDialog::on_type_currentIndexChanged(int index)
{
    DeviceDiscoverer::Type type;
    if (index < 0) {
        type = DeviceDiscoverer::BOOL;
    } else {
        type = (DeviceDiscoverer::Type)DeviceDiscoverer::m_typeStrings.indexOf(ui->type->currentText());
    }
    bool minMaxVisible = true;    // Default to FLOAT
    bool precisionVisible = true;
    bool listVisible = false;
    bool labelVisible = false;
    int decimals = 3;
    if (type == DeviceDiscoverer::BOOL)
    {
        minMaxVisible = false;
        precisionVisible = false;
    }
    else if (type == DeviceDiscoverer::INT)
    {
        decimals = 0;
        precisionVisible = false;
    }
    else if (type == DeviceDiscoverer::STRING)
    {
        minMaxVisible = false;
        precisionVisible = false;
    }
    else if (type == DeviceDiscoverer::LIST)
    {
        minMaxVisible = false;
        precisionVisible = false;
        listVisible = true;
    }
    else if (type == DeviceDiscoverer::BUTTON)
    {
        minMaxVisible = false;
        precisionVisible = false;
        labelVisible = true;
    }
    ui->widgetType->setVisible(precisionVisible);
    ui->minLabel->setVisible(minMaxVisible);
    ui->min->setVisible(minMaxVisible);
    ui->min->setDecimals(decimals);
    ui->maxLabel->setVisible(minMaxVisible);
    ui->max->setVisible(minMaxVisible);
    ui->max->setDecimals(decimals);
    ui->scaleLabel->setVisible(precisionVisible);
    ui->scale->setVisible(precisionVisible);
    ui->precisionLabel->setVisible(precisionVisible);
    ui->precision->setVisible(precisionVisible);
    ui->values->setVisible(listVisible);
    ui->remove->setVisible(listVisible);
    ui->labelLabel->setVisible(labelVisible);
    ui->label->setVisible(labelVisible);

    bool getStateEnabled = type != DeviceDiscoverer::BUTTON;
    ui->getStateLabel->setEnabled(getStateEnabled);
    ui->getState->setEnabled(getStateEnabled);
}

void RemoteControlVISAControlDialog::on_name_textChanged(const QString &text)
{
    if (m_add && !m_userHasEditedId)
    {
        // Set Id to lower case version of name
        ui->id->setText(text.trimmed().toLower().replace(" ", ""));
    }
}

void RemoteControlVISAControlDialog::on_id_textChanged(const QString &text)
{
    (void)text;
    validate();
}

void RemoteControlVISAControlDialog::on_id_textEdited(const QString &text)
{
    (void)text;
    m_userHasEditedId = true;
}

void RemoteControlVISAControlDialog::on_setState_textChanged()
{
    validate();
}

void RemoteControlVISAControlDialog::on_remove_clicked()
{
    ui->values->removeItem(ui->values->currentIndex());
}

void RemoteControlVISAControlDialog::validate()
{
    bool valid = true;

    QString id = ui->id->text().trimmed();
    if (id.isEmpty()) {
        valid = false;
    } else if (m_add) {
        if (m_device->getControl(id)) {
            valid = false;
        }
    }
    if (ui->setState->toPlainText().trimmed().isEmpty()) {
        valid = false;
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}
