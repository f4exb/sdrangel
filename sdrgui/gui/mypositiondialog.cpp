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

#include "gui/mypositiondialog.h"
#include "ui_myposdialog.h"


MyPositionDialog::MyPositionDialog(MainSettings& mainSettings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::MyPositionDialog),
	m_mainSettings(mainSettings)
{
	ui->setupUi(this);
    ui->latitudeSpinBox->setValue(m_mainSettings.getLatitude());
    ui->longitudeSpinBox->setValue(m_mainSettings.getLongitude());
}

MyPositionDialog::~MyPositionDialog()
{
	delete ui;
}

void MyPositionDialog::accept()
{
    m_mainSettings.setLatitude(ui->latitudeSpinBox->value());
    m_mainSettings.setLongitude(ui->longitudeSpinBox->value());
	QDialog::accept();
}
