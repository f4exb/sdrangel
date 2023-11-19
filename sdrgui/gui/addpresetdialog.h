///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
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
#ifndef INCLUDE_ADDPRESETDIALOG_H
#define INCLUDE_ADDPRESETDIALOG_H

#include <QDialog>

#include "export.h"

namespace Ui {
	class AddPresetDialog;
}

class SDRGUI_API AddPresetDialog : public QDialog {
	Q_OBJECT

public:
	explicit AddPresetDialog(const QStringList& groups, const QString& group, QWidget* parent = NULL);
	~AddPresetDialog();

	QString group() const;
	QString description() const;
	void setGroup(const QString& group);
	void setDescription(const QString& description);
	void showGroupOnly();
	void setDialogTitle(const QString& title);
	void setDescriptionBoxTitle(const QString& title);

private:
	enum Audio {
		ATDefault,
		ATInterface,
		ATDevice
	};

	Ui::AddPresetDialog* ui;
};

#endif // INCLUDE_ADDPRESETDIALOG_H
