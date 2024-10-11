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

#include "gui/tablecolorchooser.h"

#include "pagerdemodnotificationdialog.h"
#include "pagerdemodgui.h"

// Map main table column numbers to combo box indices
std::vector<int> PagerDemodNotificationDialog::m_columnMap = {
    PagerDemodSettings::MESSAGE_COL_ADDRESS, PagerDemodSettings::MESSAGE_COL_MESSAGE
};

PagerDemodNotificationDialog::PagerDemodNotificationDialog(PagerDemodSettings *settings,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::PagerDemodNotificationDialog),
    m_settings(settings)
{
    ui->setupUi(this);

    resizeTable();

    for (int i = 0; i < m_settings->m_notificationSettings.size(); i++) {
        addRow(m_settings->m_notificationSettings[i]);
    }
}

PagerDemodNotificationDialog::~PagerDemodNotificationDialog()
{
    delete ui;
    qDeleteAll(m_colorGUIs);
}

void PagerDemodNotificationDialog::accept()
{
    qDeleteAll(m_settings->m_notificationSettings);
    m_settings->m_notificationSettings.clear();
    for (int i = 0; i < ui->table->rowCount(); i++)
    {
        PagerDemodSettings::NotificationSettings *notificationSettings = new PagerDemodSettings::NotificationSettings();
        int idx = ((QComboBox *)ui->table->cellWidget(i, NOTIFICATION_COL_MATCH))->currentIndex();
        notificationSettings->m_matchColumn = m_columnMap[idx];
        notificationSettings->m_regExp = ui->table->item(i, NOTIFICATION_COL_REG_EXP)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_speech = ui->table->item(i, NOTIFICATION_COL_SPEECH)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_command = ui->table->item(i, NOTIFICATION_COL_COMMAND)->data(Qt::DisplayRole).toString().trimmed();
        notificationSettings->m_highlight = !m_colorGUIs[i]->m_noColor;
        notificationSettings->m_highlightColor = m_colorGUIs[i]->m_color;
        notificationSettings->m_plotOnMap = ((QCheckBox *) ui->table->cellWidget(i, NOTIFICATION_COL_PLOT_ON_MAP))->isChecked();
        notificationSettings->updateRegularExpression();
        m_settings->m_notificationSettings.append(notificationSettings);
    }
    QDialog::accept();
}

void PagerDemodNotificationDialog::resizeTable()
{
    PagerDemodSettings::NotificationSettings dummy;
    dummy.m_matchColumn = PagerDemodSettings::MESSAGE_COL_ADDRESS;
    dummy.m_regExp = "1234567";
    dummy.m_speech = "${message}";
    dummy.m_command = "cmail.exe -to:user@host.com \"-subject: Paging ${address}\" \"-body: ${message}\"";
    dummy.m_highlight = true;
    dummy.m_plotOnMap = true;
    addRow(&dummy);
    ui->table->resizeColumnsToContents();
    ui->table->selectRow(0);
    on_remove_clicked();
    ui->table->selectRow(-1);
}

void PagerDemodNotificationDialog::on_add_clicked()
{
    addRow();
}

// Remove selected row
void PagerDemodNotificationDialog::on_remove_clicked()
{
    // Selection mode is single, so only a single row should be returned
    QModelIndexList indexList = ui->table->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->table->removeRow(row);
        m_colorGUIs.removeAt(row);
    }
}

void PagerDemodNotificationDialog::addRow(PagerDemodSettings::NotificationSettings *settings)
{
    int row = ui->table->rowCount();
    ui->table->setSortingEnabled(false);
    ui->table->setRowCount(row + 1);

    QComboBox *match = new QComboBox();
    TableColorChooser *highlight;
    if (settings) {
        highlight = new TableColorChooser(ui->table, row, NOTIFICATION_COL_HIGHLIGHT, !settings->m_highlight, settings->m_highlightColor);
    } else {
        highlight = new TableColorChooser(ui->table, row, NOTIFICATION_COL_HIGHLIGHT, false, QColor(Qt::red).rgba());
    }
    m_colorGUIs.append(highlight);
    QCheckBox *plotOnMap = new QCheckBox();
    plotOnMap->setChecked(false);
    QWidget *matchWidget = new QWidget();
    QHBoxLayout *pLayout = new QHBoxLayout(matchWidget);
    pLayout->addWidget(match);
    pLayout->setAlignment(Qt::AlignCenter);
    pLayout->setContentsMargins(0, 0, 0, 0);
    matchWidget->setLayout(pLayout);

    match->addItem("Address");
    match->addItem("Message");

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
        plotOnMap->setChecked(settings->m_plotOnMap);
    }
    else
    {
        match->setCurrentIndex(0);
        regExpItem->setData(Qt::DisplayRole, ".*");
        speechItem->setData(Qt::DisplayRole, "${message}");
#ifdef _MSC_VER
        commandItem->setData(Qt::DisplayRole, "cmail.exe -to:user@host.com \"-subject: Paging ${address}\" \"-body: ${message}\"");
#else
        commandItem->setData(Qt::DisplayRole, "sendmail -s \"Paging ${address}: ${message}\" user@host.com");
#endif
    }

    ui->table->setCellWidget(row, NOTIFICATION_COL_MATCH, match);
    ui->table->setItem(row, NOTIFICATION_COL_REG_EXP, regExpItem);
    ui->table->setItem(row, NOTIFICATION_COL_SPEECH, speechItem);
    ui->table->setItem(row, NOTIFICATION_COL_COMMAND, commandItem);
    ui->table->setCellWidget(row, NOTIFICATION_COL_PLOT_ON_MAP, plotOnMap);
    ui->table->setSortingEnabled(true);
}
