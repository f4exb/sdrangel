#ifndef INCLUDE_AUDIODIALOG_H
#define INCLUDE_AUDIODIALOG_H

#include <QDialog>

#include "export.h"
#include "audio/audiodevicemanager.h"

class QTreeWidgetItem;

namespace Ui {
	class AudioDialog;
}

class SDRGUI_API AudioDialogX : public QDialog {
	Q_OBJECT

public:
	explicit AudioDialogX(AudioDeviceManager* audioDeviceManager, QWidget* parent = 0);
	~AudioDialogX();

	int m_inIndex;
	int m_outIndex;

private:
	void updateInputDisplay();
	void updateOutputDisplay();
	void updateInputDeviceInfo();
	void updateOutputDeviceInfo();

	Ui::AudioDialog* ui;

	AudioDeviceManager* m_audioDeviceManager;
	AudioDeviceManager::InputDeviceInfo m_inputDeviceInfo;
	AudioDeviceManager::OutputDeviceInfo m_outputDeviceInfo;
	quint16 m_outputUDPPort;

private slots:
	void accept();
	void reject();
    void on_audioInTree_currentItemChanged(QTreeWidgetItem* currentItem, QTreeWidgetItem* previousItem);
	void on_audioOutTree_currentItemChanged(QTreeWidgetItem* currentItem, QTreeWidgetItem* previousItem);
	void on_inputVolume_valueChanged(int value);
    void on_inputReset_clicked(bool checked);
    void on_inputCleanup_clicked(bool checked);
    void on_outputUDPPort_editingFinished();
    void on_outputReset_clicked(bool checked);
    void on_outputCleanup_clicked(bool checked);
};

#endif // INCLUDE_AUDIODIALOG_H
