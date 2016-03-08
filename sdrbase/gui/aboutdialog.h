#ifndef INCLUDE_ABOUTDIALOG_H
#define INCLUDE_ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
	class AboutDialog;
}

class AboutDialog : public QDialog {
	Q_OBJECT

public:
	explicit AboutDialog(QWidget* parent = NULL);
	~AboutDialog();

private:
	Ui::AboutDialog* ui;
};

#endif // INCLUDE_ABOUTDIALOG_H
