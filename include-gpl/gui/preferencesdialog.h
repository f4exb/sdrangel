#ifndef INCLUDE_PREFERENCESDIALOG_H
#define INCLUDE_PREFERENCESDIALOG_H

#include <QDialog>

class AudioDeviceInfo;

namespace Ui {
	class PreferencesDialog;
}

class PreferencesDialog : public QDialog {
	Q_OBJECT

public:
	explicit PreferencesDialog(AudioDeviceInfo* audioDeviceInfo, QWidget* parent = NULL);
	~PreferencesDialog();

private:
	enum Audio {
		ATDefault,
		ATInterface,
		ATDevice
	};

	Ui::PreferencesDialog* ui;

	AudioDeviceInfo* m_audioDeviceInfo;

private slots:
	void accept();
};

#endif // INCLUDE_PREFERENCESDIALOG_H
