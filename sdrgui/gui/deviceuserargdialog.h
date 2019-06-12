///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRGUI_GUI_DEVICEUSERARGDIALOG_H
#define SDRGUI_GUI_DEVICEUSERARGDIALOG_H

#include <QDialog>

#include "export.h"

class QTreeWidgetItem;
class DeviceEnumerator;

namespace Ui {
	class DeviceUserArgDialog;
}

class SDRGUI_API DeviceUserArgDialog : public QDialog {
	Q_OBJECT

public:
	explicit DeviceUserArgDialog(DeviceEnumerator* deviceEnumerator, QWidget* parent = 0);
	~DeviceUserArgDialog();

private:
	Ui::DeviceUserArgDialog* ui;
    DeviceEnumerator* m_deviceEnumerator;

private slots:
	void accept();
	void reject();
    void on_keySelect_currentIndexChanged(int index);
    void on_deleteKey_clicked(bool checked);
    void on_keyEdit_returnPressed();
    void on_addKey_clicked(bool checked);
    void on_refreshKeys_clicked(bool checked);
    void on_valueEdit_returnPressed();
};

#endif // SDRGUI_GUI_DEVICEUSERARGDIALOG_H