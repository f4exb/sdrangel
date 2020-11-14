///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QString>
#include <QListWidgetItem>

#include "ambedevicesdialog.h"
#include "ui_ambedevicesdialog.h"

AMBEDevicesDialog::AMBEDevicesDialog(AMBEEngine* ambeEngine, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::AMBEDevicesDialog),
    m_ambeEngine(ambeEngine)
{
    ui->setupUi(this);
    populateSerialList();
    refreshInUseList();
}

AMBEDevicesDialog::~AMBEDevicesDialog()
{
    delete ui;
}

void AMBEDevicesDialog::populateSerialList()
{
    std::vector<QString> ambeSerialDevices;
    m_ambeEngine->scan(ambeSerialDevices);
    ui->ambeSerialDevices->clear();
    std::vector<QString>::const_iterator it = ambeSerialDevices.begin();

    for (; it != ambeSerialDevices.end(); ++it)
    {
        ui->ambeSerialDevices->addItem(QString(*it));
    }
}

void AMBEDevicesDialog::refreshInUseList()
{
    std::vector<QString> inUseDevices;
    m_ambeEngine->getDeviceRefs(inUseDevices);
    ui->ambeDeviceRefs->clear();
    std::vector<QString>::const_iterator it = inUseDevices.begin();

    for (; it != inUseDevices.end(); ++it)
    {
        qDebug("AMBEDevicesDialog::refreshInUseList: %s", qPrintable(*it));
        ui->ambeDeviceRefs->addItem(*it);
    }
}

void AMBEDevicesDialog::on_importSerial_clicked()
{
    QListWidgetItem *serialItem = ui->ambeSerialDevices->currentItem();

    if (!serialItem)
    {
        ui->statusText->setText("No selection");
        return;
    }

    QString serialName = serialItem->text();
    QList<QListWidgetItem*> foundItems = ui->ambeDeviceRefs->findItems(serialName, Qt::MatchFixedString|Qt::MatchCaseSensitive);

    if (foundItems.size() == 0)
    {
        if (m_ambeEngine->registerController(serialName.toStdString()))
        {
            ui->ambeDeviceRefs->addItem(serialName);
            ui->statusText->setText(tr("%1 added").arg(serialName));
        }
        else
        {
            ui->statusText->setText(tr("Cannot open %1").arg(serialName));
        }
    }
    else
    {
        ui->statusText->setText("Device already in use");
    }
}

void AMBEDevicesDialog::on_importAllSerial_clicked()
{
    int count = 0;

    for (int i = 0; i < ui->ambeSerialDevices->count(); i++)
    {
        const QListWidgetItem *serialItem = ui->ambeSerialDevices->item(i);
        QString serialName = serialItem->text();
        QList<QListWidgetItem*> foundItems = ui->ambeDeviceRefs->findItems(serialName, Qt::MatchFixedString|Qt::MatchCaseSensitive);

        if (foundItems.size() == 0)
        {
            if (m_ambeEngine->registerController(serialName.toStdString()))
            {
                ui->ambeDeviceRefs->addItem(serialName);
                count++;
            }
        }
    }

    ui->statusText->setText(tr("%1 devices added").arg(count));
}

void AMBEDevicesDialog::on_removeAmbeDevice_clicked()
{
    QListWidgetItem *deviceItem = ui->ambeDeviceRefs->currentItem();

    if (!deviceItem)
    {
        ui->statusText->setText("No selection");
        return;
    }

    QString deviceName = deviceItem->text();
    m_ambeEngine->releaseController(deviceName.toStdString());
    ui->statusText->setText(tr("%1 removed").arg(deviceName));
    refreshInUseList();
}

void AMBEDevicesDialog::on_refreshAmbeList_clicked()
{
    refreshInUseList();
    ui->statusText->setText("In use refreshed");
}

void AMBEDevicesDialog::on_clearAmbeList_clicked()
{
    if (ui->ambeDeviceRefs->count() == 0)
    {
        ui->statusText->setText("No active items");
        return;
    }

    m_ambeEngine->releaseAll();
    ui->ambeDeviceRefs->clear();
    ui->statusText->setText("All items released");
}

void AMBEDevicesDialog::on_importAddress_clicked()
{
    QString addressAndPort = ui->ambeAddressText->text();

    QList<QListWidgetItem*> foundItems = ui->ambeDeviceRefs->findItems(addressAndPort, Qt::MatchFixedString|Qt::MatchCaseSensitive);

    if (foundItems.size() == 0)
    {
        if (m_ambeEngine->registerController(addressAndPort.toStdString()))
        {
            ui->ambeDeviceRefs->addItem(addressAndPort);
            ui->statusText->setText(tr("%1 added").arg(addressAndPort));
        }
        else
        {
            ui->statusText->setText(tr("Cannot open %1").arg(addressAndPort));
        }
    }
    else
    {
        ui->statusText->setText("Address already in use");
    }
}
