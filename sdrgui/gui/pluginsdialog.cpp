///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015, 2017, 2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>       //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "gui/pluginsdialog.h"
#include "mainwindow.h"
#include "ui_pluginsdialog.h"

PluginsDialog::PluginsDialog(PluginManager* pluginManager, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::PluginsDialog)
{
	ui->setupUi(this);
	const PluginManager::Plugins& plugins = pluginManager->getPlugins();

    for (PluginManager::Plugins::const_iterator it = plugins.constBegin(); it != plugins.constEnd(); ++it)
    {
		QStringList sl;
		const PluginDescriptor& desc = it->pluginInterface->getPluginDescriptor();
		sl.append(desc.displayedName);
		sl.append(desc.version);

        if (desc.licenseIsGPL) {
			sl.append(tr("YES"));
        } else {
            sl.append("no");
        }

		QTreeWidgetItem* pluginItem = new QTreeWidgetItem(ui->tree, sl);
		sl.clear();
		sl.append(tr("Copyright: %1").arg(desc.copyright));
		QTreeWidgetItem* item = new QTreeWidgetItem(pluginItem, sl);
		item->setFirstColumnSpanned(true);
		sl.clear();
		sl.append(tr("Website: %1").arg(desc.website));
		item = new QTreeWidgetItem(pluginItem, sl);
		item->setFirstColumnSpanned(true);
		sl.clear();
		sl.append(tr("Source Code: %1").arg(desc.sourceCodeURL));
		item = new QTreeWidgetItem(pluginItem, sl);
		item->setFirstColumnSpanned(true);
        sl.clear();
        sl.append(tr("Hardware ID: %1").arg(desc.hardwareId));
		item = new QTreeWidgetItem(pluginItem, sl);
		item->setFirstColumnSpanned(true);
	}

    ui->tree->resizeColumnToContents(0);
	ui->tree->resizeColumnToContents(1);
	ui->tree->resizeColumnToContents(2);
}

PluginsDialog::~PluginsDialog()
{
	delete ui;
}
