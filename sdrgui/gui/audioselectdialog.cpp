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
    bool systemDefault;
    int sampleRate;

    // panel

    QAudioDeviceInfo defaultDeviceInfo = input ? QAudioDeviceInfo::defaultInputDevice() : QAudioDeviceInfo::defaultOutputDevice();
    defaultItem = new QTreeWidgetItem(ui->audioTree);
    defaultItem->setText(1, AudioDeviceManager::m_defaultDeviceName);
    bool deviceFound = getDeviceInfos(input, AudioDeviceManager::m_defaultDeviceName, systemDefault, sampleRate);
    defaultItem->setText(0, deviceFound ? "__" : "_D");
    defaultItem->setText(2, tr("%1").arg(sampleRate));
    defaultItem->setTextAlignment(2, Qt::AlignRight);

    QList<QAudioDeviceInfo> devices = input ? m_audioDeviceManager->getInputDevices() : m_audioDeviceManager->getOutputDevices();

    for(QList<QAudioDeviceInfo>::const_iterator it = devices.begin(); it != devices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioTree);
        treeItem->setText(1, it->deviceName());
        bool deviceFound = getDeviceInfos(input, it->deviceName(), systemDefault, sampleRate);
        treeItem->setText(0, QString(systemDefault ? "S" : "_") + QString(deviceFound ? "_" : "D"));
        treeItem->setText(2, tr("%1").arg(sampleRate));
        treeItem->setTextAlignment(2, Qt::AlignRight);

        if (systemDefault) {
            treeItem->setBackground(1, QBrush(qRgb(80,80,80)));
        }

        if (deviceName == it->deviceName()) {
            selectedItem = treeItem;
        }
    }

    ui->audioTree->resizeColumnToContents(0);
    ui->audioTree->resizeColumnToContents(1);
    ui->audioTree->resizeColumnToContents(2);

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

bool AudioSelectDialog::getDeviceInfos(bool input, const QString& deviceName, bool& systemDefault, int& sampleRate)
{
    bool found;

    if (input)
    {
        AudioDeviceManager::InputDeviceInfo inDeviceInfo;
        found = m_audioDeviceManager->getInputDeviceInfo(deviceName, inDeviceInfo);
        systemDefault = deviceName == QAudioDeviceInfo::defaultInputDevice().deviceName();

        if (found) {
            sampleRate = inDeviceInfo.sampleRate;
        } else {
            sampleRate = AudioDeviceManager::m_defaultAudioSampleRate;
        }
    }
    else
    {
        AudioDeviceManager::OutputDeviceInfo outDeviceInfo;
        found = m_audioDeviceManager->getOutputDeviceInfo(deviceName, outDeviceInfo);
        systemDefault = deviceName == QAudioDeviceInfo::defaultOutputDevice().deviceName();

        if (found) {
            sampleRate = outDeviceInfo.sampleRate;
        } else {
            sampleRate = AudioDeviceManager::m_defaultAudioSampleRate;
        }
    }

    return found;
}


