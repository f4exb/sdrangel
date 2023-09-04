///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "rttymodtxsettingsdialog.h"

static QListWidgetItem* newItem(const QString& text)
{
    QListWidgetItem* item = new QListWidgetItem(text);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    return item;
}

RttyModTXSettingsDialog::RttyModTXSettingsDialog(RttyModSettings* settings, QWidget *parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::RttyModTXSettingsDialog)
{
    ui->setupUi(this);
    ui->prefixCRLF->setChecked(m_settings->m_prefixCRLF);
    ui->postfixCRLF->setChecked(m_settings->m_postfixCRLF);
    for (const auto& text : m_settings->m_predefinedTexts) {
        ui->predefinedText->addItem(newItem(text));
    }
    ui->pulseShaping->setChecked(m_settings->m_pulseShaping);
    ui->beta->setValue(m_settings->m_beta);
    ui->symbolSpan->setValue(m_settings->m_symbolSpan);
    ui->lpfTaps->setValue(m_settings->m_lpfTaps);
    ui->rfNoise->setChecked(m_settings->m_rfNoise);
}

RttyModTXSettingsDialog::~RttyModTXSettingsDialog()
{
    delete ui;
}

void RttyModTXSettingsDialog::accept()
{
    m_settings->m_prefixCRLF = ui->prefixCRLF->isChecked();
    m_settings->m_postfixCRLF = ui->postfixCRLF->isChecked();
    m_settings->m_predefinedTexts.clear();
    for (int i = 0; i < ui->predefinedText->count(); i++) {
        m_settings->m_predefinedTexts.append(ui->predefinedText->item(i)->text());
    }
    m_settings->m_pulseShaping = ui->pulseShaping->isChecked();
    m_settings->m_beta = ui->beta->value();
    m_settings->m_symbolSpan = ui->symbolSpan->value();
    m_settings->m_lpfTaps = ui->lpfTaps->value();
    m_settings->m_rfNoise = ui->rfNoise->isChecked();

    QDialog::accept();
}

void RttyModTXSettingsDialog::on_add_clicked()
{
    QListWidgetItem* item = newItem("...");
    ui->predefinedText->addItem(item);
    ui->predefinedText->setCurrentItem(item);
}

void RttyModTXSettingsDialog::on_remove_clicked()
{
    QList<QListWidgetItem*> items = ui->predefinedText->selectedItems();
    for (auto item : items) {
        delete ui->predefinedText->takeItem(ui->predefinedText->row(item));
    }
}

void RttyModTXSettingsDialog::on_up_clicked()
{
    QList<QListWidgetItem*> items = ui->predefinedText->selectedItems();
    for (auto item : items)
    {
        int row = ui->predefinedText->row(item);
        if (row > 0)
        {
            QListWidgetItem* item = ui->predefinedText->takeItem(row);
            ui->predefinedText->insertItem(row - 1, item);
            ui->predefinedText->setCurrentItem(item);
        }
    }
}

void RttyModTXSettingsDialog::on_down_clicked()
{
    QList<QListWidgetItem*> items = ui->predefinedText->selectedItems();
    for (auto item : items)
    {
        int row = ui->predefinedText->row(item);
        if (row < ui->predefinedText->count() - 1)
        {
            QListWidgetItem* item = ui->predefinedText->takeItem(row);
            ui->predefinedText->insertItem(row + 1, item);
            ui->predefinedText->setCurrentItem(item);
        }
    }
}
