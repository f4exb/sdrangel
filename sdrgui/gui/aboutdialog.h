#ifndef INCLUDE_ABOUTDIALOG_H
#define INCLUDE_ABOUTDIALOG_H

#include <QDialog>

#include "util/export.h"

namespace Ui {
	class AboutDialog;
}

class SDRGUI_API AboutDialog : public QDialog {
	Q_OBJECT

public:
	explicit AboutDialog(const QString& apiHost, int apiPort, QWidget* parent = 0);
	~AboutDialog();

private:
	Ui::AboutDialog* ui;
};

#endif // INCLUDE_ABOUTDIALOG_H
