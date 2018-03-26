#include "audiodialog.h"

#include <audio/audiodevicemanager.h>
#include <QTreeWidgetItem>
#include "ui_audiodialog.h"

AudioDialogX::AudioDialogX(AudioDeviceManager* audioDeviceManager, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AudioDialog),
	m_audioDeviceManager(audioDeviceManager)
{
	ui->setupUi(this);
	QTreeWidgetItem* treeItem;

	// out panel

	QAudioDeviceInfo defaultOutputDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
	treeItem = new QTreeWidgetItem(ui->audioOutTree);
	treeItem->setText(0, AudioDeviceManager::m_defaultDeviceName);
	ui->audioOutTree->setCurrentItem(treeItem);

	const QList<QAudioDeviceInfo>& outputDevices = m_audioDeviceManager->getOutputDevices();

    for(QList<QAudioDeviceInfo>::const_iterator it = outputDevices.begin(); it != outputDevices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioOutTree);
        treeItem->setText(0, it->deviceName());

        if (it->deviceName() == defaultOutputDeviceInfo.deviceName()) {
            treeItem->setBackground(0, QBrush(qRgb(96,96,96)));
        }
    }

    // in panel

    QAudioDeviceInfo defaultInputDeviceInfo = QAudioDeviceInfo::defaultInputDevice();
    treeItem = new QTreeWidgetItem(ui->audioInTree);
    treeItem->setText(0, AudioDeviceManager::m_defaultDeviceName);
    ui->audioInTree->setCurrentItem(treeItem);

    const QList<QAudioDeviceInfo>& inputDevices = m_audioDeviceManager->getInputDevices();

    for(QList<QAudioDeviceInfo>::const_iterator it = inputDevices.begin(); it != inputDevices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioInTree);
        treeItem->setText(0, it->deviceName());

        if (it->deviceName() == defaultInputDeviceInfo.deviceName()) {
            treeItem->setBackground(0, QBrush(qRgb(96,96,96)));
        }
    }

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
    QString inDeviceName = currentItem->text(0);
    int newIndex = ui->audioInTree->indexOfTopLevelItem(currentItem);
    int oldIndex = ui->audioInTree->indexOfTopLevelItem(previousItem);
    //qDebug("AudioDialogX::on_audioInTree_currentItemChanged: %s", qPrintable(inDeviceName));

    if (newIndex != oldIndex) {
        ui->inputResetKey->setChecked(false);
    }

    bool found = m_audioDeviceManager->getInputDeviceInfo(inDeviceName, inDeviceInfo);
    m_inputDeviceInfo = inDeviceInfo;
    ui->inputDefaultText->setText(found ? "" : "D");

    updateInputDisplay();
}

void AudioDialogX::on_audioOutTree_currentItemChanged(
        QTreeWidgetItem* currentItem,
        QTreeWidgetItem* previousItem)
{
    AudioDeviceManager::OutputDeviceInfo outDeviceInfo;
    QString outDeviceName = currentItem->text(0);
    int newIndex = ui->audioOutTree->indexOfTopLevelItem(currentItem);
    int oldIndex = ui->audioOutTree->indexOfTopLevelItem(previousItem);

    if (newIndex != oldIndex) {
        ui->outputResetKey->setChecked(false);
    }

    //qDebug("AudioDialogX::on_audioOutTree_currentItemChanged: %s", qPrintable(outDeviceName));
    bool found = m_audioDeviceManager->getOutputDeviceInfo(outDeviceName, outDeviceInfo);
    m_outputDeviceInfo = outDeviceInfo;
    ui->outputDefaultText->setText(found ? "" : "D");

    updateOutputDisplay();
}

void AudioDialogX::on_inputVolume_valueChanged(int value)
{
    float volume = value / 100.0f;
    ui->inputVolumeText->setText(QString("%1").arg(volume, 0, 'f', 2));
}

void AudioDialogX::on_inputReset_clicked(bool checked __attribute__((unused)))
{
    m_inputDeviceInfo.resetToDefaults();
    updateInputDisplay();
}

void AudioDialogX::on_inputCleanup_clicked(bool checked __attribute__((unused)))
{
    m_audioDeviceManager->inputInfosCleanup();
}

void AudioDialogX::updateInputDisplay()
{
    ui->inputSampleRate->setValue(m_inputDeviceInfo.sampleRate);
    ui->inputVolume->setValue(roundf(m_inputDeviceInfo.volume * 100.0f));
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

void AudioDialogX::on_outputReset_clicked(bool checked __attribute__((unused)))
{
    m_outputDeviceInfo.resetToDefaults();
    updateOutputDisplay();
}

void AudioDialogX::on_outputCleanup_clicked(bool checked __attribute__((unused)))
{
    m_audioDeviceManager->outputInfosCleanup();
}

void AudioDialogX::updateOutputDisplay()
{
    ui->outputSampleRate->setValue(m_outputDeviceInfo.sampleRate);
    ui->outputUDPAddress->setText(m_outputDeviceInfo.udpAddress);
    ui->outputUDPPort->setText(tr("%1").arg(m_outputDeviceInfo.udpPort));
    ui->outputUDPCopy->setChecked(m_outputDeviceInfo.copyToUDP);
    ui->outputUDPUseRTP->setChecked(m_outputDeviceInfo.udpUseRTP);
    ui->outputUDPChannelMode->setCurrentIndex((int) m_outputDeviceInfo.udpChannelMode);
}

void AudioDialogX::updateOutputDeviceInfo()
{
    m_outputDeviceInfo.sampleRate = ui->outputSampleRate->value();
    m_outputDeviceInfo.udpAddress = ui->outputUDPAddress->text();
    m_outputDeviceInfo.udpPort = m_outputUDPPort;
    m_outputDeviceInfo.copyToUDP = ui->outputUDPCopy->isChecked();
    m_outputDeviceInfo.udpUseRTP = ui->outputUDPUseRTP->isChecked();
    m_outputDeviceInfo.udpChannelMode = (AudioOutput::UDPChannelMode) ui->outputUDPChannelMode->currentIndex();
}

