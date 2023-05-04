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
    int idx;
    ui->setupUi(this);
    ui->satelliteTrackerControl->setChecked(settings->m_satelliteTrackerControl);
    ui->satellite->setCurrentText(settings->m_satelliteName);
    ui->autoSave->setChecked(settings->m_autoSave);
    ui->saveCombined->setChecked(settings->m_saveCombined);
    ui->saveSeparate->setChecked(settings->m_saveSeparate);
    ui->saveProjection->setChecked(settings->m_saveProjection);
    ui->autoSavePath->setText(settings->m_autoSavePath);
    ui->minScanlines->setValue(settings->m_autoSaveMinScanLines);
    ui->scanlinesPerImageUpdate->setValue(settings->m_scanlinesPerImageUpdate);
    idx = ui->horizontalPixelsPerDegree->findText(QString::number(settings->m_horizontalPixelsPerDegree));
    ui->horizontalPixelsPerDegree->setCurrentIndex(idx);
    idx = ui->verticalPixelsPerDegree->findText(QString::number(settings->m_verticalPixelsPerDegree));
    ui->verticalPixelsPerDegree->setCurrentIndex(idx);
    ui->satTimeOffset->setValue(settings->m_satTimeOffset);
    ui->satYaw->setValue(settings->m_satYaw);
    for (auto file : settings->m_palettes) {
        ui->palettes->addItem(file);
    }
    on_autoSave_clicked(settings->m_autoSave);
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
    m_settings->m_saveCombined = ui->saveCombined->isChecked();
    m_settings->m_saveSeparate = ui->saveSeparate->isChecked();
    m_settings->m_saveProjection = ui->saveProjection->isChecked();
    m_settings->m_autoSavePath = ui->autoSavePath->text();
    m_settings->m_autoSaveMinScanLines = ui->minScanlines->value();
    m_settings->m_scanlinesPerImageUpdate = ui->scanlinesPerImageUpdate->value();
    m_settings->m_palettes.clear();
    m_settings->m_horizontalPixelsPerDegree = ui->horizontalPixelsPerDegree->currentText().toInt();
    m_settings->m_verticalPixelsPerDegree = ui->verticalPixelsPerDegree->currentText().toInt();
    m_settings->m_satTimeOffset = ui->satTimeOffset->value();
    m_settings->m_satYaw = ui->satYaw->value();
    for (int i = 0; i < ui->palettes->count(); i++) {
        m_settings->m_palettes.append(ui->palettes->item(i)->text());
    }
    QDialog::accept();
}

void APTDemodSettingsDialog::on_autoSavePathBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select directory to save images to", "",
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    ui->autoSavePath->setText(dir);
}

void APTDemodSettingsDialog::on_autoSave_clicked(bool checked)
{
    (void) checked;
    /* Commented out until theme greys out disabled widgets
    ui->saveProjectionLabel->setEnabled(checked);
    ui->saveCombined->setEnabled(checked);
    ui->saveSeparate->setEnabled(checked);
    ui->saveProjection->setEnabled(checked);
    ui->autoSavePathLabel->setEnabled(checked);
    ui->autoSavePath->setEnabled(checked);
    ui->autoSavePathBrowse->setEnabled(checked);
    ui->minScanlinesLabel->setEnabled(checked);
    ui->minScanlines->setEnabled(checked);
    */
}

void APTDemodSettingsDialog::on_addPalette_clicked()
{
    QFileDialog fileDialog(nullptr, "Select palette files", "", "*.png *.bmp");
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        for (auto fileName : fileNames) {
            ui->palettes->addItem(fileName);
        }
    }
}

void APTDemodSettingsDialog::on_removePalette_clicked()
{
    QList<QListWidgetItem *> items = ui->palettes->selectedItems();
    for (auto item : items)
    {
        ui->palettes->removeItemWidget(item);
        delete item;
    }
}
