///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QStringListModel>

#include "workspaceselectiondialog.h"
#include "ui_workspaceselectiondialog.h"

WorkspaceSelectionDialog::WorkspaceSelectionDialog(int numberOfWorkspaces, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WorkspaceSelectionDialog),
    m_numberOfWorkspaces(numberOfWorkspaces),
    m_hasChanged(false)
{
    ui->setupUi(this);

    for (int i = 0; i < m_numberOfWorkspaces; i++) {
        ui->workspaceList->addItem(tr("W:%1").arg(i));
    }
}

WorkspaceSelectionDialog::~WorkspaceSelectionDialog()
{
    delete ui;
}

void WorkspaceSelectionDialog::accept()
{
    m_selectedRow = ui->workspaceList->currentRow();
    m_hasChanged = true;
    QDialog::accept();
}
