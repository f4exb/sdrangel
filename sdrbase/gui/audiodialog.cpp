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
	api = new QTreeWidgetItem(ui->audioTree, sl, ATDefault);
	api->setFirstColumnSpanned(true);
	for(AudioDeviceInfo::Devices::const_iterator it = devices.begin(); it != devices.end(); ++it) {
		int apiIndex;
		sl.clear();

		for(apiIndex = 0; apiIndex < ui->audioTree->topLevelItemCount(); ++apiIndex) {
			if(ui->audioTree->topLevelItem(apiIndex)->text(0) == it->api)
				break;
		}
		if(apiIndex >= ui->audioTree->topLevelItemCount()) {
			sl.append(it->api);
			api = new QTreeWidgetItem(ui->audioTree, sl, ATInterface);
			api->setExpanded(true);
			api->setFirstColumnSpanned(true);
			sl.clear();
		} else {
			api = ui->audioTree->topLevelItem(apiIndex);
		}

		sl.append(it->name);
		new QTreeWidgetItem(api, sl, ATDevice);
	}
	if(ui->audioTree->currentItem() == NULL)
		ui->audioTree->setCurrentItem(ui->audioTree->topLevelItem(0));

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
