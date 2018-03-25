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
	int i;

	// out panel

	QAudioDeviceInfo defaultOutputDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
	treeItem = new QTreeWidgetItem(ui->audioOutTree);
	treeItem->setText(0, tr("System default output device"));

	const QList<QAudioDeviceInfo>& outputDevices = m_audioDeviceManager->getOutputDevices();
	i = 0;

    for(QList<QAudioDeviceInfo>::const_iterator it = outputDevices.begin(); it != outputDevices.end(); ++it)
    {
        bool isDefaultDevice = it->deviceName() == defaultOutputDeviceInfo.deviceName();
        treeItem = new QTreeWidgetItem(ui->audioOutTree);
        treeItem->setText(0, it->deviceName() + (isDefaultDevice ? "(*)" : ""));

        if (i == 0)
        {
            ui->audioOutTree->setCurrentItem(treeItem);
        }

        i++;
    }

    // in panel

    QAudioDeviceInfo defaultInputDeviceInfo = QAudioDeviceInfo::defaultInputDevice();
    treeItem = new QTreeWidgetItem(ui->audioInTree);
    treeItem->setText(0, tr("System default input device"));

    const QList<QAudioDeviceInfo>& inputDevices = m_audioDeviceManager->getInputDevices();
    i = 0;

    for(QList<QAudioDeviceInfo>::const_iterator it = inputDevices.begin(); it != inputDevices.end(); ++it)
    {
        bool isDefaultDevice = it->deviceName() == defaultInputDeviceInfo.deviceName();
        treeItem = new QTreeWidgetItem(ui->audioInTree);
        treeItem->setText(0, it->deviceName() + (isDefaultDevice ? "(*)" : ""));

        if (i == 0)
        {
            ui->audioInTree->setCurrentItem(treeItem);
        }

        i++;
    }

    if(ui->audioOutTree->currentItem() == 0) {
        ui->audioOutTree->setCurrentItem(ui->audioOutTree->topLevelItem(0));
    }

	if(ui->audioInTree->currentItem() == 0) {
		ui->audioInTree->setCurrentItem(ui->audioInTree->topLevelItem(0));
	}

	ui->tabWidget->setCurrentIndex(0);

	ui->inputVolume->setValue((int) (m_inputVolume * 100.0f));
	ui->inputVolumeText->setText(QString("%1").arg(m_inputVolume, 0, 'f', 2));
}

AudioDialogX::~AudioDialogX()
{
	delete ui;
}

void AudioDialogX::accept()
{
	QDialog::accept();
}

void AudioDialogX::on_inputVolume_valueChanged(int value)
{
    m_inputVolume = (float) value / 100.0f;
    ui->inputVolumeText->setText(QString("%1").arg(m_inputVolume, 0, 'f', 2));
}
