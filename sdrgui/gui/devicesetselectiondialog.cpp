///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
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

#include "device/deviceuiset.h"
#include "device/devicegui.h"
#include "device/deviceapi.h"
#include "devicesetselectiondialog.h"

#include "ui_workspaceselectiondialog.h"

DeviceSetSelectionDialog::DeviceSetSelectionDialog(std::vector<DeviceUISet*>& deviceUIs, int channelDeviceSetIndex, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WorkspaceSelectionDialog),
    m_deviceUIs(deviceUIs),
    m_channelDeviceSetIndex(channelDeviceSetIndex),
    m_hasChanged(false)
{
    ui->setupUi(this);
    setWindowTitle("Device"); // Change window title

    DeviceUISet *originDeviceUISet = deviceUIs[channelDeviceSetIndex];
    int originDeviceType = (int) originDeviceUISet->m_deviceGUI->getDeviceType();

    for (int i = 0; i < (int) m_deviceUIs.size(); i++)
    {
        DeviceUISet *deviceUISet = m_deviceUIs[i];

        if ((int) deviceUISet->m_deviceGUI->getDeviceType() == originDeviceType)
        {
            ui->workspaceList->addItem(tr("%1:%2 %3")
                .arg(getDeviceTypeChar(originDeviceType))
                .arg(i)
                .arg(deviceUISet->m_deviceAPI->getSamplingDeviceDisplayName().split(" ")[0])
            );
            m_deviceSetIndexes.push_back(i);
        }
    }
}

DeviceSetSelectionDialog::~DeviceSetSelectionDialog()
{
    delete ui;
}

void DeviceSetSelectionDialog::accept()
{
    m_selectedDeviceSetIndex = m_deviceSetIndexes[ui->workspaceList->currentRow()];
    m_hasChanged = true;
    QDialog::accept();
}
