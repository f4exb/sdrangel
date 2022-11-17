///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 F4EXB                                                 //
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

#include <math.h>

#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QFileDialog>

#include "audio/audiodevicemanager.h"
#include "audiodialog.h"
#include "ui_audiodialog.h"

AudioDialogX::AudioDialogX(AudioDeviceManager* audioDeviceManager, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AudioDialog),
	m_audioDeviceManager(audioDeviceManager)
{
	ui->setupUi(this);
	QTreeWidgetItem* treeItem;

	// out panel

	AudioDeviceManager::OutputDeviceInfo outDeviceInfo;
	AudioDeviceInfo defaultOutputDeviceInfo = AudioDeviceInfo::defaultOutputDevice();
	treeItem = new QTreeWidgetItem(ui->audioOutTree);
	treeItem->setText(1, AudioDeviceManager::m_defaultDeviceName);
	bool found = m_audioDeviceManager->getOutputDeviceInfo(AudioDeviceManager::m_defaultDeviceName, outDeviceInfo);
	treeItem->setText(0, found ? "__" : "_D");
	ui->audioOutTree->setCurrentItem(treeItem);

	const QList<AudioDeviceInfo>& outputDevices = m_audioDeviceManager->getOutputDevices();

    for(QList<AudioDeviceInfo>::const_iterator it = outputDevices.begin(); it != outputDevices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioOutTree);
        treeItem->setText(1, it->deviceName());
        bool systemDefault = it->deviceName() == defaultOutputDeviceInfo.deviceName();
        found = m_audioDeviceManager->getOutputDeviceInfo(it->deviceName(), outDeviceInfo);
        treeItem->setText(0, QString(systemDefault ? "S" : "_") + QString(found ? "_" : "D"));

        if (systemDefault) {
            treeItem->setBackground(1, QBrush(qRgb(96,96,96)));
        }
    }

    ui->audioOutTree->resizeColumnToContents(0);
    ui->audioOutTree->resizeColumnToContents(1);

    // in panel

    AudioDeviceManager::InputDeviceInfo inDeviceInfo;
    AudioDeviceInfo defaultInputDeviceInfo = AudioDeviceInfo::defaultInputDevice();
    treeItem = new QTreeWidgetItem(ui->audioInTree);
    treeItem->setText(1, AudioDeviceManager::m_defaultDeviceName);
    found = m_audioDeviceManager->getInputDeviceInfo(AudioDeviceManager::m_defaultDeviceName, inDeviceInfo);
    treeItem->setText(0, found ? "__" : "_D");
    ui->audioInTree->setCurrentItem(treeItem);

    const QList<AudioDeviceInfo>& inputDevices = m_audioDeviceManager->getInputDevices();

    for(QList<AudioDeviceInfo>::const_iterator it = inputDevices.begin(); it != inputDevices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioInTree);
        treeItem->setText(1, it->deviceName());
        bool systemDefault = it->deviceName() == defaultInputDeviceInfo.deviceName();
        found = m_audioDeviceManager->getInputDeviceInfo(it->deviceName(), inDeviceInfo);
        treeItem->setText(0, QString(systemDefault ? "S" : "_") + QString(found ? "_" : "D"));

        if (systemDefault) {
            treeItem->setBackground(1, QBrush(qRgb(96,96,96)));
        }
    }

    ui->audioInTree->resizeColumnToContents(0);
    ui->audioInTree->resizeColumnToContents(1);

    m_outputUDPPort = 9998;
    m_outIndex = -1;
    m_inIndex = -1;

	ui->tabWidget->setCurrentIndex(0);
}

AudioDialogX::~AudioDialogX()
{
	delete ui;
}

void AudioDialogX::accept()
{
    m_inIndex = ui->audioInTree->indexOfTopLevelItem(ui->audioInTree->currentItem());
    m_outIndex = ui->audioOutTree->indexOfTopLevelItem(ui->audioOutTree->currentItem());

    if (ui->tabWidget->currentIndex() == 0) // output
    {
        updateOutputDeviceInfo();

        if (ui->outputResetKey->isChecked()) {
            m_audioDeviceManager->unsetOutputDeviceInfo(m_outIndex-1);
        } else {
            m_audioDeviceManager->setOutputDeviceInfo(m_outIndex-1, m_outputDeviceInfo);
        }
    }
    else if (ui->tabWidget->currentIndex() == 1) // input
    {
        updateInputDeviceInfo();

        if (ui->inputResetKey->isChecked()) {
            m_audioDeviceManager->unsetInputDeviceInfo(m_inIndex-1);
        } else {
            m_audioDeviceManager->setInputDeviceInfo(m_inIndex-1, m_inputDeviceInfo);
        }
    }

    QDialog::accept();
}

void AudioDialogX::reject()
{
    QDialog::reject();
}

void AudioDialogX::on_audioInTree_currentItemChanged(
        QTreeWidgetItem* currentItem,
        QTreeWidgetItem* previousItem)
{
    AudioDeviceManager::InputDeviceInfo inDeviceInfo;
    QString inDeviceName = currentItem->text(1);
    int newIndex = ui->audioInTree->indexOfTopLevelItem(currentItem);
    int oldIndex = ui->audioInTree->indexOfTopLevelItem(previousItem);

    if (newIndex != oldIndex) {
        ui->inputResetKey->setChecked(false);
    }

    m_audioDeviceManager->getInputDeviceInfo(inDeviceName, inDeviceInfo);
    m_inputDeviceInfo = inDeviceInfo;

    updateInputDisplay();
}

void AudioDialogX::on_audioOutTree_currentItemChanged(
        QTreeWidgetItem* currentItem,
        QTreeWidgetItem* previousItem)
{
    AudioDeviceManager::OutputDeviceInfo outDeviceInfo;
    QString outDeviceName = currentItem->text(1);
    int newIndex = ui->audioOutTree->indexOfTopLevelItem(currentItem);
    int oldIndex = ui->audioOutTree->indexOfTopLevelItem(previousItem);

    if (newIndex != oldIndex) {
        ui->outputResetKey->setChecked(false);
    }

    m_audioDeviceManager->getOutputDeviceInfo(outDeviceName, outDeviceInfo);
    m_outputDeviceInfo = outDeviceInfo;

    updateOutputDisplay();
}

void AudioDialogX::on_inputVolume_valueChanged(int value)
{
    float volume = value / 100.0f;
    ui->inputVolumeText->setText(QString("%1").arg(volume, 0, 'f', 2));
}

void AudioDialogX::on_inputReset_clicked(bool checked)
{
    (void) checked;
    m_inputDeviceInfo.resetToDefaults();
    updateInputDisplay();
}

void AudioDialogX::on_inputCleanup_clicked(bool checked)
{
    (void) checked;
    m_audioDeviceManager->inputInfosCleanup();
}

void AudioDialogX::updateInputDisplay()
{
    ui->inputSampleRate->setValue(m_inputDeviceInfo.sampleRate);
    ui->inputVolume->setValue(round(m_inputDeviceInfo.volume * 100.0f));
    ui->inputVolumeText->setText(QString("%1").arg(m_inputDeviceInfo.volume, 0, 'f', 2));
}

void AudioDialogX::updateInputDeviceInfo()
{
    m_inputDeviceInfo.sampleRate = ui->inputSampleRate->value();
    m_inputDeviceInfo.volume = ui->inputVolume->value() / 100.0f;
}

void AudioDialogX::on_outputUDPPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->outputUDPPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024)) {
        udpPort = 9999;
    }

    m_outputUDPPort = udpPort;
    ui->outputUDPPort->setText(tr("%1").arg(m_outputDeviceInfo.udpPort));
}

void AudioDialogX::on_outputReset_clicked(bool checked)
{
    (void) checked;
    m_outputDeviceInfo.resetToDefaults();
    updateOutputDisplay();
}

void AudioDialogX::on_outputCleanup_clicked(bool checked)
{
    (void) checked;
    m_audioDeviceManager->outputInfosCleanup();
}

void AudioDialogX::on_outputSampleRate_valueChanged(int value)
{
    m_outputDeviceInfo.sampleRate = value;
    updateOutputSDPString();
    check();
}

void AudioDialogX::on_decimationFactor_currentIndexChanged(int index)
{
    m_outputDeviceInfo.udpDecimationFactor = index + 1;
    updateOutputSDPString();
    check();
}

void AudioDialogX::on_outputUDPChannelCodec_currentIndexChanged(int index)
{
    m_outputDeviceInfo.udpChannelCodec = (AudioOutputDevice::UDPChannelCodec) index;
    updateOutputSDPString();
    check();
}

void AudioDialogX::on_outputUDPChannelMode_currentIndexChanged(int index)
{
    m_outputDeviceInfo.udpChannelMode = (AudioOutputDevice::UDPChannelMode) index;
    updateOutputSDPString();
    check();
}

void AudioDialogX::on_record_toggled(bool checked)
{
    ui->showFileDialog->setEnabled(!checked);
    m_outputDeviceInfo.recordToFile = checked;
}

void AudioDialogX::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QFileDialog fileDialog(
        this,
        tr("Save record file"),
        m_outputDeviceInfo.fileRecordName,
        tr("WAV Files (*.wav)")
    );

    fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    QStringList fileNames;

    if (fileDialog.exec())
    {
        fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            m_outputDeviceInfo.fileRecordName = fileNames.at(0);
		    ui->fileNameText->setText(m_outputDeviceInfo.fileRecordName);
        }
    }
}

void AudioDialogX::on_recordSilenceTime_valueChanged(int value)
{
    m_outputDeviceInfo.recordSilenceTime = value;
    ui->recordSilenceText->setText(tr("%1").arg(value / 10.0, 0, 'f', 1));
}

void AudioDialogX::updateOutputDisplay()
{
    ui->outputSampleRate->blockSignals(true);
    ui->outputUDPChannelMode->blockSignals(true);
    ui->outputUDPChannelCodec->blockSignals(true);
    ui->decimationFactor->blockSignals(true);

    ui->outputSampleRate->setValue(m_outputDeviceInfo.sampleRate);
    ui->outputUDPAddress->setText(m_outputDeviceInfo.udpAddress);
    ui->outputUDPPort->setText(tr("%1").arg(m_outputDeviceInfo.udpPort));
    ui->outputUDPCopy->setChecked(m_outputDeviceInfo.copyToUDP);
    ui->outputUDPUseRTP->setChecked(m_outputDeviceInfo.udpUseRTP);
    ui->outputUDPChannelMode->setCurrentIndex((int) m_outputDeviceInfo.udpChannelMode);
    ui->outputUDPChannelCodec->setCurrentIndex((int) m_outputDeviceInfo.udpChannelCodec);
    ui->decimationFactor->setCurrentIndex(m_outputDeviceInfo.udpDecimationFactor == 0 ? 0 : m_outputDeviceInfo.udpDecimationFactor - 1);
    ui->record->setChecked(m_outputDeviceInfo.recordToFile);
    ui->fileNameText->setText(m_outputDeviceInfo.fileRecordName);
    ui->showFileDialog->setEnabled(!m_outputDeviceInfo.recordToFile);
    ui->recordSilenceTime->setValue(m_outputDeviceInfo.recordSilenceTime);
    ui->recordSilenceText->setText(tr("%1").arg(m_outputDeviceInfo.recordSilenceTime / 10.0, 0, 'f', 1));

    updateOutputSDPString();

    ui->outputSampleRate->blockSignals(false);
    ui->outputUDPChannelMode->blockSignals(false);
    ui->outputUDPChannelCodec->blockSignals(false);
    ui->decimationFactor->blockSignals(false);
}

void AudioDialogX::updateOutputDeviceInfo()
{
    m_outputDeviceInfo.sampleRate = ui->outputSampleRate->value();
    m_outputDeviceInfo.udpAddress = ui->outputUDPAddress->text();
    m_outputDeviceInfo.udpPort = m_outputUDPPort;
    m_outputDeviceInfo.copyToUDP = ui->outputUDPCopy->isChecked();
    m_outputDeviceInfo.udpUseRTP = ui->outputUDPUseRTP->isChecked();
    m_outputDeviceInfo.udpChannelMode = (AudioOutputDevice::UDPChannelMode) ui->outputUDPChannelMode->currentIndex();
    m_outputDeviceInfo.udpChannelCodec = (AudioOutputDevice::UDPChannelCodec) ui->outputUDPChannelCodec->currentIndex();
    m_outputDeviceInfo.udpDecimationFactor = ui->decimationFactor->currentIndex() + 1;
    m_outputDeviceInfo.recordToFile = ui->record->isChecked();
    m_outputDeviceInfo.fileRecordName = ui->fileNameText->text();
    m_outputDeviceInfo.recordSilenceTime = ui->recordSilenceTime->value();
}

void AudioDialogX::updateOutputSDPString()
{
    QString format;
    int nChannels = m_outputDeviceInfo.udpChannelMode == AudioOutputDevice::UDPChannelStereo ? 2 : 1;
    uint32_t effectiveSampleRate = m_outputDeviceInfo.sampleRate / (m_outputDeviceInfo.udpDecimationFactor == 0 ? 1 : m_outputDeviceInfo.udpDecimationFactor);

    switch (m_outputDeviceInfo.udpChannelCodec)
    {
    case AudioOutputDevice::UDPCodecALaw:
        format = "PCMA";
        break;
    case AudioOutputDevice::UDPCodecULaw:
        format = "PCMU";
        break;
    case AudioOutputDevice::UDPCodecG722:
        format = "G722";
        effectiveSampleRate /= 2; // codec does a decimation by 2
        break;
    case AudioOutputDevice::UDPCodecL8:
        format = "L8";
        break;
    case AudioOutputDevice::UDPCodecOpus:
        format = "opus";
        nChannels = 2; // always 2 even for mono
        effectiveSampleRate = 48000; // always 48000 regardless of input rate
        break;
    case AudioOutputDevice::UDPCodecL16:
    default:
        format = "L16";
        break;
    }

    ui->outputSDPText->setText(tr("%1/%2/%3").arg(format).arg(effectiveSampleRate).arg(nChannels));
}

void AudioDialogX::check()
{
    int nChannels = m_outputDeviceInfo.udpChannelMode == AudioOutputDevice::UDPChannelStereo ? 2 : 1;
    uint32_t decimationFactor = m_outputDeviceInfo.udpDecimationFactor == 0 ? 1 : m_outputDeviceInfo.udpDecimationFactor;

    if (m_outputDeviceInfo.udpChannelCodec == AudioOutputDevice::UDPCodecALaw)
    {
        if ((nChannels != 1) || (m_outputDeviceInfo.sampleRate/decimationFactor != 8000)) {
            QMessageBox::information(this, tr("Message"), tr("PCMA must be 8000 Hz single channel"));
        }
    }
    else if (m_outputDeviceInfo.udpChannelCodec == AudioOutputDevice::UDPCodecULaw)
    {
        if ((nChannels != 1) || (m_outputDeviceInfo.sampleRate/decimationFactor != 8000)) {
            QMessageBox::information(this, tr("Message"), tr("PCMU must be 8000 Hz single channel"));
        }
    }
    else if (m_outputDeviceInfo.udpChannelCodec == AudioOutputDevice::UDPCodecG722)
    {
        if ((nChannels != 1) || (m_outputDeviceInfo.sampleRate/decimationFactor != 16000)) {
            QMessageBox::information(this, tr("Message"), tr("G722 must be 16000 Hz single channel"));
        }
    }
    else if (m_outputDeviceInfo.udpChannelCodec == AudioOutputDevice::UDPCodecOpus)
    {
        int effectiveSampleRate = m_outputDeviceInfo.sampleRate/decimationFactor;
        if ((effectiveSampleRate != 48000) && (effectiveSampleRate != 24000) && (effectiveSampleRate != 16000) && (effectiveSampleRate != 12000)) {
            QMessageBox::information(this, tr("Message"), tr("Opus takes only 48, 24, 16 or 12 kHz sample rates"));
        }
    }
}
