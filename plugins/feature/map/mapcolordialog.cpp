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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDebug>

#include "mapcolordialog.h"

MapColorDialog::MapColorDialog(const QColor &initial, QWidget *parent) :
    QDialog(parent)
{
    m_colorDialog = new QColorDialog(initial);
    m_colorDialog->setWindowFlags(Qt::Widget);
    m_colorDialog->setOptions(QColorDialog::ShowAlphaChannel | QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog);
    QVBoxLayout *v = new QVBoxLayout(this);
    v->addWidget(m_colorDialog);
    QHBoxLayout *h = new QHBoxLayout();
    m_noColorButton = new QPushButton("No Color");
    m_cancelButton = new QPushButton("Cancel");
    m_okButton = new QPushButton("OK");
    h->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
    h->addWidget(m_noColorButton);
    h->addWidget(m_cancelButton);
    h->addWidget(m_okButton);
    v->addLayout(h);

    connect(m_noColorButton, &QPushButton::clicked, this, &MapColorDialog::noColorClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);

    m_noColorSelected = false;
}

QColor MapColorDialog::selectedColor() const
{
    return m_colorDialog->selectedColor();
}

bool MapColorDialog::noColorSelected() const
{
    return m_noColorSelected;
}

void MapColorDialog::accept()
{
    m_colorDialog->accept();
    QDialog::accept();
}

void MapColorDialog::noColorClicked()
{
    m_noColorSelected = true;
    accept();
}
