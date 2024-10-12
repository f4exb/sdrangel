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
#include <QCheckBox>

#include "pagerdemodfilterdialog.h"

PagerDemodFilterDialog::PagerDemodFilterDialog(PagerDemodSettings *settings,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::PagerDemodFilterDialog),
    m_settings(settings)
{
    ui->setupUi(this);

    ui->matchLastOnly->setChecked(m_settings->m_duplicateMatchLastOnly);
    ui->matchMessageOnly->setChecked(m_settings->m_duplicateMatchMessageOnly);
}

PagerDemodFilterDialog::~PagerDemodFilterDialog()
{
    delete ui;
}

void PagerDemodFilterDialog::accept()
{
    m_settings->m_duplicateMatchLastOnly = ui->matchLastOnly->isChecked();
    m_settings->m_duplicateMatchMessageOnly = ui->matchMessageOnly->isChecked();

    QDialog::accept();
}
