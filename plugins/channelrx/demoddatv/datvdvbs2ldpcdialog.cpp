///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019, 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QFileDialog>

#include "datvdvbs2ldpcdialog.h"
#include "datvdemodsettings.h"
#include "ui_datvdvbs2ldpcdialog.h"

DatvDvbS2LdpcDialog::DatvDvbS2LdpcDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::DatvDvbS2LdpcDialog)
{
    ui->setupUi(this);
}

DatvDvbS2LdpcDialog::~DatvDvbS2LdpcDialog()
{
    delete ui;
}

void DatvDvbS2LdpcDialog::accept()
{
    QDialog::accept();
}

void DatvDvbS2LdpcDialog::setFileName(const QString& fileName)
{
    m_fileName = fileName;
    ui->ldpcToolText->setText(m_fileName);
}

void DatvDvbS2LdpcDialog::setMaxTrials(int maxTrials)
{
    m_maxTrials = maxTrials < 1 ? 1 :
        maxTrials > DATVDemodSettings::m_softLDPCMaxMaxTrials ? DATVDemodSettings::m_softLDPCMaxMaxTrials : maxTrials;
    ui->maxTrials->setValue(m_maxTrials);
}

void DatvDvbS2LdpcDialog::on_showFileDialog_clicked(bool checked)
{
    (void) checked;

    QFileDialog fileDialog(this, "Select LDPC tool");
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);
#ifdef _MSC_VER
    fileDialog.setNameFilter("*.exe");
#else
    fileDialog.setFilter(QDir::Executable | QDir::Files);
#endif
    fileDialog.selectFile(m_fileName);

    if (fileDialog.exec() == QDialog::Accepted)
    {
        QStringList fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            m_fileName = fileNames[0];
            ui->ldpcToolText->setText(m_fileName);
        }
    }
}

void DatvDvbS2LdpcDialog::on_maxTrials_valueChanged(int value)
{
    m_maxTrials = value;
}

