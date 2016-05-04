#ifndef INCLUDE_AUDIODIALOG_H
#define INCLUDE_AUDIODIALOG_H

#include <QDialog>

class AudioDeviceInfo;

namespace Ui {
	class AudioDialog;
}

class AudioDialog : public QDialog {
	Q_OBJECT

public:
	explicit AudioDialog(AudioDeviceInfo* audioDeviceInfo, QWidget* parent = NULL);
	~AudioDialog();

private:
	enum Audio {
		ATDefault,
		ATInterface,
		ATDevice
	};

	Ui::AudioDialog* ui;

	AudioDeviceInfo* m_audioDeviceInfo;

private slots:
	void accept();
};

#endif // INCLUDE_AUDIODIALOG_H
