///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QComboBox>

#include "aircraftnotificationdialog.h"
#include "gui/fastitemdelegate.h"

AircraftNotificationDialog::AircraftNotificationDialog(AircraftSettings *settings,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::AircraftNotificationDialog),
    m_settings(settings)
{
    ui->setupUi(this);

    // Cells painted through QStyle cost a SystemParametersInfo call each with the Fusion
    // style - 28 to 93 us depending on the machine. See FastItemDelegate
    ui->table->setItemDelegate(new FastItemDelegate(ui->table));


    resizeTable();

    for (int i = 0; i < m_settings->m_notificationSettings.size(); i++) {
        addRow(m_settings->m_notificationSettings[i].data());
    }
}

AircraftNotificationDialog::~AircraftNotificationDialog()
{
    delete ui;
}

void AircraftNotificationDialog::accept()
{
    m_settings->m_notificationSettings.clear();
    for (int i = 0; i < ui->table->rowCount(); i++)
    {
        QSharedPointer<AircraftSettings::NotificationSettings> notificationSettings = QSharedPointer<AircraftSettings::NotificationSettings>::create();
        notificationSettings->m_matchColumn = ((QComboBox *)ui->table->cellWidget(i, NOTIFICATION_COL_MATCH))->currentIndex();
        notificationSettings->m_regExp = ui->table->item(i, NOTIFICATION_COL_REG_EXP)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_speech = ui->table->item(i, NOTIFICATION_COL_SPEECH)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_command = ui->table->item(i, NOTIFICATION_COL_COMMAND)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->updateRegularExpression();
        m_settings->m_notificationSettings.append(notificationSettings);
    }
    QDialog::accept();
}

void AircraftNotificationDialog::resizeTable()
{
    AircraftSettings::NotificationSettings dummy;
    dummy.m_matchColumn = AircraftSettings::MATCH_FLIGHT;
    dummy.m_regExp = "A regular expression";
    dummy.m_speech = "${flight} ${type} detected on ${protocol}";
    dummy.m_command = "/usr/home/sdrangel/myscript ${flight}";
    addRow(&dummy);
    ui->table->resizeColumnsToContents();
    ui->table->selectRow(0);
    on_remove_clicked();
    ui->table->selectRow(-1);
}

void AircraftNotificationDialog::on_add_clicked()
{
    addRow();
}

// Remove selected row
void AircraftNotificationDialog::on_remove_clicked()
{
    // Selection mode is single, so only a single row should be returned
    QModelIndexList indexList = ui->table->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->table->removeRow(row);
    }
}

void AircraftNotificationDialog::addRow(AircraftSettings::NotificationSettings *settings)
{
    QComboBox *match = new QComboBox();

    // In the same order as enum NotificationMatch
    match->addItem("ICAO ID");
    match->addItem("Reg");
    match->addItem("Flight");
    match->addItem("Type");

    QTableWidgetItem *regExpItem = new QTableWidgetItem();
    QTableWidgetItem *speechItem = new QTableWidgetItem();
    QTableWidgetItem *commandItem = new QTableWidgetItem();

    if (settings != nullptr)
    {
        match->setCurrentIndex(settings->m_matchColumn);
        regExpItem->setData(Qt::DisplayRole, settings->m_regExp);
        speechItem->setData(Qt::DisplayRole, settings->m_speech);
        commandItem->setData(Qt::DisplayRole, settings->m_command);
    }
    else
    {
        match->setCurrentIndex(AircraftSettings::MATCH_FLIGHT);
        regExpItem->setData(Qt::DisplayRole, ".*");
        speechItem->setData(Qt::DisplayRole, "${flight} detected");
    }

    ui->table->setSortingEnabled(false);
    int row = ui->table->rowCount();
    ui->table->setRowCount(row + 1);
    ui->table->setCellWidget(row, NOTIFICATION_COL_MATCH, match);
    ui->table->setItem(row, NOTIFICATION_COL_REG_EXP, regExpItem);
    ui->table->setItem(row, NOTIFICATION_COL_SPEECH, speechItem);
    ui->table->setItem(row, NOTIFICATION_COL_COMMAND, commandItem);
    ui->table->setSortingEnabled(true);
}
