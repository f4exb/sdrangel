///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RIGCTRLGUI_H
#define INCLUDE_RIGCTRLGUI_H

#include <QDialog>
#include "rigctrlsettings.h"
#include "rigctrl.h"

namespace Ui {
	class RigCtrlGUI;
}

class RigCtrlGUI : public QDialog {
	Q_OBJECT

public:
	explicit RigCtrlGUI(RigCtrl *rigCtrl, QWidget *parent = 0);
	~RigCtrlGUI();

private slots:
    void accept();

private:
	Ui::RigCtrlGUI* ui;
    RigCtrlSettings m_settings;
    RigCtrl *m_rigCtrl;
};

#endif // INCLUDE_RIGCTRLGUI_H
