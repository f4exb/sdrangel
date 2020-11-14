///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRGUI_GUI_AMBEDEVICESDIALOG_H_
#define SDRGUI_GUI_AMBEDEVICESDIALOG_H_

#include <QDialog>

#include "export.h"
#include "ambe/ambeengine.h"

class QListWidgetItem;

namespace Ui {
    class AMBEDevicesDialog;
}

class SDRGUI_API AMBEDevicesDialog : public QDialog {
    Q_OBJECT

public:
    explicit AMBEDevicesDialog(AMBEEngine* ambeEngine, QWidget* parent = nullptr);
    ~AMBEDevicesDialog();

private:
    void populateSerialList();
    void refreshInUseList();

    Ui::AMBEDevicesDialog* ui;
    AMBEEngine* m_ambeEngine;

private slots:
    void on_importSerial_clicked();
    void on_importAllSerial_clicked();
    void on_removeAmbeDevice_clicked();
    void on_refreshAmbeList_clicked();
    void on_clearAmbeList_clicked();
    void on_importAddress_clicked();
};

#endif // SDRGUI_GUI_AMBEDEVICESDIALOG_H_
