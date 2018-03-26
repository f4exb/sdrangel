///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "audioselectdialog.h"
#include "ui_audioselectdialog.h"

AudioSelectDialog::AudioSelectDialog(AudioDeviceManager* audioDeviceManager, const QString& deviceName, bool input, QWidget* parent) :
    QDialog(parent),
    m_selected(false),
    ui(new Ui::AudioSelectDialog),
    m_audioDeviceManager(audioDeviceManager),
    m_input(input)
{
    ui->setupUi(this);
    QTreeWidgetItem *treeItem, *defaultItem, *selectedItem = 0;

    // panel

    QAudioDeviceInfo defaultDeviceInfo = input ? QAudioDeviceInfo::defaultInputDevice() : QAudioDeviceInfo::defaultOutputDevice();
    defaultItem = new QTreeWidgetItem(ui->audioTree);
    defaultItem->setText(0, AudioDeviceManager::m_defaultDeviceName);

    QList<QAudioDeviceInfo> devices = input ? m_audioDeviceManager->getInputDevices() : m_audioDeviceManager->getOutputDevices();

    for(QList<QAudioDeviceInfo>::const_iterator it = devices.begin(); it != devices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioTree);
        treeItem->setText(0, it->deviceName());

        if (it->deviceName() == defaultDeviceInfo.deviceName()) {
            treeItem->setBackground(0, QBrush(qRgb(96,96,96)));
        }

        if (deviceName == it->deviceName()) {
            selectedItem = treeItem;
        }
    }

    if (selectedItem) {
        ui->audioTree->setCurrentItem(selectedItem);
    } else {
        ui->audioTree->setCurrentItem(defaultItem);
    }
}

AudioSelectDialog::~AudioSelectDialog()
{
    delete ui;
}

void AudioSelectDialog::accept()
{
    int deviceIndex = ui->audioTree->indexOfTopLevelItem(ui->audioTree->currentItem()) - 1;

    if (m_input)
    {
        if (!m_audioDeviceManager->getInputDeviceName(deviceIndex, m_audioDeviceName)) {
            m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
        }
    }
    else
    {
        if (!m_audioDeviceManager->getOutputDeviceName(deviceIndex, m_audioDeviceName)) {
            m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
        }

        qDebug("AudioSelectDialog::accept: output: %d (%s)", deviceIndex, qPrintable(m_audioDeviceName));
    }

    m_selected = true;
    QDialog::accept();
}

void AudioSelectDialog::reject()
{
    QDialog::reject();
}



