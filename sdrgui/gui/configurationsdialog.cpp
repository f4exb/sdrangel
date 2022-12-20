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

#include <QMessageBox>
#include <QDebug>
#include <QFileDialog>

#include "gui/addpresetdialog.h"
#include "maincore.h"
#include "configurationsdialog.h"
#include "ui_configurationsdialog.h"

#include "settings/configuration.h"

ConfigurationsDialog::ConfigurationsDialog(bool openOnly, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ConfigurationsDialog)
{
    ui->setupUi(this);
    if (openOnly)
    {
        ui->buttonBox->setStandardButtons(QDialogButtonBox::Close | QDialogButtonBox::Open);
        ui->configurationDelete->setVisible(false);
        ui->configurationEdit->setVisible(false);
        ui->configurationExport->setVisible(false);
        ui->configurationImport->setVisible(false);
        ui->configurationLoad->setVisible(false);
        ui->configurationSave->setVisible(false);
        ui->configurationUpdate->setVisible(false);
    }
    else
    {
        ui->description->setVisible(false);
    }
}

ConfigurationsDialog::~ConfigurationsDialog()
{
    delete ui;
}

void ConfigurationsDialog::populateTree()
{
    if (!m_configurations) {
        return;
    }

    QList<Configuration*>::const_iterator it = m_configurations->begin();
    int middleIndex = m_configurations->size() / 2;
    QTreeWidgetItem *treeItem;
    ui->configurationsTree->clear();

    for (int i = 0; it != m_configurations->end(); ++it, i++)
    {
        treeItem = addConfigurationToTree(*it);

        if (i == middleIndex) {
            ui->configurationsTree->setCurrentItem(treeItem);
        }
    }

    updateConfigurationControls();
}

QTreeWidgetItem* ConfigurationsDialog::addConfigurationToTree(const Configuration* configuration)
{
	QTreeWidgetItem* group = nullptr;

	for (int i = 0; i < ui->configurationsTree->topLevelItemCount(); i++)
	{
		if (ui->configurationsTree->topLevelItem(i)->text(0) == configuration->getGroup())
		{
			group = ui->configurationsTree->topLevelItem(i);
			break;
		}
	}

	if (!group)
	{
		QStringList sl;
		sl.append(configuration->getGroup());
		group = new QTreeWidgetItem(ui->configurationsTree, sl, PGroup);
		group->setFirstColumnSpanned(true);
		group->setExpanded(true);
		ui->configurationsTree->sortByColumn(0, Qt::AscendingOrder);
	}

	QStringList sl;
	sl.append(configuration->getDescription());
    QTreeWidgetItem* item = new QTreeWidgetItem(group, sl, PItem); // description column
	item->setTextAlignment(0, Qt::AlignLeft);
	item->setData(0, Qt::UserRole, QVariant::fromValue(configuration));

	updateConfigurationControls();
	return item;
}

void ConfigurationsDialog::updateConfigurationControls()
{
	ui->configurationsTree->resizeColumnToContents(0);

	if (ui->configurationsTree->currentItem())
	{
		ui->configurationDelete->setEnabled(true);
		ui->configurationLoad->setEnabled(true);
	}
	else
	{
		ui->configurationDelete->setEnabled(false);
		ui->configurationLoad->setEnabled(false);
	}
}

void ConfigurationsDialog::on_configurationSave_clicked()
{
    QStringList groups;
    QString group;
    QString description = "";

    for (int i = 0; i < ui->configurationsTree->topLevelItemCount(); i++) {
        groups.append(ui->configurationsTree->topLevelItem(i)->text(0));
    }

    QTreeWidgetItem* item = ui->configurationsTree->currentItem();

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
        Configuration* configuration = newConfiguration(dlg.group(), dlg.description());
        emit saveConfiguration(configuration);
        ui->configurationsTree->setCurrentItem(addConfigurationToTree(configuration));
    }

    sortConfigurations();
}

void ConfigurationsDialog::on_configurationUpdate_clicked()
{
	QTreeWidgetItem* item = ui->configurationsTree->currentItem();
	const Configuration* changedConfiguration = nullptr;

	if (item)
	{
		if( item->type() == PItem)
		{
			const Configuration* configuration = qvariant_cast<const Configuration*>(item->data(0, Qt::UserRole));

			if (configuration)
			{
				Configuration* configuration_mod = const_cast<Configuration*>(configuration);
				emit saveConfiguration(configuration_mod);
				changedConfiguration = configuration;
			}
		}
	}

	sortConfigurations();
    ui->configurationsTree->clear();

    for (int i = 0; i < m_configurations->size(); ++i)
    {
        QTreeWidgetItem *item_x = addConfigurationToTree(m_configurations->at(i));
        const Configuration* configuration_x = qvariant_cast<const Configuration*>(item_x->data(0, Qt::UserRole));

        if (changedConfiguration &&  (configuration_x == changedConfiguration)) { // set cursor on changed configuration
            ui->configurationsTree->setCurrentItem(item_x);
        }
    }
}

void ConfigurationsDialog::on_configurationEdit_clicked()
{
    QTreeWidgetItem* item = ui->configurationsTree->currentItem();
    QStringList groups;
    bool change = false;
    const Configuration *changedConfiguration = nullptr;
    QString newGroupName;

    for (int i = 0; i < ui->configurationsTree->topLevelItemCount(); i++) {
        groups.append(ui->configurationsTree->topLevelItem(i)->text(0));
    }

    if (item)
    {
        if (item->type() == PItem)
        {
            const Configuration* configuration = qvariant_cast<const Configuration*>(item->data(0, Qt::UserRole));
            AddPresetDialog dlg(groups, configuration->getGroup(), this);
            dlg.setDescription(configuration->getDescription());

            if (dlg.exec() == QDialog::Accepted)
            {
                Configuration* configuration_mod = const_cast<Configuration*>(configuration);
                configuration_mod->setGroup(dlg.group());
                configuration_mod->setDescription(dlg.description());
                change = true;
                changedConfiguration = configuration;
            }
        }
        else if (item->type() == PGroup)
        {
            AddPresetDialog dlg(groups, item->text(0), this);
            dlg.showGroupOnly();
            dlg.setDialogTitle("Edit configuration group");

            if (dlg.exec() == QDialog::Accepted)
            {
                renameConfigurationGroup(item->text(0), dlg.group());
                newGroupName = dlg.group();
                change = true;
            }
        }
    }

    if (change)
    {
        sortConfigurations();
        ui->configurationsTree->clear();

        for (int i = 0; i < m_configurations->size(); ++i)
        {
            QTreeWidgetItem *item_x = addConfigurationToTree(m_configurations->at(i));
            const Configuration* configuration_x = qvariant_cast<const Configuration*>(item_x->data(0, Qt::UserRole));

            if (changedConfiguration && (configuration_x == changedConfiguration)) { // set cursor on changed configuration
                ui->configurationsTree->setCurrentItem(item_x);
            }
        }

        if (!changedConfiguration) // on group name change set cursor on the group that has been changed
        {
            for(int i = 0; i < ui->configurationsTree->topLevelItemCount(); i++)
            {
                QTreeWidgetItem* item = ui->configurationsTree->topLevelItem(i);

                if (item->text(0) == newGroupName) {
                    ui->configurationsTree->setCurrentItem(item);
                }
            }
        }
    }
}

void ConfigurationsDialog::on_configurationDelete_clicked()
{
	QTreeWidgetItem* item = ui->configurationsTree->currentItem();

	if (item == 0)
	{
		updateConfigurationControls();
		return;
	}
	else
	{
        if (item->type() == PItem)
        {
            const Configuration* configuration = qvariant_cast<const Configuration*>(item->data(0, Qt::UserRole));

            if (configuration)
            {
                if (
                    QMessageBox::question(
                        this,
                        tr("Delete Configuration"),
                        tr("Do you want to delete configuration '%1'?").arg(configuration->getDescription()),
                        QMessageBox::No | QMessageBox::Yes,
                        QMessageBox::No
                    ) == QMessageBox::Yes
                )
                {
                    delete item;
                    deleteConfiguration(configuration);
                }
            }
        }
        else if (item->type() == PGroup)
        {
            if (
                QMessageBox::question(
                    this,
                    tr("Delete configuration group"),
                    tr("Do you want to delete configuration group '%1'?").arg(item->text(0)),
                    QMessageBox::No | QMessageBox::Yes,
                    QMessageBox::No
                ) == QMessageBox::Yes
            )
            {
                deleteConfigurationGroup(item->text(0));

                ui->configurationsTree->clear();

                for (int i = 0; i < m_configurations->size(); ++i) {
                    addConfigurationToTree(m_configurations->at(i));
                }
            }
        }
	}
}

void ConfigurationsDialog::on_configurationLoad_clicked()
{
	qDebug() << "ConfigurationsDialog::on_configurationLoad_clicked";

	QTreeWidgetItem* item = ui->configurationsTree->currentItem();

	if (!item)
	{
		qDebug("ConfigurationsDialog::on_configurationLoad_clicked: item null");
		updateConfigurationControls();
		return;
	}

	const Configuration* configuration = qvariant_cast<const Configuration*>(item->data(0, Qt::UserRole));

	if (!configuration)
	{
		qDebug("ConfigurationsDialog::on_configurationLoad_clicked: configuration null");
		return;
	}

	emit loadConfiguration(configuration);
}

void ConfigurationsDialog::on_configurationExport_clicked()
{
	QTreeWidgetItem* item = ui->configurationsTree->currentItem();

	if (item)
    {
		if (item->type() == PItem)
		{
			const Configuration* configuration = qvariant_cast<const Configuration*>(item->data(0, Qt::UserRole));
			QString base64Str = configuration->serialize().toBase64();
			QString fileName = QFileDialog::getSaveFileName(
                this,
			    tr("Open preset export file"),
                ".",
                tr("Configuration export files (*.cfgx)"),
                0);

			if (fileName != "")
			{
				QFileInfo fileInfo(fileName);

				if (fileInfo.suffix() != "cfgx") {
					fileName += ".cfgx";
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

void ConfigurationsDialog::on_configurationImport_clicked()
{
	QTreeWidgetItem* item = ui->configurationsTree->currentItem();

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
            tr("Preset export files (*.cfgx)"),
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

				Configuration* configuration = MainCore::instance()->m_settings.newConfiguration("", "");
				configuration->deserialize(QByteArray::fromBase64(base64Str));
				configuration->setGroup(group); // override with current group

				ui->configurationsTree->setCurrentItem(addConfigurationToTree(configuration));
			}
			else
			{
				QMessageBox::information(this, tr("Message"), tr("Cannot open file for reading"));
			}
		}
	}
}

void ConfigurationsDialog::on_configurationTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    (void) current;
    (void) previous;
	updateConfigurationControls();
}

void ConfigurationsDialog::on_configurationTree_itemActivated(QTreeWidgetItem *item, int column)
{
    (void) item;
    (void) column;
	on_configurationLoad_clicked();
}

Configuration* ConfigurationsDialog::newConfiguration(const QString& group, const QString& description)
{
	Configuration* configuration = new Configuration();
	configuration->setGroup(group);
	configuration->setDescription(description);
	addConfiguration(configuration);
	return configuration;
}

void ConfigurationsDialog::addConfiguration(Configuration *configuration)
{
    m_configurations->append(configuration);
}

void ConfigurationsDialog::sortConfigurations()
{
    std::sort(m_configurations->begin(), m_configurations->end(), Configuration::configCompare);
}

void ConfigurationsDialog::renameConfigurationGroup(const QString& oldGroupName, const QString& newGroupName)
{
    for (int i = 0; i < m_configurations->size(); i++)
    {
        if (m_configurations->at(i)->getGroup() == oldGroupName)
        {
            Configuration *configuration_mod = const_cast<Configuration*>(m_configurations->at(i));
            configuration_mod->setGroup(newGroupName);
        }
    }
}

void ConfigurationsDialog::deleteConfiguration(const Configuration* configuration)
{
	m_configurations->removeAll((Configuration*) configuration);
	delete (Configuration*) configuration;
}

void ConfigurationsDialog::deleteConfigurationGroup(const QString& groupName)
{
    QList<Configuration*>::iterator it = m_configurations->begin();

    while (it != m_configurations->end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_configurations->erase(it);
        } else {
            ++it;
        }
    }
}

void ConfigurationsDialog::accept()
{
    on_configurationLoad_clicked();
    QDialog::accept();
}
