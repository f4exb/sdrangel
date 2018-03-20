#ifndef INCLUDE_PLUGINSDIALOG_H
#define INCLUDE_PLUGINSDIALOG_H

#include <QDialog>
#include "plugin/pluginmanager.h"
#include "export.h"

namespace Ui {
	class PluginsDialog;
}

class SDRGUI_API PluginsDialog : public QDialog {
	Q_OBJECT

public:
	explicit PluginsDialog(PluginManager* pluginManager, QWidget* parent = NULL);
	~PluginsDialog();

private:
	Ui::PluginsDialog* ui;
};

#endif // INCLUDE_PLUGINSDIALOG_H
