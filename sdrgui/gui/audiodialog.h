///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>        //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
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
	void updateOutputSDPString();

	Ui::AudioDialog* ui;

	AudioDeviceManager* m_audioDeviceManager;
	AudioDeviceManager::InputDeviceInfo m_inputDeviceInfo;
	AudioDeviceManager::OutputDeviceInfo m_outputDeviceInfo;
	quint16 m_outputUDPPort;

private slots:
	void accept();
	void reject();
	void check();
    void on_audioInTree_currentItemChanged(QTreeWidgetItem* currentItem, QTreeWidgetItem* previousItem);
	void on_audioOutTree_currentItemChanged(QTreeWidgetItem* currentItem, QTreeWidgetItem* previousItem);
	void on_inputVolume_valueChanged(int value);
    void on_inputReset_clicked(bool checked);
    void on_inputCleanup_clicked(bool checked);
    void on_outputUDPPort_editingFinished();
    void on_outputReset_clicked(bool checked);
    void on_outputCleanup_clicked(bool checked);
    void on_outputSampleRate_valueChanged(int value);
    void on_decimationFactor_currentIndexChanged(int index);
    void on_outputUDPChannelCodec_currentIndexChanged(int index);
    void on_outputUDPChannelMode_currentIndexChanged(int index);
	void on_record_toggled(bool checked);
    void on_showFileDialog_clicked(bool checked);
    void on_recordSilenceTime_valueChanged(int value);
};

#endif // INCLUDE_AUDIODIALOG_H
