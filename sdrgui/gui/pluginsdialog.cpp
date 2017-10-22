#include "gui/pluginsdialog.h"
#include "mainwindow.h"
#include "ui_pluginsdialog.h"

PluginsDialog::PluginsDialog(PluginManager* pluginManager, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::PluginsDialog)
{
	ui->setupUi(this);

	const PluginManager::Plugins& plugins = pluginManager->getPlugins();
	for(PluginManager::Plugins::const_iterator it = plugins.constBegin(); it != plugins.constEnd(); ++it) {
		QStringList sl;
		const PluginDescriptor& desc = it->pluginInterface->getPluginDescriptor();
		sl.append(desc.displayedName);
		sl.append(desc.version);
		if(desc.licenseIsGPL)
			sl.append(tr("YES"));
		else sl.append("no");
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
	}
	ui->tree->resizeColumnToContents(0);
	ui->tree->resizeColumnToContents(1);
	ui->tree->resizeColumnToContents(2);
}

PluginsDialog::~PluginsDialog()
{
	delete ui;
}
