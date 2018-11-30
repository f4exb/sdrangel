///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRGUI_GUI_AUDIOSELECTDIALOG_H_
#define SDRGUI_GUI_AUDIOSELECTDIALOG_H_

#include <QDialog>

#include "export.h"
#include "audio/audiodevicemanager.h"

class QTreeWidgetItem;

namespace Ui {
    class AudioSelectDialog;
}

class SDRGUI_API AudioSelectDialog : public QDialog {
    Q_OBJECT
public:
    explicit AudioSelectDialog(AudioDeviceManager* audioDeviceManager, const QString& deviceName, bool input = false, QWidget* parent = 0);
    ~AudioSelectDialog();

    QString m_audioDeviceName;
    bool m_selected;

private:
    bool getDeviceInfos(bool input, const QString& deviceName, bool& systemDefault, int& sampleRate);
    Ui::AudioSelectDialog* ui;
    AudioDeviceManager* m_audioDeviceManager;
    bool m_input;

private slots:
    void accept();
    void reject();
    //void on_audioInTree_currentItemChanged(QTreeWidgetItem* currentItem, QTreeWidgetItem* previousItem);

};

#endif /* SDRGUI_GUI_AUDIOSELECTDIALOG_H_ */
