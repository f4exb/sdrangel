///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include <QCheckBox>
#include <QComboBox>
#include <QToolButton>

#include "gui/tablecolorchooser.h"

#include "ui_radiopagingdemodnotificationdialog.h"
#include "radiopagingdemodnotificationdialog.h"

RadioPagingDemodNotificationDialog::RadioPagingDemodNotificationDialog(RadioPagingDemodSettings *settings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RadioPagingDemodNotificationDialog),
    m_settings(settings)
{
    ui->setupUi(this);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RadioPagingDemodNotificationDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->add, &QToolButton::clicked, this, [this]() { addRow(); });
    connect(ui->remove, &QToolButton::clicked, this, &RadioPagingDemodNotificationDialog::removeSelected);

    resizeTable();

    for (const auto& notification : m_settings->m_notificationSettings) {
        addRow(notification.data());
    }
}

RadioPagingDemodNotificationDialog::~RadioPagingDemodNotificationDialog()
{
    delete ui;
    qDeleteAll(m_colorGUIs);
}

void RadioPagingDemodNotificationDialog::accept()
{
    m_settings->m_notificationSettings.clear();

    for (int row = 0; row < ui->table->rowCount(); row++)
    {
        QSharedPointer<RadioPagingDemodSettings::NotificationSettings> notification =
            QSharedPointer<RadioPagingDemodSettings::NotificationSettings>::create();
        notification->m_matchField = qobject_cast<QComboBox *>(ui->table->cellWidget(row, COL_MATCH))->currentIndex();
        notification->m_regExp = ui->table->item(row, COL_REG_EXP)->data(Qt::DisplayRole).toString().trimmed();
        notification->m_speech = ui->table->item(row, COL_SPEECH)->data(Qt::DisplayRole).toString().trimmed();
        notification->m_command = ui->table->item(row, COL_COMMAND)->data(Qt::DisplayRole).toString().trimmed();
        notification->m_highlight = !m_colorGUIs[row]->m_noColor;
        notification->m_highlightColor = m_colorGUIs[row]->m_color;
        notification->m_plotOnMap = qobject_cast<QCheckBox *>(ui->table->cellWidget(row, COL_PLOT_ON_MAP))->isChecked();
        notification->updateRegularExpression();
        m_settings->m_notificationSettings.append(notification);
    }

    QDialog::accept();
}

// The columns are sized to a typical row, which is then removed
void RadioPagingDemodNotificationDialog::resizeTable()
{
    RadioPagingDemodSettings::NotificationSettings dummy;
    dummy.m_matchField = RadioPagingDemodSettings::NotificationSettings::MatchProtocol;
    dummy.m_regExp = "1234567A";
    dummy.m_speech = "${message}";
    dummy.m_command = "sendmail -s \"Paging ${address}: ${message}\" user@host.com";
    dummy.m_highlight = true;
    dummy.m_plotOnMap = true;
    addRow(&dummy);
    ui->table->resizeColumnsToContents();
    ui->table->selectRow(0);
    removeSelected();
    ui->table->selectRow(-1);
}

void RadioPagingDemodNotificationDialog::removeSelected()
{
    QModelIndexList indexList = ui->table->selectionModel()->selectedRows();

    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->table->removeRow(row);
        delete m_colorGUIs.takeAt(row);
    }
}

void RadioPagingDemodNotificationDialog::addRow(const RadioPagingDemodSettings::NotificationSettings *settings)
{
    int row = ui->table->rowCount();
    ui->table->setRowCount(row + 1);

    QComboBox *match = new QComboBox();

    for (int field = RadioPagingDemodSettings::NotificationSettings::MatchAddress;
        field <= RadioPagingDemodSettings::NotificationSettings::MatchAny; field++)
    {
        match->addItem(RadioPagingDemodSettings::NotificationSettings::matchFieldName(field));
    }

    TableColorChooser *highlight = settings
        ? new TableColorChooser(ui->table, row, COL_HIGHLIGHT, !settings->m_highlight, settings->m_highlightColor)
        : new TableColorChooser(ui->table, row, COL_HIGHLIGHT, false, QColor(Qt::red).rgba());
    m_colorGUIs.append(highlight);

    QCheckBox *plotOnMap = new QCheckBox();
    QTableWidgetItem *regExpItem = new QTableWidgetItem();
    QTableWidgetItem *speechItem = new QTableWidgetItem();
    QTableWidgetItem *commandItem = new QTableWidgetItem();

    if (settings)
    {
        match->setCurrentIndex(settings->m_matchField);
        regExpItem->setData(Qt::DisplayRole, settings->m_regExp);
        speechItem->setData(Qt::DisplayRole, settings->m_speech);
        commandItem->setData(Qt::DisplayRole, settings->m_command);
        plotOnMap->setChecked(settings->m_plotOnMap);
    }
    else
    {
        match->setCurrentIndex(RadioPagingDemodSettings::NotificationSettings::MatchAddress);
        regExpItem->setData(Qt::DisplayRole, ".*");
        speechItem->setData(Qt::DisplayRole, "${message}");
#ifdef _MSC_VER
        commandItem->setData(Qt::DisplayRole, "cmail.exe -to:user@host.com \"-subject: Paging ${address}\" \"-body: ${message}\"");
#else
        commandItem->setData(Qt::DisplayRole, "sendmail -s \"Paging ${address}: ${message}\" user@host.com");
#endif
    }

    ui->table->setCellWidget(row, COL_MATCH, match);
    ui->table->setItem(row, COL_REG_EXP, regExpItem);
    ui->table->setItem(row, COL_SPEECH, speechItem);
    ui->table->setItem(row, COL_COMMAND, commandItem);
    ui->table->setCellWidget(row, COL_PLOT_ON_MAP, plotOnMap);
}
