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

#include "aptdemodsettingsdialog.h"

APTDemodSettingsDialog::APTDemodSettingsDialog(APTDemodSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::APTDemodSettingsDialog)
{
    ui->setupUi(this);
    ui->satelliteTrackerControl->setChecked(settings->m_satelliteTrackerControl);
    ui->satellite->setCurrentText(settings->m_satelliteName);
    ui->autoSave->setChecked(settings->m_autoSave);
    ui->autoSavePath->setText(settings->m_autoSavePath);
    ui->minScanlines->setValue(settings->m_autoSaveMinScanLines);
}

APTDemodSettingsDialog::~APTDemodSettingsDialog()
{
    delete ui;
}

void APTDemodSettingsDialog::accept()
{
    m_settings->m_satelliteTrackerControl = ui->satelliteTrackerControl->isChecked();
    m_settings->m_satelliteName = ui->satellite->currentText();
    m_settings->m_autoSave = ui->autoSave->isChecked();
    m_settings->m_autoSavePath = ui->autoSavePath->text();
    m_settings->m_autoSaveMinScanLines = ui->minScanlines->value();
    QDialog::accept();
}

void APTDemodSettingsDialog::on_autoSavePathBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select directory to save images to", "",
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    ui->autoSavePath->setText(dir);
}
