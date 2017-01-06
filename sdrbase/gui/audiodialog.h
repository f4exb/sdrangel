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
	Ui::AudioDialog* ui;

	AudioDeviceInfo* m_audioDeviceInfo;
	float m_inputVolume;

private slots:
	void accept();
	void on_inputVolume_valueChanged(int value);
};

#endif // INCLUDE_AUDIODIALOG_H
