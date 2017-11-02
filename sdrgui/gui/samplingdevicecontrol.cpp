///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2017 Edouard Griffiths, F4EXB                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "samplingdevicecontrol.h"
#include "samplingdevicedialog.h"
#include "plugin/pluginmanager.h"
#include "device/deviceenumerator.h"
#include "ui_samplingdevicecontrol.h"


SamplingDeviceControl::SamplingDeviceControl(int tabIndex, bool rxElseTx, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SamplingDeviceControl),
    m_pluginManager(0),
    m_deviceTabIndex(tabIndex),
    m_rxElseTx(rxElseTx),
    m_selectedDeviceIndex(-1)
{
    ui->setupUi(this);
    ui->deviceSelectedText->setText("None");
}

SamplingDeviceControl::~SamplingDeviceControl()
{
    delete ui;
}

void SamplingDeviceControl::on_deviceChange_clicked()
{
    SamplingDeviceDialog dialog(m_rxElseTx, m_deviceTabIndex, this);
    dialog.exec();

    if (dialog.getSelectedDeviceIndex() >= 0)
    {
        m_selectedDeviceIndex = dialog.getSelectedDeviceIndex();
        setSelectedDeviceIndex(m_selectedDeviceIndex);
        emit changed();
    }
}

void SamplingDeviceControl::on_deviceReload_clicked()
{
    if (m_selectedDeviceIndex >= 0) {
        emit changed();
    }
}

void SamplingDeviceControl::setSelectedDeviceIndex(int index)
{
    if (m_rxElseTx)
    {
        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(index);
        DeviceEnumerator::instance()->changeRxSelection(m_deviceTabIndex, index);
        ui->deviceSelectedText->setText(samplingDevice.displayedName);
    }
    else
    {
        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(index);
        DeviceEnumerator::instance()->changeTxSelection(m_deviceTabIndex, index);
        ui->deviceSelectedText->setText(samplingDevice.displayedName);
    }

    m_selectedDeviceIndex = index;
}

void SamplingDeviceControl::removeSelectedDeviceIndex()
{
    if (m_rxElseTx)
    {
        DeviceEnumerator::instance()->removeRxSelection(m_deviceTabIndex);
        ui->deviceSelectedText->setText("None");
    }
    else
    {
        DeviceEnumerator::instance()->removeTxSelection(m_deviceTabIndex);
        ui->deviceSelectedText->setText("None");
    }

    m_selectedDeviceIndex = -1;
}

QComboBox *SamplingDeviceControl::getChannelSelector()
{
    return ui->channelSelect;
}

QPushButton *SamplingDeviceControl::getAddChannelButton()
{
    return ui->addChannel;
}
