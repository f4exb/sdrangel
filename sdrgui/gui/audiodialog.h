#ifndef INCLUDE_AUDIODIALOG_H
#define INCLUDE_AUDIODIALOG_H

#include <QDialog>

#include "export.h"

class AudioDeviceManager;

namespace Ui {
	class AudioDialog;
}

class SDRGUI_API AudioDialog : public QDialog {
	Q_OBJECT

public:
	explicit AudioDialog(AudioDeviceManager* audioDeviceInfo, QWidget* parent = NULL);
	~AudioDialog();

private:
	Ui::AudioDialog* ui;

	AudioDeviceManager* m_audioDeviceInfo;
	float m_inputVolume;

private slots:
	void accept();
	void on_inputVolume_valueChanged(int value);
};

#endif // INCLUDE_AUDIODIALOG_H
