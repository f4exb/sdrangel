#ifndef INCLUDE_ADDPRESETDIALOG_H
#define INCLUDE_ADDPRESETDIALOG_H

#include <QDialog>

namespace Ui {
	class AddPresetDialog;
}

class AddPresetDialog : public QDialog {
	Q_OBJECT

public:
	explicit AddPresetDialog(const QStringList& groups, const QString& group, QWidget* parent = NULL);
	~AddPresetDialog();

	QString group() const;
	QString description() const;

private:
	enum Audio {
		ATDefault,
		ATInterface,
		ATDevice
	};

	Ui::AddPresetDialog* ui;
};

#endif // INCLUDE_ADDPRESETDIALOG_H
