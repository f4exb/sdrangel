///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "settings/serializableinterface.h"
#include "settings/pluginpreset.h"
#include "maincore.h"

#include "pluginpresetsdialog.h"
#include "ui_pluginpresetsdialog.h"

PluginPresetsDialog::PluginPresetsDialog(const QString& pluginId, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::PluginPresetsDialog),
    m_pluginId(pluginId),
    m_pluginPresets(nullptr),
    m_serializableInterface(nullptr),
    m_presetLoaded(false)
{
    ui->setupUi(this);
}

PluginPresetsDialog::~PluginPresetsDialog()
{
    delete ui;
}

void PluginPresetsDialog::populateTree()
{
    if (!m_pluginPresets) {
        return;
    }

    QList<PluginPreset*>::const_iterator it = m_pluginPresets->begin();
    int middleIndex = m_pluginPresets->size() / 2;
    QTreeWidgetItem *treeItem;
    ui->presetsTree->clear();

    for (int i = 0; it != m_pluginPresets->end(); ++it, i++)
    {
        const PluginPreset *preset = *it;
        treeItem = addPresetToTree(preset);

        if (i == middleIndex) {
            ui->presetsTree->setCurrentItem(treeItem);
        }
    }

    updatePresetControls();
}

QTreeWidgetItem* PluginPresetsDialog::addPresetToTree(const PluginPreset* preset)
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
        // Only show group when it contains presets for this pluging
        if (preset->getPluginIdURI() != m_pluginId) {
            group->setHidden(true);
        }
    }
    else
    {
        if (preset->getPluginIdURI() == m_pluginId) {
            group->setHidden(false);
        }
    }

    QStringList sl;
    sl.append(preset->getDescription());
    QTreeWidgetItem* item = new QTreeWidgetItem(group, sl, PItem); // description column
    item->setTextAlignment(0, Qt::AlignLeft);
    item->setData(0, Qt::UserRole, QVariant::fromValue(preset));
    // Only show presets for this plugin
    if (preset->getPluginIdURI() != m_pluginId) {
        item->setHidden(true);
    }

    updatePresetControls();
    return item;
}

void PluginPresetsDialog::updatePresetControls()
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

void PluginPresetsDialog::on_presetSave_clicked()
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
        PluginPreset* preset = newPreset(dlg.group(), dlg.description());
        savePresetSettings(preset);
        ui->presetsTree->setCurrentItem(addPresetToTree(preset));
    }

    sortPresets();
}

void PluginPresetsDialog::on_presetUpdate_clicked()
{
    QTreeWidgetItem* item = ui->presetsTree->currentItem();
    const PluginPreset* changedPreset = nullptr;

    if (item)
    {
        if( item->type() == PItem)
        {
            const PluginPreset* preset = qvariant_cast<const PluginPreset*>(item->data(0, Qt::UserRole));

            if (preset)
            {
                PluginPreset* preset_mod = const_cast<PluginPreset*>(preset);
                savePresetSettings(preset_mod);
                changedPreset = preset;
            }
        }
    }

    sortPresets();
    ui->presetsTree->clear();

    for (int i = 0; i < m_pluginPresets->size(); ++i)
    {
        QTreeWidgetItem *item_x = addPresetToTree(m_pluginPresets->at(i));
        const PluginPreset* preset_x = qvariant_cast<const PluginPreset*>(item_x->data(0, Qt::UserRole));

        if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
            ui->presetsTree->setCurrentItem(item_x);
        }
    }
}

void PluginPresetsDialog::on_presetEdit_clicked()
{
    QTreeWidgetItem* item = ui->presetsTree->currentItem();
    QStringList groups;
    bool change = false;
    const PluginPreset *changedPreset = nullptr;
    QString newGroupName;

    for (int i = 0; i < ui->presetsTree->topLevelItemCount(); i++) {
        groups.append(ui->presetsTree->topLevelItem(i)->text(0));
    }

    if (item)
    {
        if (item->type() == PItem)
        {
            const PluginPreset* preset = qvariant_cast<const PluginPreset*>(item->data(0, Qt::UserRole));
            AddPresetDialog dlg(groups, preset->getGroup(), this);
            dlg.setDescription(preset->getDescription());

            if (dlg.exec() == QDialog::Accepted)
            {
                PluginPreset* preset_mod = const_cast<PluginPreset*>(preset);
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
        sortPresets();
        ui->presetsTree->clear();

        for (int i = 0; i < m_pluginPresets->size(); ++i)
        {
            QTreeWidgetItem *item_x = addPresetToTree(m_pluginPresets->at(i));
            const PluginPreset* preset_x = qvariant_cast<const PluginPreset*>(item_x->data(0, Qt::UserRole));

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

void PluginPresetsDialog::on_presetDelete_clicked()
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
            const PluginPreset* preset = qvariant_cast<const PluginPreset*>(item->data(0, Qt::UserRole));

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

                for (int i = 0; i < m_pluginPresets->size(); ++i) {
                    addPresetToTree(m_pluginPresets->at(i));
                }
            }
        }
    }
}

void PluginPresetsDialog::on_presetLoad_clicked()
{
    qDebug() << "PluginPresetsDialog::on_presetLoad_clicked";

    QTreeWidgetItem* item = ui->presetsTree->currentItem();

    if (!item)
    {
        qDebug("PluginPresetsDialog::on_presetLoad_clicked: item null");
        updatePresetControls();
        return;
    }

    const PluginPreset* preset = qvariant_cast<const PluginPreset*>(item->data(0, Qt::UserRole));

    if (!preset)
    {
        qDebug("PluginPresetsDialog::on_presetLoad_clicked: preset null");
        return;
    }

    loadPresetSettings(preset);
}

void PluginPresetsDialog::on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    (void) current;
    (void) previous;
    updatePresetControls();
}

void PluginPresetsDialog::on_presetTree_itemActivated(QTreeWidgetItem *item, int column)
{
    (void) item;
    (void) column;
    on_presetLoad_clicked();
}

PluginPreset* PluginPresetsDialog::newPreset(const QString& group, const QString& description)
{
    PluginPreset* preset = new PluginPreset();
    preset->setGroup(group);
    preset->setDescription(description);
    addPreset(preset);
    return preset;
}

void PluginPresetsDialog::addPreset(PluginPreset *preset)
{
    m_pluginPresets->append(preset);
}

void PluginPresetsDialog::savePresetSettings(PluginPreset* preset)
{
    qDebug("PluginPresetsDialog::savePresetSettings: preset [%s | %s]",
        qPrintable(preset->getGroup()),
        qPrintable(preset->getDescription()));

    preset->setConfig(m_pluginId, m_serializableInterface->serialize());
}

void PluginPresetsDialog::loadPresetSettings(const PluginPreset* preset)
{
    qDebug("PluginPresetsDialog::loadPresetSettings: preset [%s | %s]",
        qPrintable(preset->getGroup()),
        qPrintable(preset->getDescription()));

    m_serializableInterface->deserialize(preset->getConfig());

    m_presetLoaded = true;
}

void PluginPresetsDialog::sortPresets()
{
    std::sort(m_pluginPresets->begin(), m_pluginPresets->end(), PluginPreset::presetCompare);
}

void PluginPresetsDialog::renamePresetGroup(const QString& oldGroupName, const QString& newGroupName)
{
    for (int i = 0; i < m_pluginPresets->size(); i++)
    {
        if (m_pluginPresets->at(i)->getGroup() == oldGroupName)
        {
            PluginPreset *preset_mod = const_cast<PluginPreset*>(m_pluginPresets->at(i));
            preset_mod->setGroup(newGroupName);
        }
    }
}

void PluginPresetsDialog::deletePreset(const PluginPreset* preset)
{
    m_pluginPresets->removeAll((PluginPreset*) preset);
    delete (PluginPreset*) preset;
}

void PluginPresetsDialog::deletePresetGroup(const QString& groupName)
{
    QList<PluginPreset*>::iterator it = m_pluginPresets->begin();

    while (it != m_pluginPresets->end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_pluginPresets->erase(it);
        } else {
            ++it;
        }
    }
}
