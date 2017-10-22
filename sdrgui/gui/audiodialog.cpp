#include <gui/audiodialog.h>
#include <QTreeWidgetItem>
#include "ui_audiodialog.h"
#include "audio/audiodeviceinfo.h"

AudioDialog::AudioDialog(AudioDeviceInfo* audioDeviceInfo, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AudioDialog),
	m_audioDeviceInfo(audioDeviceInfo)
{
	ui->setupUi(this);
	QTreeWidgetItem* treeItem;
	int i;

	// out panel

	treeItem = new QTreeWidgetItem(ui->audioOutTree);
	treeItem->setText(0, tr("Default (use first suitable device)"));

	const QList<QAudioDeviceInfo>& outputDevices = m_audioDeviceInfo->getOutputDevices();
	i = 0;

    for(QList<QAudioDeviceInfo>::const_iterator it = outputDevices.begin(); it != outputDevices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioOutTree);
        treeItem->setText(0, qPrintable(it->deviceName()));

        if (i == m_audioDeviceInfo->getOutputDeviceIndex())
        {
            ui->audioOutTree->setCurrentItem(treeItem);
        }

        i++;
    }

    // in panel

    treeItem = new QTreeWidgetItem(ui->audioInTree);
    treeItem->setText(0, tr("Default (use first suitable device)"));

    const QList<QAudioDeviceInfo>& inputDevices = m_audioDeviceInfo->getInputDevices();
    i = 0;

    for(QList<QAudioDeviceInfo>::const_iterator it = inputDevices.begin(); it != inputDevices.end(); ++it)
    {
        treeItem = new QTreeWidgetItem(ui->audioInTree);
        treeItem->setText(0, qPrintable(it->deviceName()));

        if (i == m_audioDeviceInfo->getInputDeviceIndex())
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

    m_inputVolume = m_audioDeviceInfo->m_inputVolume;
	ui->inputVolume->setValue((int) (m_inputVolume * 100.0f));
	ui->inputVolumeText->setText(QString("%1").arg(m_inputVolume, 0, 'f', 2));
}

AudioDialog::~AudioDialog()
{
	delete ui;
}

void AudioDialog::accept()
{
    int inIndex = ui->audioInTree->indexOfTopLevelItem(ui->audioInTree->currentItem());
    int outIndex = ui->audioOutTree->indexOfTopLevelItem(ui->audioOutTree->currentItem());

    m_audioDeviceInfo->m_inputDeviceIndex = inIndex - 1;
    m_audioDeviceInfo->m_outputDeviceIndex = outIndex - 1;
    m_audioDeviceInfo->m_inputVolume = m_inputVolume;

	QDialog::accept();
}

void AudioDialog::on_inputVolume_valueChanged(int value)
{
    m_inputVolume = (float) value / 100.0f;
    ui->inputVolumeText->setText(QString("%1").arg(m_inputVolume, 0, 'f', 2));
}
