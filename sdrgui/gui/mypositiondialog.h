///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#ifndef SDRBASE_GUI_MYPOSITIONDIALOG_H_
#define SDRBASE_GUI_MYPOSITIONDIALOG_H_

#include <QDialog>
#include "settings/mainsettings.h"

namespace Ui {
	class MyPositionDialog;
}

class MyPositionDialog : public QDialog {
	Q_OBJECT

public:
	explicit MyPositionDialog(MainSettings& mainSettings, QWidget* parent = 0);
	~MyPositionDialog();

private:
	Ui::MyPositionDialog* ui;
	MainSettings& m_mainSettings;

private slots:
	void accept();
};

#endif /* SDRBASE_GUI_MYPOSITIONDIALOG_H_ */
