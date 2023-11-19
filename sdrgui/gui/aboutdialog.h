///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
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
#ifndef INCLUDE_ABOUTDIALOG_H
#define INCLUDE_ABOUTDIALOG_H

#include <QDialog>

#include "export.h"

class MainSettings;

namespace Ui {
	class AboutDialog;
}

class SDRGUI_API AboutDialog : public QDialog {
	Q_OBJECT

public:
	explicit AboutDialog(const QString& apiHost, int apiPort, const MainSettings& mainSettings, QWidget* parent = 0);
	~AboutDialog();

private:
	Ui::AboutDialog* ui;
};

#endif // INCLUDE_ABOUTDIALOG_H
