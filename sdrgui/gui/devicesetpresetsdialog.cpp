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

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>

#include "settings/preset.h"
#include "gui/presetitem.h"
#include "gui/addpresetdialog.h"
#include "device/deviceuiset.h"
#include "maincore.h"

#include "devicesetpresetsdialog.h"
#include "ui_devicesetpresetsdialog.h"

DeviceSetPresetsDialog::DeviceSetPresetsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::DeviceSetPresetsDialog),
    m_deviceSetPresets(nullptr),
    m_deviceUISet(nullptr),
    m_pluginAPI(nullptr),
    m_currentWorkspace(nullptr),
    m_workspaces(nullptr),
    m_presetLoaded(false)
{
    ui->setupUi(this);
}

DeviceSetPresetsDialog::~DeviceSetPresetsDialog()
{
    delete ui;
}

void DeviceSetPresetsDialog::populateTree(int deviceType)
{
    if (!m_deviceSetPresets) {
        return;
    }

    QList<Preset*>::const_iterator it = m_deviceSetPresets->begin();
    QList<QTreeWidgetItem*> treeItems;
    ui->presetTree->clear();

    for (int i = 0; it != m_deviceSetPresets->end(); ++it, i++)
    {
        if (((*it)->isSourcePreset() && (deviceType == 0)) ||
            ((*it)->isSinkPreset() && (deviceType == 1)) ||
            ((*it)->isMIMOPreset() && (deviceType == 2)))
        {
            QTreeWidgetItem *treeItem = addPresetToTree(*it);
            treeItems.push_back(treeItem);
        }
    }

    if (treeItems.size() > 0) {
        ui->presetTree->setCurrentItem(treeItems.at(treeItems.size()/2));
    }

    updatePresetControls();
}

QTreeWidgetItem* DeviceSetPresetsDialog::addPresetToTree(const Preset* preset)
{
	QTreeWidgetItem* group = 0;

	for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++)
	{
		if(ui->presetTree->topLevelItem(i)->text(0) == preset->getGroup())
		{
			group = ui->presetTree->topLevelItem(i);
			break;
		}
	}

	if(group == 0)
	{
		QStringList sl;
		sl.append(preset->getGroup());
		group = new QTreeWidgetItem(ui->presetTree, sl, PGroup);
		group->setFirstColumnSpanned(true);
		group->setExpanded(true);
		ui->presetTree->sortByColumn(0, Qt::AscendingOrder);
	}

	QStringList sl;
	sl.append(QString("%1").arg(preset->getCenterFrequency() / 1e6f, 0, 'f', 3)); // frequency column
	sl.append(QString("%1").arg(preset->isSourcePreset() ? 'R' : preset->isSinkPreset() ? 'T' : preset->isMIMOPreset() ? 'M' : 'X'));           // mode column
	sl.append(preset->getDescription());                                          // description column
	PresetItem* item = new PresetItem(group, sl, preset->getCenterFrequency(), PItem);
	item->setTextAlignment(0, Qt::AlignRight);
	item->setData(0, Qt::UserRole, QVariant::fromValue(preset));
	ui->presetTree->resizeColumnToContents(0); // Resize frequency column to minimum
    ui->presetTree->resizeColumnToContents(1); // Resize mode column to minimum

	updatePresetControls();
	return item;
}

void DeviceSetPresetsDialog::updatePresetControls()
{
	ui->presetTree->resizeColumnToContents(0);

	if (ui->presetTree->currentItem() != 0)
	{
	 	ui->presetDelete->setEnabled(true);
	 	ui->presetLoad->setEnabled(true);
	}
	else
	{
	 	ui->presetDelete->setEnabled(false);
	 	ui->presetLoad->setEnabled(false);
	}
}

void DeviceSetPresetsDialog::on_presetSave_clicked()
{
    QStringList groups;
    QString group;
    QString description = "";

    for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++) {
        groups.append(ui->presetTree->topLevelItem(i)->text(0));
    }

    QTreeWidgetItem* item = ui->presetTree->currentItem();

    if (item)
    {
        if (item->type() == PGroup)
        {
            group = item->text(0);
        }
        else if (item->type() == PItem)
        {
            group = item->parent()->text(0);
            description = item->text(0);
        }
    }

    AddPresetDialog dlg(groups, group, this);

    if (description.length() > 0) {
        dlg.setDescription(description);
    }

    if (dlg.exec() == QDialog::Accepted)
    {
        Preset* preset = MainCore::instance()->m_settings.newPreset(dlg.group(), dlg.description());
        m_deviceUISet->saveDeviceSetSettings(preset);
        ui->presetTree->setCurrentItem(addPresetToTree(preset));
    }

    MainCore::instance()->m_settings.sortPresets();
}

void DeviceSetPresetsDialog::on_presetUpdate_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();
	const Preset* changedPreset = 0;

	if(item != 0)
	{
		if(item->type() == PItem)
		{
			const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

			if (preset != 0)
			{
				Preset* preset_mod = const_cast<Preset*>(preset);
                m_deviceUISet->saveDeviceSetSettings(preset_mod);
				changedPreset = preset;
			}
		}
	}

	MainCore::instance()->m_settings.sortPresets();
    ui->presetTree->clear();

    for (int i = 0; i < MainCore::instance()->m_settings.getPresetCount(); ++i)
    {
        QTreeWidgetItem *item_x = addPresetToTree(MainCore::instance()->m_settings.getPreset(i));
        const Preset* preset_x = qvariant_cast<const Preset*>(item_x->data(0, Qt::UserRole));

        if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
            ui->presetTree->setCurrentItem(item_x);
        }
    }
}

void DeviceSetPresetsDialog::on_presetEdit_clicked()
{
    QTreeWidgetItem* item = ui->presetTree->currentItem();
    QStringList groups;
    bool change = false;
    const Preset *changedPreset = 0;
    QString newGroupName;

    for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++) {
        groups.append(ui->presetTree->topLevelItem(i)->text(0));
    }

    if (item)
    {
        if (item->type() == PItem)
        {
            const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
            AddPresetDialog dlg(groups, preset->getGroup(), this);
            dlg.setDescription(preset->getDescription());

            if (dlg.exec() == QDialog::Accepted)
            {
                Preset* preset_mod = const_cast<Preset*>(preset);
                preset_mod->setGroup(dlg.group());
                preset_mod->setDescription(dlg.description());
                change = true;
                changedPreset = preset;
            }
        }
        else if (item->type() == PGroup)
        {
            AddPresetDialog dlg(groups, item->text(0), this);
            dlg.showGroupOnly();
            dlg.setDialogTitle("Edit preset group");

            if (dlg.exec() == QDialog::Accepted)
            {
                MainCore::instance()->m_settings.renamePresetGroup(item->text(0), dlg.group());
                newGroupName = dlg.group();
                change = true;
            }
        }
    }

    if (change)
    {
        MainCore::instance()->m_settings.sortPresets();
        ui->presetTree->clear();

        for (int i = 0; i < MainCore::instance()->m_settings.getPresetCount(); ++i)
        {
            QTreeWidgetItem *item_x = addPresetToTree(MainCore::instance()->m_settings.getPreset(i));
            const Preset* preset_x = qvariant_cast<const Preset*>(item_x->data(0, Qt::UserRole));

            if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
                ui->presetTree->setCurrentItem(item_x);
            }
        }

        if (!changedPreset) // on group name change set cursor on the group that has been changed
        {
            for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++)
            {
                QTreeWidgetItem* item = ui->presetTree->topLevelItem(i);

                if (item->text(0) == newGroupName) {
                    ui->presetTree->setCurrentItem(item);
                }
            }
        }
    }
}

void DeviceSetPresetsDialog::on_presetExport_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if (item)
    {
		if (item->type() == PItem)
		{
			const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
			QString base64Str = preset->serialize().toBase64();
			QString fileName = QFileDialog::getSaveFileName(
                this,
			    tr("Open preset export file"),
                ".",
                tr("Preset export files (*.prex)"),
                0
            );

			if (fileName != "")
			{
				QFileInfo fileInfo(fileName);

				if (fileInfo.suffix() != "prex") {
					fileName += ".prex";
				}

				QFile exportFile(fileName);

				if (exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
				{
					QTextStream outstream(&exportFile);
					outstream << base64Str;
					exportFile.close();
				}
				else
				{
			    	QMessageBox::information(this, tr("Message"), tr("Cannot open file for writing"));
				}
			}
		}
	}
}

void DeviceSetPresetsDialog::on_presetImport_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if (item)
	{
		QString group;

		if (item->type() == PGroup)	{
			group = item->text(0);
		} else if (item->type() == PItem) {
			group = item->parent()->text(0);
		} else {
			return;
		}

		QString fileName = QFileDialog::getOpenFileName(
            this,
		    tr("Open preset export file"),
            ".",
            tr("Preset export files (*.prex)"),
            0
        );

		if (fileName != "")
		{
			QFile exportFile(fileName);

			if (exportFile.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				QByteArray base64Str;
				QTextStream instream(&exportFile);
				instream >> base64Str;
				exportFile.close();

				Preset* preset = MainCore::instance()->m_settings.newPreset("", "");
				preset->deserialize(QByteArray::fromBase64(base64Str));
				preset->setGroup(group); // override with current group

				ui->presetTree->setCurrentItem(addPresetToTree(preset));
			}
			else
			{
				QMessageBox::information(this, tr("Message"), tr("Cannot open file for reading"));
			}
		}
	}
}

void DeviceSetPresetsDialog::on_presetLoad_clicked()
{
	qDebug("DeviceSetPresetsDialog::on_presetLoad_clicked");

	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if (!item)
	{
		qDebug("DeviceSetPresetsDialog::on_presetLoad_clicked: item null");
		updatePresetControls();
		return;
	}

	const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

	if (!preset)
	{
		qDebug("DeviceSetPresetsDialog::on_presetLoad_clicked: preset null");
		return;
	}

	loadDeviceSetPresetSettings(preset);
}

void DeviceSetPresetsDialog::on_presetDelete_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if (!item)
	{
		updatePresetControls();
		return;
	}
	else
	{
        if (item->type() == PItem)
        {
            const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

            if (preset)
            {
                if (QMessageBox::question(
                    this,
                    tr("Delete Preset"),
                    tr("Do you want to delete preset '%1'?").arg(preset->getDescription()),
                    QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes
                )
                {
                    delete item;
                    MainCore::instance()->m_settings.deletePreset(preset);
                }
            }
        }
        else if (item->type() == PGroup)
        {
            if (QMessageBox::question(
                this,
                tr("Delete preset group"),
                tr("Do you want to delete preset group '%1'?").arg(item->text(0)),
                QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes
            )
            {
                MainCore::instance()->m_settings.deletePresetGroup(item->text(0));

                ui->presetTree->clear();

                for (int i = 0; i < MainCore::instance()->m_settings.getPresetCount(); ++i) {
                    addPresetToTree(MainCore::instance()->m_settings.getPreset(i));
                }
            }
        }
	}
}

void DeviceSetPresetsDialog::on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    (void) current;
    (void) previous;
	updatePresetControls();
}

void DeviceSetPresetsDialog::on_presetTree_itemActivated(QTreeWidgetItem *item, int column)
{
    (void) item;
    (void) column;
	on_presetLoad_clicked();
}

void DeviceSetPresetsDialog::loadDeviceSetPresetSettings(const Preset* preset)
{
	qDebug("DeviceSetPresetsDialog::loadPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    m_deviceUISet->loadDeviceSetSettings(preset, m_pluginAPI, m_workspaces, m_currentWorkspace);
    m_presetLoaded = true;
}
