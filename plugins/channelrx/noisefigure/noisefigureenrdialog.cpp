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

#include "noisefigureenrdialog.h"

NoiseFigureENRDialog::NoiseFigureENRDialog(NoiseFigureSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::NoiseFigureENRDialog)
{
    ui->setupUi(this);
    ui->enr->sortByColumn(0, Qt::AscendingOrder);
    for (int i = 0; i < m_settings->m_enr.size(); i++) {
        addRow( m_settings->m_enr[i]->m_frequency, m_settings->m_enr[i]->m_enr);
    }
}

NoiseFigureENRDialog::~NoiseFigureENRDialog()
{
    delete ui;
}

void NoiseFigureENRDialog::accept()
{
    QDialog::accept();
    qDeleteAll(m_settings->m_enr);
    m_settings->m_enr.clear();
    ui->enr->sortByColumn(0, Qt::AscendingOrder);
    for (int i = 0; i < ui->enr->rowCount(); i++)
    {
        QTableWidgetItem *freqItem = ui->enr->item(i, ENR_COL_FREQ);
        QTableWidgetItem *enrItem =  ui->enr->item(i, ENR_COL_ENR);
        double freqValue = freqItem->data(Qt::DisplayRole).toDouble();
        double enrValue = enrItem->data(Qt::DisplayRole).toDouble();

        NoiseFigureSettings::ENR *enr = new NoiseFigureSettings::ENR(freqValue, enrValue);
        m_settings->m_enr.append(enr);
    }
}

void NoiseFigureENRDialog::addRow(double freq, double enr)
{
    ui->enr->setSortingEnabled(false);
    int row = ui->enr->rowCount();
    ui->enr->setRowCount(row + 1);
    QTableWidgetItem *freqItem = new QTableWidgetItem();
    QTableWidgetItem *enrItem = new QTableWidgetItem();
    ui->enr->setItem(row, ENR_COL_FREQ, freqItem);
    ui->enr->setItem(row, ENR_COL_ENR, enrItem);
    freqItem->setData(Qt::DisplayRole, freq);
    enrItem->setData(Qt::DisplayRole, enr);
    ui->enr->setSortingEnabled(true);
}

void NoiseFigureENRDialog::on_addRow_clicked()
{
    addRow(0.0, 0.0);
}

void NoiseFigureENRDialog::on_deleteRow_clicked()
{
    QModelIndexList indexList = ui->enr->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->enr->removeRow(row);
    }
}
