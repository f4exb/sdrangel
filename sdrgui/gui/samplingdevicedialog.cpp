///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#include "samplingdevicedialog.h"
#include "ui_samplingdevicedialog.h"
#include "device/deviceenumerator.h"
#include "maincore.h"

SamplingDeviceDialog::SamplingDeviceDialog(int deviceType, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SamplingDeviceDialog),
    m_deviceType(deviceType),
    m_selectedDeviceIndex(-1),
    m_hasChanged(false)
{
    ui->setupUi(this);
    on_refreshDevices_clicked();
}

SamplingDeviceDialog::~SamplingDeviceDialog()
{
    delete ui;
}

int SamplingDeviceDialog::exec()
{
    m_hasChanged = false;
    return QDialog::exec();
}

void SamplingDeviceDialog::displayDevices()
{
    QList<QString> deviceDisplayNames;

    m_deviceIndexes.clear();
    if (m_deviceType == 0) { // Single Rx
        DeviceEnumerator::instance()->listRxDeviceNames(deviceDisplayNames, m_deviceIndexes);
    } else if (m_deviceType == 1) { // Single Tx
        DeviceEnumerator::instance()->listTxDeviceNames(deviceDisplayNames, m_deviceIndexes);
    } else if (m_deviceType == 2) { // MIMO
        DeviceEnumerator::instance()->listMIMODeviceNames(deviceDisplayNames, m_deviceIndexes);
    }

    ui->deviceSelect->clear();
    ui->deviceSelect->addItems(deviceDisplayNames);
}

void SamplingDeviceDialog::setSelectedDeviceIndex(int deviceIndex)
{
    ui->deviceSelect->blockSignals(true);
    ui->deviceSelect->setCurrentIndex(deviceIndex);
    m_selectedDeviceIndex = deviceIndex;
    ui->deviceSelect->blockSignals(false);
}

void SamplingDeviceDialog::getDeviceId(QString& id) const
{
    id  = ui->deviceSelect->currentText();
}

void SamplingDeviceDialog::on_deviceSelect_currentIndexChanged(int index)
{
    (void) index;
    m_hasChanged = true;
}

void SamplingDeviceDialog::on_refreshDevices_clicked()
{
    PluginManager *pluginManager = MainCore::instance()->getPluginManager();

    if (m_deviceType == 0) {
        DeviceEnumerator::instance()->enumerateRxDevices(pluginManager);
    } else if (m_deviceType == 1) {
        DeviceEnumerator::instance()->enumerateTxDevices(pluginManager);
    } else if (m_deviceType == 2) {
        DeviceEnumerator::instance()->enumerateMIMODevices(pluginManager);
    }

    displayDevices();
}

void SamplingDeviceDialog::accept()
{
    m_selectedDeviceIndex = m_deviceIndexes[ui->deviceSelect->currentIndex()];
    QDialog::accept();
}

void SamplingDeviceDialog::reject()
{
    m_hasChanged = false;
    QDialog::reject();
}
