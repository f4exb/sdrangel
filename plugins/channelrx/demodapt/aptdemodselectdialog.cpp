///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDebug>
#include <QFileDialog>

#include "aptdemodselectdialog.h"

APTDemodSelectDialog::APTDemodSelectDialog(const QStringList &list, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::APTDemodSelectDialog)
{
    ui->setupUi(this);
    for (auto item : list) {
        ui->list->addItem(item);
    }
}

APTDemodSelectDialog::~APTDemodSelectDialog()
{
    delete ui;
}

void APTDemodSelectDialog::accept()
{
    QList<QListWidgetItem *> items = ui->list->selectedItems();
    m_selected.clear();
    for (auto item : items)
    {
        m_selected.append(item->text());
    }
    QDialog::accept();
}
