///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
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

#include <QMessageBox>
#include <QDebug>

#include "gui/addpresetdialog.h"
#include "feature/featureuiset.h"
#include "settings/featuresetpreset.h"
#include "maincore.h"

#include "featurepresetsdialog.h"
#include "ui_featurepresetsdialog.h"

FeaturePresetsDialog::FeaturePresetsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FeaturePresetsDialog),
    m_featureSetPresets(nullptr),
    m_featureUISet(nullptr),
    m_pluginAPI(nullptr),
    m_apiAdapter(nullptr),
    m_currentWorkspace(nullptr),
    m_workspaces(nullptr),
    m_presetLoaded(false)
{
    ui->setupUi(this);
}

FeaturePresetsDialog::~FeaturePresetsDialog()
{
    delete ui;
}

void FeaturePresetsDialog::populateTree()
{
    if (!m_featureSetPresets) {
        return;
    }

    QList<FeatureSetPreset*>::const_iterator it = m_featureSetPresets->begin();
    int middleIndex = m_featureSetPresets->size() / 2;
    QTreeWidgetItem *treeItem;
    ui->presetsTree->clear();

    for (int i = 0; it != m_featureSetPresets->end(); ++it, i++)
    {
        treeItem = addPresetToTree(*it);

        if (i == middleIndex) {
            ui->presetsTree->setCurrentItem(treeItem);
        }
    }

    updatePresetControls();
}

QTreeWidgetItem* FeaturePresetsDialog::addPresetToTree(const FeatureSetPreset* preset)
{
	QTreeWidgetItem* group = nullptr;

	for (int i = 0; i < ui->presetsTree->topLevelItemCount(); i++)
	{
		if (ui->presetsTree->topLevelItem(i)->text(0) == preset->getGroup())
		{
			group = ui->presetsTree->topLevelItem(i);
			break;
		}
	}

	if (!group)
	{
		QStringList sl;
		sl.append(preset->getGroup());
		group = new QTreeWidgetItem(ui->presetsTree, sl, PGroup);
		group->setFirstColumnSpanned(true);
		group->setExpanded(true);
		ui->presetsTree->sortByColumn(0, Qt::AscendingOrder);
	}

	QStringList sl;
	sl.append(preset->getDescription());
    QTreeWidgetItem* item = new QTreeWidgetItem(group, sl, PItem); // description column
	item->setTextAlignment(0, Qt::AlignLeft);
	item->setData(0, Qt::UserRole, QVariant::fromValue(preset));

	updatePresetControls();
	return item;
}

void FeaturePresetsDialog::updatePresetControls()
{
	ui->presetsTree->resizeColumnToContents(0);

	if (ui->presetsTree->currentItem())
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

void FeaturePresetsDialog::on_presetSave_clicked()
{
    QStringList groups;
    QString group;
    QString description = "";

    for (int i = 0; i < ui->presetsTree->topLevelItemCount(); i++) {
        groups.append(ui->presetsTree->topLevelItem(i)->text(0));
    }

    QTreeWidgetItem* item = ui->presetsTree->currentItem();

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
        FeatureSetPreset* preset = newFeatureSetPreset(dlg.group(), dlg.description());
        savePresetSettings(preset);
        ui->presetsTree->setCurrentItem(addPresetToTree(preset));
    }

    sortFeatureSetPresets();
}

void FeaturePresetsDialog::on_presetUpdate_clicked()
{
	QTreeWidgetItem* item = ui->presetsTree->currentItem();
	const FeatureSetPreset* changedPreset = nullptr;

	if (item)
	{
		if( item->type() == PItem)
		{
			const FeatureSetPreset* preset = qvariant_cast<const FeatureSetPreset*>(item->data(0, Qt::UserRole));

			if (preset)
			{
				FeatureSetPreset* preset_mod = const_cast<FeatureSetPreset*>(preset);
				savePresetSettings(preset_mod);
				changedPreset = preset;
			}
		}
	}

	sortFeatureSetPresets();
    ui->presetsTree->clear();

    for (int i = 0; i < m_featureSetPresets->size(); ++i)
    {
        QTreeWidgetItem *item_x = addPresetToTree(m_featureSetPresets->at(i));
        const FeatureSetPreset* preset_x = qvariant_cast<const FeatureSetPreset*>(item_x->data(0, Qt::UserRole));

        if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
            ui->presetsTree->setCurrentItem(item_x);
        }
    }
}

void FeaturePresetsDialog::on_presetEdit_clicked()
{
    QTreeWidgetItem* item = ui->presetsTree->currentItem();
    QStringList groups;
    bool change = false;
    const FeatureSetPreset *changedPreset = nullptr;
    QString newGroupName;

    for (int i = 0; i < ui->presetsTree->topLevelItemCount(); i++) {
        groups.append(ui->presetsTree->topLevelItem(i)->text(0));
    }

    if (item)
    {
        if (item->type() == PItem)
        {
            const FeatureSetPreset* preset = qvariant_cast<const FeatureSetPreset*>(item->data(0, Qt::UserRole));
            AddPresetDialog dlg(groups, preset->getGroup(), this);
            dlg.setDescription(preset->getDescription());

            if (dlg.exec() == QDialog::Accepted)
            {
                FeatureSetPreset* preset_mod = const_cast<FeatureSetPreset*>(preset);
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
                renamePresetGroup(item->text(0), dlg.group());
                newGroupName = dlg.group();
                change = true;
            }
        }
    }

    if (change)
    {
        sortFeatureSetPresets();
        ui->presetsTree->clear();

        for (int i = 0; i < m_featureSetPresets->size(); ++i)
        {
            QTreeWidgetItem *item_x = addPresetToTree(m_featureSetPresets->at(i));
            const FeatureSetPreset* preset_x = qvariant_cast<const FeatureSetPreset*>(item_x->data(0, Qt::UserRole));

            if (changedPreset && (preset_x == changedPreset)) { // set cursor on changed preset
                ui->presetsTree->setCurrentItem(item_x);
            }
        }

        if (!changedPreset) // on group name change set cursor on the group that has been changed
        {
            for(int i = 0; i < ui->presetsTree->topLevelItemCount(); i++)
            {
                QTreeWidgetItem* item = ui->presetsTree->topLevelItem(i);

                if (item->text(0) == newGroupName) {
                    ui->presetsTree->setCurrentItem(item);
                }
            }
        }
    }
}

void FeaturePresetsDialog::on_presetDelete_clicked()
{
	QTreeWidgetItem* item = ui->presetsTree->currentItem();

	if (item == 0)
	{
		updatePresetControls();
		return;
	}
	else
	{
        if (item->type() == PItem)
        {
            const FeatureSetPreset* preset = qvariant_cast<const FeatureSetPreset*>(item->data(0, Qt::UserRole));

            if (preset)
            {
                if(QMessageBox::question(this, tr("Delete Preset"), tr("Do you want to delete preset '%1'?").arg(preset->getDescription()), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                    delete item;
                    deletePreset(preset);
                }
            }
        }
        else if (item->type() == PGroup)
        {
            if (QMessageBox::question(this,
                    tr("Delete preset group"),
                    tr("Do you want to delete preset group '%1'?")
                        .arg(item->text(0)), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
            {
                deletePresetGroup(item->text(0));

                ui->presetsTree->clear();

                for (int i = 0; i < m_featureSetPresets->size(); ++i) {
                    addPresetToTree(m_featureSetPresets->at(i));
                }
            }
        }
	}
}

void FeaturePresetsDialog::on_presetLoad_clicked()
{
	qDebug() << "FeaturePresetsDialog::on_presetLoad_clicked";

	QTreeWidgetItem* item = ui->presetsTree->currentItem();

	if (!item)
	{
		qDebug("FeaturePresetsDialog::on_presetLoad_clicked: item null");
		updatePresetControls();
		return;
	}

	const FeatureSetPreset* preset = qvariant_cast<const FeatureSetPreset*>(item->data(0, Qt::UserRole));

	if (!preset)
	{
		qDebug("FeatureSetPreset::on_presetLoad_clicked: preset null");
		return;
	}

	loadPresetSettings(preset);
}

void FeaturePresetsDialog::on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    (void) current;
    (void) previous;
	updatePresetControls();
}

void FeaturePresetsDialog::on_presetTree_itemActivated(QTreeWidgetItem *item, int column)
{
    (void) item;
    (void) column;
	on_presetLoad_clicked();
}

FeatureSetPreset* FeaturePresetsDialog::newFeatureSetPreset(const QString& group, const QString& description)
{
	FeatureSetPreset* preset = new FeatureSetPreset();
	preset->setGroup(group);
	preset->setDescription(description);
	addFeatureSetPreset(preset);
	return preset;
}

void FeaturePresetsDialog::addFeatureSetPreset(FeatureSetPreset *preset)
{
    m_featureSetPresets->append(preset);
}

void FeaturePresetsDialog::savePresetSettings(FeatureSetPreset* preset)
{
	qDebug("FeaturePresetsDialog::savePresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    preset->clearFeatures();
    m_featureUISet->saveFeatureSetSettings(preset);
}

void FeaturePresetsDialog::loadPresetSettings(const FeatureSetPreset* preset)
{
	qDebug("FeaturePresetsDialog::loadPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    m_featureUISet->loadFeatureSetSettings(preset, m_pluginAPI, m_apiAdapter, m_workspaces, m_currentWorkspace);
    m_presetLoaded = true;
}

void FeaturePresetsDialog::sortFeatureSetPresets()
{
    std::sort(m_featureSetPresets->begin(), m_featureSetPresets->end(), FeatureSetPreset::presetCompare);
}

void FeaturePresetsDialog::renamePresetGroup(const QString& oldGroupName, const QString& newGroupName)
{
    for (int i = 0; i < m_featureSetPresets->size(); i++)
    {
        if (m_featureSetPresets->at(i)->getGroup() == oldGroupName)
        {
            FeatureSetPreset *preset_mod = const_cast<FeatureSetPreset*>(m_featureSetPresets->at(i));
            preset_mod->setGroup(newGroupName);
        }
    }
}

void FeaturePresetsDialog::deletePreset(const FeatureSetPreset* preset)
{
	m_featureSetPresets->removeAll((FeatureSetPreset*) preset);
	delete (FeatureSetPreset*) preset;
}

void FeaturePresetsDialog::deletePresetGroup(const QString& groupName)
{
    QList<FeatureSetPreset*>::iterator it = m_featureSetPresets->begin();

    while (it != m_featureSetPresets->end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_featureSetPresets->erase(it);
        } else {
            ++it;
        }
    }
}
