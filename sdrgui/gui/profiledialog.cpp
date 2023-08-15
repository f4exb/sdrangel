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

#include <QAbstractButton>

#include "profiledialog.h"
#include "ui_profiledialog.h"
#include "gui/nanosecondsdelegate.h"
#include "util/profiler.h"

ProfileDialog::ProfileDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ProfileDialog)
{
    ui->setupUi(this);
    connect(&m_timer, &QTimer::timeout, this, &ProfileDialog::updateData);
    resizeTable();
    m_timer.start(500);
}

ProfileDialog::~ProfileDialog()
{
    delete ui;
}

void ProfileDialog::accept()
{
    QDialog::accept();
}

void ProfileDialog::clicked(QAbstractButton *button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::ButtonRole::ResetRole)
    {
        ui->table->setRowCount(0);
        GlobalProfileData::resetProfileData();
    }
}

void ProfileDialog::resizeTable()
{
    int row = ui->table->rowCount();
    ui->table->setRowCount(row + 1);
    ui->table->setItem(row, COL_NAME, new QTableWidgetItem("Random-SDR[0] Spectrum @12345678910"));
    ui->table->setItem(row, COL_TOTAL, new QTableWidgetItem("1000.000 ms"));
    ui->table->setItem(row, COL_AVERAGE, new QTableWidgetItem("1000.000 ns/frame"));
    ui->table->setItem(row, COL_LAST, new QTableWidgetItem("1000.000 ms"));
    ui->table->setItem(row, COL_NUM_SAMPLES, new QTableWidgetItem("1000000000"));
    ui->table->resizeColumnsToContents();
    ui->table->setRowCount(row);
}

// Update table with latest profile data
void ProfileDialog::updateData()
{
    QHash<QString, ProfileData>& profileData = GlobalProfileData::getProfileData();

    QHashIterator<QString, ProfileData> itr(profileData);

    while (itr.hasNext())
    {
        itr.next();
        QString key = itr.key();
        const ProfileData& data = itr.value();
        double totalTime = data.getTotal();
        double averageTime = data.getAverage();
        double lastTime = data.getLast();

        int i = 0;
        for (; i < ui->table->rowCount(); i++)
        {
            QString name = ui->table->item(i, COL_NAME)->text();
            if (name == key)
            {
                // Update existing row
                ui->table->item(i, COL_TOTAL)->setData(Qt::DisplayRole, totalTime);
                ui->table->item(i, COL_AVERAGE)->setData(Qt::DisplayRole, averageTime);
                ui->table->item(i, COL_LAST)->setData(Qt::DisplayRole, lastTime);
                ui->table->item(i, COL_NUM_SAMPLES)->setData(Qt::DisplayRole, data.getNumSamples());
                break;
            }
        }
        if (i >= ui->table->rowCount())
        {
            // Add new row
            ui->table->setSortingEnabled(false);
            int row = ui->table->rowCount();
            ui->table->setRowCount(row + 1);

            QTableWidgetItem *name = new QTableWidgetItem(key);
            QTableWidgetItem *total = new QTableWidgetItem();
            QTableWidgetItem *average = new QTableWidgetItem();
            QTableWidgetItem *last = new QTableWidgetItem();
            QTableWidgetItem *numSamples = new QTableWidgetItem();

            ui->table->setItem(row, COL_NAME, name);
            ui->table->setItem(row, COL_TOTAL, total);
            ui->table->setItem(row, COL_AVERAGE, average);
            ui->table->setItem(row, COL_LAST, last);
            ui->table->setItem(row, COL_NUM_SAMPLES, numSamples);

            total->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            average->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            last->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            numSamples->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            total->setData(Qt::DisplayRole, totalTime);
            average->setData(Qt::DisplayRole, averageTime);
            last->setData(Qt::DisplayRole, lastTime);
            numSamples->setData(Qt::DisplayRole, data.getNumSamples());

            ui->table->setItemDelegateForColumn(COL_TOTAL, new NanoSecondsDelegate());
            ui->table->setItemDelegateForColumn(COL_AVERAGE, new NanoSecondsDelegate());
            ui->table->setItemDelegateForColumn(COL_LAST, new NanoSecondsDelegate());

            ui->table->setSortingEnabled(true);
        }
    }

    GlobalProfileData::releaseProfileData();
}
