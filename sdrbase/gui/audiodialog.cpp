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

	const AudioDeviceInfo::Devices& devices = audioDeviceInfo->getDevices();

	QTreeWidgetItem* api;
	QStringList sl;
	sl.append(tr("Default (use first suitable device)"));
	api = new QTreeWidgetItem(ui->audioOutTree, sl, ATDefault);
	api->setFirstColumnSpanned(true);

	for(AudioDeviceInfo::Devices::const_iterator it = devices.begin(); it != devices.end(); ++it)
	{
		int apiIndex;
		sl.clear();

		for(apiIndex = 0; apiIndex < ui->audioOutTree->topLevelItemCount(); ++apiIndex)
		{
			if(ui->audioOutTree->topLevelItem(apiIndex)->text(0) == it->api)
				break;
		}

		if(apiIndex >= ui->audioOutTree->topLevelItemCount())
		{
			sl.append(it->api);
			api = new QTreeWidgetItem(ui->audioOutTree, sl, ATInterface);
			api->setExpanded(true);
			api->setFirstColumnSpanned(true);
			sl.clear();
		}
		else
		{
			api = ui->audioOutTree->topLevelItem(apiIndex);
		}

		sl.append(it->name);
		new QTreeWidgetItem(api, sl, ATDevice);
	}

    if(ui->audioOutTree->currentItem() == NULL)
        ui->audioOutTree->setCurrentItem(ui->audioOutTree->topLevelItem(0));

    sl.clear();
    sl.append(tr("Default (use first suitable device)"));
    api = new QTreeWidgetItem(ui->audioInTree, sl, ATDefault);
    api->setFirstColumnSpanned(true);

	if(ui->audioInTree->currentItem() == NULL)
		ui->audioInTree->setCurrentItem(ui->audioInTree->topLevelItem(0));

	ui->tabWidget->setCurrentIndex(0);
}

AudioDialog::~AudioDialog()
{
	delete ui;
}

void AudioDialog::accept()
{
	QDialog::accept();
}

void AudioDialog::on_inputVolume_valueChanged(int value)
{
    float inputVolume = (float) value / 100.0f;
    ui->inputVolumeText->setText(QString("%1").arg(inputVolume, 0, 'f', 2));
}
