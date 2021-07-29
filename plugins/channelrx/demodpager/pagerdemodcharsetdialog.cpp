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

#include "pagerdemodcharsetdialog.h"

PagerDemodCharsetDialog::PagerDemodCharsetDialog(PagerDemodSettings *settings,
        QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::PagerDemodCharsetDialog)
{
    ui->setupUi(this);
    if (settings->m_sevenbit.size() > 0) {
        ui->preset->setCurrentIndex(2); // User
    }
    ui->reverse->setChecked(settings->m_reverse);
    for (int i = 0; i < settings->m_sevenbit.size(); i++) {
        addRow(settings->m_sevenbit[i], settings->m_unicode[i]);
    }
    connect(ui->table, &QTableWidget::cellChanged, this, &PagerDemodCharsetDialog::on_table_cellChanged);
}

PagerDemodCharsetDialog::~PagerDemodCharsetDialog()
{
    delete ui;
}

void PagerDemodCharsetDialog::accept()
{
    m_settings->m_sevenbit.clear();
    m_settings->m_unicode.clear();
    for (int i = 0; i < ui->table->rowCount(); i++)
    {
        int sevenbit = ui->table->item(i, SEVENBIT_COL)->data(Qt::DisplayRole).toString().toInt(nullptr, 16);
        int unicode = ui->table->item(i, UNICODE_COL)->data(Qt::DisplayRole).toString().toInt(nullptr, 16);
        m_settings->m_sevenbit.append(sevenbit);
        m_settings->m_unicode.append(unicode);
    }
    m_settings->m_reverse = ui->reverse->isChecked();
    QDialog::accept();
}

void PagerDemodCharsetDialog::on_add_clicked()
{
    addRow(0, 0);
}

void PagerDemodCharsetDialog::on_remove_clicked()
{
    QModelIndexList indexList = ui->table->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->table->removeRow(row);
    }
}

void PagerDemodCharsetDialog::on_preset_currentIndexChanged(int index)
{
    ui->table->setRowCount(0);
    ui->reverse->setChecked(false);
    if (index == 1)
    {
        // Hebrew
        for (int i = 0; i < 22+5; i++) {
            addRow(96 + i, 0x05D0 + i);
        }
        // Even though Hebrew is right-to-left, it seems characters are
        // transmitted last first, so no need to check reverse
    }
}

void PagerDemodCharsetDialog::addRow(int sevenBit, int unicode)
{
    ui->table->setSortingEnabled(false);
    ui->table->blockSignals(true);
    int row = ui->table->rowCount();
    ui->table->setRowCount(row + 1);
    QTableWidgetItem *sevenbitItem = new QTableWidgetItem();
    QTableWidgetItem *unicodeItem = new QTableWidgetItem();
    QTableWidgetItem *glyphItem = new QTableWidgetItem();
    ui->table->setItem(row, SEVENBIT_COL, sevenbitItem);
    ui->table->setItem(row, UNICODE_COL, unicodeItem);
    ui->table->setItem(row, GLYPH_COL, glyphItem);
    sevenbitItem->setFlags(Qt::ItemIsEditable | sevenbitItem->flags());
    sevenbitItem->setData(Qt::DisplayRole, QString::number(sevenBit, 16));
    unicodeItem->setFlags(Qt::ItemIsEditable | unicodeItem->flags());
    unicodeItem->setData(Qt::DisplayRole, QString::number(unicode, 16));
    glyphItem->setFlags(glyphItem->flags() & ~Qt::ItemIsEditable);
    glyphItem->setData(Qt::DisplayRole, QChar(unicode));
    ui->table->blockSignals(false);
    ui->table->setSortingEnabled(true);
}

void PagerDemodCharsetDialog::on_table_cellChanged(int row, int column)
{
    if (column == UNICODE_COL)
    {
        // Update glyph to match entered unicode code point
        int unicode = ui->table->item(row, UNICODE_COL)->data(Qt::DisplayRole).toString().toInt(nullptr, 16);
        ui->table->item(row, GLYPH_COL)->setData(Qt::DisplayRole, QChar(unicode));
    }
}
