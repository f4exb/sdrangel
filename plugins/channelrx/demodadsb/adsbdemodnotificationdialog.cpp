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
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>

#include "adsbdemodnotificationdialog.h"

// Map main ADS-B table column numbers to combo box indicies
std::vector<int> ADSBDemodNotificationDialog::m_columnMap = {
    ADSB_COL_ICAO, ADSB_COL_CALLSIGN, ADSB_COL_MODEL,
    ADSB_COL_ALTITUDE, ADSB_COL_SPEED, ADSB_COL_RANGE,
    ADSB_COL_CATEGORY, ADSB_COL_STATUS, ADSB_COL_SQUAWK,
    ADSB_COL_REGISTRATION, ADSB_COL_MANUFACTURER, ADSB_COL_OWNER, ADSB_COL_OPERATOR_ICAO
};

ADSBDemodNotificationDialog::ADSBDemodNotificationDialog(ADSBDemodSettings *settings,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ADSBDemodNotificationDialog),
    m_settings(settings)
{
    ui->setupUi(this);

    resizeTable();

    for (int i = 0; i < m_settings->m_notificationSettings.size(); i++) {
        addRow(m_settings->m_notificationSettings[i]);
    }
}

ADSBDemodNotificationDialog::~ADSBDemodNotificationDialog()
{
    delete ui;
}

void ADSBDemodNotificationDialog::accept()
{
    qDeleteAll(m_settings->m_notificationSettings);
    m_settings->m_notificationSettings.clear();
    for (int i = 0; i < ui->table->rowCount(); i++)
    {
        ADSBDemodSettings::NotificationSettings *notificationSettings = new ADSBDemodSettings::NotificationSettings();
        int idx = ((QComboBox *)ui->table->cellWidget(i, NOTIFICATION_COL_MATCH))->currentIndex();
        notificationSettings->m_matchColumn = m_columnMap[idx];
        notificationSettings->m_regExp = ui->table->item(i, NOTIFICATION_COL_REG_EXP)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_speech = ui->table->item(i, NOTIFICATION_COL_SPEECH)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_command = ui->table->item(i, NOTIFICATION_COL_COMMAND)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_autoTarget = ((QCheckBox *) ui->table->cellWidget(i, NOTIFICATION_COL_AUTOTARGET))->isChecked();
        notificationSettings->updateRegularExpression();
        m_settings->m_notificationSettings.append(notificationSettings);
    }
    QDialog::accept();
}

void ADSBDemodNotificationDialog::resizeTable()
{
    ADSBDemodSettings::NotificationSettings dummy;
    dummy.m_matchColumn = ADSB_COL_MANUFACTURER;
    dummy.m_regExp = "No emergency and some";
    dummy.m_speech = "${aircraft} ${reg} has entered your airspace";
    dummy.m_command = "/usr/home/sdrangel/myscript ${aircraft} ${reg}";
    dummy.m_autoTarget = false;
    addRow(&dummy);
    ui->table->resizeColumnsToContents();
    ui->table->selectRow(0);
    on_remove_clicked();
    ui->table->selectRow(-1);
}

void ADSBDemodNotificationDialog::on_add_clicked()
{
    addRow();
}

// Remove selected row
void ADSBDemodNotificationDialog::on_remove_clicked()
{
    // Selection mode is single, so only a single row should be returned
    QModelIndexList indexList = ui->table->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->table->removeRow(row);
    }
}

void ADSBDemodNotificationDialog::addRow(ADSBDemodSettings::NotificationSettings *settings)
{
    QComboBox *match = new QComboBox();
    QCheckBox *autoTarget = new QCheckBox();
    autoTarget->setChecked(false);
    QWidget *matchWidget = new QWidget();
    QHBoxLayout *pLayout = new QHBoxLayout(matchWidget);
    pLayout->addWidget(match);
    pLayout->setAlignment(Qt::AlignCenter);
    pLayout->setContentsMargins(0, 0, 0, 0);
    matchWidget->setLayout(pLayout);

    match->addItem("ICAO ID");
    match->addItem("Callsign");
    match->addItem("Aircraft");
    match->addItem("Alt (ft)");
    match->addItem("Spd (kn)");
    match->addItem("D (km)");
    match->addItem("Cat");
    match->addItem("Status");
    match->addItem("Squawk");
    match->addItem("Reg");
    match->addItem("Manufacturer");
    match->addItem("Owner");
    match->addItem("Operator");

    QTableWidgetItem *regExpItem = new QTableWidgetItem();
    QTableWidgetItem *speechItem = new QTableWidgetItem();
    QTableWidgetItem *commandItem = new QTableWidgetItem();

    if (settings != nullptr)
    {
        for (unsigned int i = 0; i < m_columnMap.size(); i++)
        {
            if (m_columnMap[i] == settings->m_matchColumn)
            {
                match->setCurrentIndex(i);
                break;
            }
        }
        regExpItem->setData(Qt::DisplayRole, settings->m_regExp);
        speechItem->setData(Qt::DisplayRole, settings->m_speech);
        commandItem->setData(Qt::DisplayRole, settings->m_command);
    }
    else
    {
         match->setCurrentIndex(2);
        regExpItem->setData(Qt::DisplayRole, ".*");
        speechItem->setData(Qt::DisplayRole, "${aircraft} detected");
    }

    ui->table->setSortingEnabled(false);
    int row = ui->table->rowCount();
    ui->table->setRowCount(row + 1);
    ui->table->setCellWidget(row, NOTIFICATION_COL_MATCH, match);
    ui->table->setItem(row, NOTIFICATION_COL_REG_EXP, regExpItem);
    ui->table->setItem(row, NOTIFICATION_COL_SPEECH, speechItem);
    ui->table->setItem(row, NOTIFICATION_COL_COMMAND, commandItem);
    ui->table->setCellWidget(row, NOTIFICATION_COL_AUTOTARGET, autoTarget);
    ui->table->setSortingEnabled(true);
}

