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
#include "device/deviceuserargs.h"
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
	m_hardwareDeviceUserArgs(hardwareDeviceUserArgs)
{
	qDebug("DeviceUserArgsDialog::DeviceUserArgsDialog");
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
		qDebug("DeviceUserArgsDialog::DeviceUserArgsDialog: available %s:%d", qPrintable(hwItem.m_hardwareId), hwItem.m_sequence);
	}

    ui->deviceTree->resizeColumnToContents(0);
    ui->deviceTree->resizeColumnToContents(1);
    ui->deviceTree->resizeColumnToContents(2);

	for (auto& deviceKey : hardwareDeviceUserArgs.m_argsByDevice)
	{
		QString hardwareId;
		int sequence;
		DeviceUserArgs::splitDeviceKey(deviceKey, hardwareId, sequence);
		QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->argsTree);
		treeItem->setText(0, hardwareId);
		treeItem->setText(1, tr("%1").arg(sequence));
		treeItem->setText(2, hardwareDeviceUserArgs.m_argsByDevice.value(deviceKey));
		qDebug("DeviceUserArgsDialog::DeviceUserArgsDialog: args %s:%d", qPrintable(hardwareId), sequence);
	}

    ui->argsTree->resizeColumnToContents(0);
    ui->argsTree->resizeColumnToContents(1);
    ui->argsTree->resizeColumnToContents(2);
}

DeviceUserArgsDialog::~DeviceUserArgsDialog()
{
    delete ui;
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
    QDialog::accept();
}

void DeviceUserArgsDialog::reject()
{
    QDialog::reject();
}

void DeviceUserArgsDialog::on_importDevice_clicked(bool checked)
{

}

void DeviceUserArgsDialog::on_deleteArgs_clicked(bool checked)
{

}

void DeviceUserArgsDialog::on_argStringEdit_returnPressed()
{

}