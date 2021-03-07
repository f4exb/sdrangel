#ifndef INCLUDE_ABOUTDIALOG_H
#define INCLUDE_ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
	class RecordInfoDialog;
}

class RecordInfoDialog : public QDialog {
	Q_OBJECT

public:
	explicit RecordInfoDialog(const QString& text, QWidget* parent = nullptr);
	~RecordInfoDialog();

private:
	Ui::RecordInfoDialog* ui;
};

#endif // INCLUDE_ABOUTDIALOG_H
