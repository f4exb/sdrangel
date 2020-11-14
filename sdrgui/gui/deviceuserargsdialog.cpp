///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "device/deviceenumerator.h"
#include "ui_deviceuserargsdialog.h"
#include "deviceuserargsdialog.h"

DeviceUserArgsDialog::DeviceUserArgsDialog(
	DeviceEnumerator* deviceEnumerator,
	DeviceUserArgs& hardwareDeviceUserArgs,
	QWidget* parent
) :
	QDialog(parent),
	ui(new Ui::DeviceUserArgsDialog),
	m_deviceEnumerator(deviceEnumerator),
	m_hardwareDeviceUserArgs(hardwareDeviceUserArgs),
    m_deviceUserArgsCopy(hardwareDeviceUserArgs),
    m_xDeviceSequence(0)
{
	ui->setupUi(this);

	for (int i = 0; i < m_deviceEnumerator->getNbRxSamplingDevices(); i++) {
		pushHWDeviceReference(m_deviceEnumerator->getRxSamplingDevice(i));
	}

	for (int i = 0; i < m_deviceEnumerator->getNbTxSamplingDevices(); i++) {
		pushHWDeviceReference(m_deviceEnumerator->getTxSamplingDevice(i));
	}

	for (int i = 0; i < m_deviceEnumerator->getNbMIMOSamplingDevices(); i++) {
		pushHWDeviceReference(m_deviceEnumerator->getMIMOSamplingDevice(i));
	}

	for (auto& hwItem : m_availableHWDevices)
	{
		QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->deviceTree);
		treeItem->setText(0, hwItem.m_hardwareId);
		treeItem->setText(1, tr("%1").arg(hwItem.m_sequence));
		treeItem->setText(2, hwItem.m_description);
	}

    ui->deviceTree->resizeColumnToContents(0);
    ui->deviceTree->resizeColumnToContents(1);
    ui->deviceTree->resizeColumnToContents(2);

    displayArgsByDevice();

    ui->addDeviceHwIDEdit->setText(m_xDeviceHardwareId);
    ui->addDeviceSeqEdit->setText(tr("%1").arg(m_xDeviceSequence));
}

DeviceUserArgsDialog::~DeviceUserArgsDialog()
{
    delete ui;
}

void DeviceUserArgsDialog::displayArgsByDevice()
{
	ui->argsTree->blockSignals(true);
    ui->argsTree->clear();
    ui->argStringEdit->clear();

    QList<DeviceUserArgs::Args>::const_iterator it = m_deviceUserArgsCopy.getArgsByDevice().begin();

    for (; it != m_deviceUserArgsCopy.getArgsByDevice().end(); ++it)
	{
		QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->argsTree);
        treeItem->setText(0, it->m_nonDiscoverable ? "ND" : "  ");
		treeItem->setText(1, it->m_id);
		treeItem->setText(2, tr("%1").arg(it->m_sequence));
		treeItem->setText(3, it->m_args);
	}

    ui->argsTree->resizeColumnToContents(0);
    ui->argsTree->resizeColumnToContents(1);
    ui->argsTree->resizeColumnToContents(2);
    ui->argsTree->resizeColumnToContents(3);
	ui->argsTree->blockSignals(false);
}

void DeviceUserArgsDialog::pushHWDeviceReference(const PluginInterface::SamplingDevice *samplingDevice)
{
	HWDeviceReference hw;
	hw.m_hardwareId = samplingDevice->hardwareId;
	hw.m_sequence = samplingDevice->sequence;
	hw.m_description = samplingDevice->displayedName;
	bool found = false;

	for (auto& hwAvail : m_availableHWDevices)
	{
		if (hw == hwAvail)
		{
			found = true;
			break;
		}
	}

	if (!found) {
		m_availableHWDevices.push_back(hw);
	}
}

void DeviceUserArgsDialog::accept()
{
    m_hardwareDeviceUserArgs = m_deviceUserArgsCopy;
    QDialog::accept();
}

void DeviceUserArgsDialog::reject()
{
    QDialog::reject();
}

void DeviceUserArgsDialog::on_importDevice_clicked()
{
    QTreeWidgetItem *deviceItem = ui->deviceTree->currentItem();

    if (deviceItem)
    {
        bool ok;
        int sequence = deviceItem->text(1).toInt(&ok);
        m_deviceUserArgsCopy.addDeviceArgs(deviceItem->text(0), sequence, "", false);
        displayArgsByDevice();
    }
}

void DeviceUserArgsDialog::on_deleteArgs_clicked()
{
    QTreeWidgetItem *deviceItem = ui->argsTree->currentItem();

    if (deviceItem)
    {
        bool ok;
        int sequence = deviceItem->text(2).toInt(&ok);
        m_deviceUserArgsCopy.deleteDeviceArgs(deviceItem->text(1), sequence);
        displayArgsByDevice();
    }
}

void DeviceUserArgsDialog::on_argsTree_currentItemChanged(QTreeWidgetItem* currentItem, QTreeWidgetItem* previousItem)
{
	(void) previousItem;
	ui->argStringEdit->setText(currentItem->text(3));
}

void DeviceUserArgsDialog::on_argStringEdit_editingFinished()
{
	QTreeWidgetItem *deviceItem = ui->argsTree->currentItem();

    if (deviceItem)
    {
        bool ok;
        int sequence = deviceItem->text(2).toInt(&ok);
        bool nonDiscoverable = deviceItem->text(0) == "ND";
        m_deviceUserArgsCopy.updateDeviceArgs(deviceItem->text(1), sequence, ui->argStringEdit->text(), nonDiscoverable);
        displayArgsByDevice();
    }
}

void DeviceUserArgsDialog::on_addDeviceHwIDEdit_editingFinished()
{
    m_xDeviceHardwareId = ui->addDeviceHwIDEdit->text();
}

void DeviceUserArgsDialog::on_addDeviceSeqEdit_editingFinished()
{
    bool ok;
    int sequence = ui->addDeviceSeqEdit->text().toInt(&ok);

    if (ok) {
        m_xDeviceSequence = sequence;
    }
}

void DeviceUserArgsDialog::on_addDevice_clicked()
{
    m_deviceUserArgsCopy.addDeviceArgs(m_xDeviceHardwareId, m_xDeviceSequence, "", true);
    displayArgsByDevice();
}
