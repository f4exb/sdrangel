///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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
#include <QScrollBar>
#include <QLineEdit>

#include "ft8demodsettings.h"
#include "ft8demodsettingsdialog.h"

FT8DemodSettingsDialog::FT8DemodSettingsDialog(FT8DemodSettings& settings, QStringList& settingsKeys, QWidget* parent ) :
    QDialog(parent),
    ui(new Ui::FT8DemodSettingsDialog),
    m_settings(settings),
    m_settingsKeys(settingsKeys)
{
    ui->setupUi(this);
    ui->decoderNbThreads->setValue(m_settings.m_nbDecoderThreads);
    ui->decoderTimeBudget->setValue(m_settings.m_decoderTimeBudget);
    ui->osdEnable->setChecked(m_settings.m_useOSD);
    ui->osdDepth->setValue(m_settings.m_osdDepth);
    ui->osdDepthText->setText(tr("%1").arg(m_settings.m_osdDepth));
    ui->osdLDPCThreshold->setValue(m_settings.m_osdLDPCThreshold);
    ui->osdLDPCThresholdText->setText(tr("%1").arg(m_settings.m_osdLDPCThreshold));
    ui->verifyOSD->setChecked(m_settings.m_verifyOSD);
    resizeBandsTable();
    populateBandsTable();
    connect(ui->bands, &QTableWidget::cellChanged, this, &FT8DemodSettingsDialog::textCellChanged);
}

FT8DemodSettingsDialog::~FT8DemodSettingsDialog()
{
}

void FT8DemodSettingsDialog::resizeBandsTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->bands->rowCount();
    ui->bands->setRowCount(row + 1);
    ui->bands->setItem(row, BAND_NAME, new QTableWidgetItem("123456789012345"));
    ui->bands->setItem(row, BAND_BASE_FREQUENCY, new QTableWidgetItem("10000000"));
    ui->bands->setItem(row, BAND_OFFSET_FREQUENCY, new QTableWidgetItem("-1000"));
    ui->bands->resizeColumnsToContents();
    ui->bands->removeRow(row);
}

void FT8DemodSettingsDialog::populateBandsTable()
{
    // Add to messages table
    int row = ui->bands->rowCount();

    for (const auto& band : m_settings.m_bandPresets)
    {
        ui->bands->setRowCount(row + 1);

        QTableWidgetItem *nameItem = new QTableWidgetItem();
        QTableWidgetItem *baseFrequencyItem = new QTableWidgetItem();
        QTableWidgetItem *offsetFrequencyItem = new QTableWidgetItem();

        ui->bands->setItem(row, BAND_NAME, nameItem);
        ui->bands->setItem(row, BAND_BASE_FREQUENCY, baseFrequencyItem);
        ui->bands->setItem(row, BAND_OFFSET_FREQUENCY, offsetFrequencyItem);

        nameItem->setText(band.m_name);
        QLineEdit *editBaseFrequency = new QLineEdit(ui->bands);
        editBaseFrequency->setValidator(new QIntValidator());
        editBaseFrequency->setText(tr("%1").arg(band.m_baseFrequency));
        editBaseFrequency->setAlignment(Qt::AlignRight);
        editBaseFrequency->setProperty("row", row);
        ui->bands->setCellWidget(row, BAND_BASE_FREQUENCY, editBaseFrequency);
        QLineEdit *editOffsetFrequency = new QLineEdit(ui->bands);
        editOffsetFrequency->setValidator(new QIntValidator());
        editOffsetFrequency->setText(tr("%1").arg(band.m_channelOffset));
        editOffsetFrequency->setAlignment(Qt::AlignRight);
        editOffsetFrequency->setProperty("row", row);
        ui->bands->setCellWidget(row, BAND_OFFSET_FREQUENCY, editOffsetFrequency);

        connect(editBaseFrequency, &QLineEdit::editingFinished, this, &FT8DemodSettingsDialog::baseFrequencyCellChanged);
        connect(editOffsetFrequency, &QLineEdit::editingFinished, this, &FT8DemodSettingsDialog::offsetFrequencyCellChanged);

        row++;
    }
}

void FT8DemodSettingsDialog::accept()
{
    QDialog::accept();
}

void FT8DemodSettingsDialog::reject()
{
    m_settingsKeys.clear();
    QDialog::reject();
}

void FT8DemodSettingsDialog::on_decoderNbThreads_valueChanged(int value)
{
    m_settings.m_nbDecoderThreads = value;

    if (!m_settingsKeys.contains("nbDecoderThreads")) {
        m_settingsKeys.append("nbDecoderThreads");
    }
}

void FT8DemodSettingsDialog::on_decoderTimeBudget_valueChanged(double value)
{
    m_settings.m_decoderTimeBudget = value;

    if (!m_settingsKeys.contains("decoderTimeBudget")) {
        m_settingsKeys.append("decoderTimeBudget");
    }
}

void FT8DemodSettingsDialog::on_osdEnable_toggled(bool checked)
{
    m_settings.m_useOSD = checked;

    if (!m_settingsKeys.contains("useOSD")) {
        m_settingsKeys.append("useOSD");
    }
}

void FT8DemodSettingsDialog::on_osdDepth_valueChanged(int value)
{
    m_settings.m_osdDepth = value;
    ui->osdDepthText->setText(tr("%1").arg(m_settings.m_osdDepth));

    if (!m_settingsKeys.contains("osdDepth")) {
        m_settingsKeys.append("osdDepth");
    }
}

void FT8DemodSettingsDialog::on_osdLDPCThreshold_valueChanged(int value)
{
    m_settings.m_osdLDPCThreshold = value;
    ui->osdLDPCThresholdText->setText(tr("%1").arg(m_settings.m_osdLDPCThreshold));

    if (!m_settingsKeys.contains("osdLDPCThreshold")) {
        m_settingsKeys.append("osdLDPCThreshold");
    }
}

void FT8DemodSettingsDialog::on_verifyOSD_stateChanged(int state)
{
    m_settings.m_verifyOSD = state == Qt::Checked;

    if (!m_settingsKeys.contains("verifyOSD")) {
        m_settingsKeys.append("verifyOSD");
    }
}

void FT8DemodSettingsDialog::on_addBand_clicked()
{
    int currentRow = ui->bands->currentRow();
    const QTableWidgetItem *currentNameItem = ui->bands->item(currentRow, BAND_NAME);
    FT8DemodBandPreset newPreset({"", 0, 0});

    if (currentNameItem) {
        newPreset.m_name = currentNameItem->text();
    }

    QLineEdit *currentEditBaseFrequency = qobject_cast<QLineEdit*>(ui->bands->cellWidget(currentRow, BAND_BASE_FREQUENCY));

    if (currentEditBaseFrequency) {
        newPreset.m_baseFrequency = currentEditBaseFrequency->text().toInt();
    }

    QLineEdit *currentEditOffsetFrequency = qobject_cast<QLineEdit*>(ui->bands->cellWidget(currentRow, BAND_OFFSET_FREQUENCY));

    if (currentEditOffsetFrequency) {
        newPreset.m_channelOffset = currentEditOffsetFrequency->text().toInt();
    }

    m_settings.m_bandPresets.push_back(newPreset);
    ui->bands->blockSignals(true);
    ui->bands->setRowCount(0);
    populateBandsTable();
    ui->bands->scrollToBottom();
    ui->bands->blockSignals(false);

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

void FT8DemodSettingsDialog::on_deleteBand_clicked()
{
    int currentRow = ui->bands->currentRow();

    if (currentRow < 0) {
        return;
    }

    m_settings.m_bandPresets.removeAt(currentRow);
    ui->bands->removeRow(currentRow);

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

void FT8DemodSettingsDialog::on_moveBandUp_clicked()
{
    int currentRow = ui->bands->currentRow();

    if (currentRow < 1) {
        return;
    }

    ui->bands->blockSignals(true);
    QList<QTableWidgetItem*> sourceItems = takeRow(currentRow);
    QList<QTableWidgetItem*> destItems = takeRow(currentRow-1);
    setRow(currentRow, destItems);
    setRow(currentRow-1, sourceItems);
    ui->bands->blockSignals(false);

    const auto sourceBandPreset = m_settings.m_bandPresets[currentRow];
    const auto destBandPreset = m_settings.m_bandPresets[currentRow-1];
    m_settings.m_bandPresets[currentRow] = destBandPreset;
    m_settings.m_bandPresets[currentRow-1] = sourceBandPreset;

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

void FT8DemodSettingsDialog::on_moveBandDown_clicked()
{
    int currentRow = ui->bands->currentRow();

    if (currentRow > ui->bands->rowCount()-2) {
        return;
    }

    ui->bands->blockSignals(true);
    QList<QTableWidgetItem*> sourceItems = takeRow(currentRow);
    QList<QTableWidgetItem*> destItems = takeRow(currentRow+1);
    setRow(currentRow, destItems);
    setRow(currentRow+1, sourceItems);
    ui->bands->blockSignals(false);

    const auto sourceBandPreset = m_settings.m_bandPresets[currentRow];
    const auto destBandPreset = m_settings.m_bandPresets[currentRow+1];
    m_settings.m_bandPresets[currentRow] = destBandPreset;
    m_settings.m_bandPresets[currentRow+1] = sourceBandPreset;

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

void FT8DemodSettingsDialog::on_restoreBandPresets_clicked()
{
    m_settings.resetBandPresets();
    ui->bands->blockSignals(true);
    ui->bands->setRowCount(0);
    populateBandsTable();
    ui->bands->blockSignals(false);

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

void FT8DemodSettingsDialog::textCellChanged(int row, int col)
{
    if (col == BAND_NAME) {
        m_settings.m_bandPresets[row].m_name = ui->bands->item(row, col)->text();
    }

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

void FT8DemodSettingsDialog::baseFrequencyCellChanged()
{
    QLineEdit *editBaseFrequency = qobject_cast<QLineEdit*>(sender());

    if (editBaseFrequency)
    {
        int row = editBaseFrequency->property("row").toInt();
        m_settings.m_bandPresets[row].m_baseFrequency = editBaseFrequency->text().toInt();
    }

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

void FT8DemodSettingsDialog::offsetFrequencyCellChanged()
{
    QLineEdit *editOffsetFrequency = qobject_cast<QLineEdit*>(sender());

    if (editOffsetFrequency)
    {
        int row = editOffsetFrequency->property("row").toInt();
        m_settings.m_bandPresets[row].m_channelOffset = editOffsetFrequency->text().toInt();
    }

    if (!m_settingsKeys.contains("bandPresets")) {
        m_settingsKeys.append("bandPresets");
    }
}

QList<QTableWidgetItem*> FT8DemodSettingsDialog::takeRow(int row)
{
	QList<QTableWidgetItem*> rowItems;

	for (int col = 0; col < ui->bands->columnCount(); ++col) {
		rowItems << ui->bands->takeItem(row, col);
	}

    return rowItems;
}

void FT8DemodSettingsDialog::setRow(int row, const QList<QTableWidgetItem*>& rowItems)
{
	for (int col = 0; col < ui->bands->columnCount(); ++col) {
		ui->bands->setItem(row, col, rowItems.at(col));
	}
}
