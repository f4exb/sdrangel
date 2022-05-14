///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QMessageBox>

#include "commands/command.h"
#include "commands/commandkeyreceiver.h"
#include "editcommanddialog.h"
#include "addpresetdialog.h"
#include "commandoutputdialog.h"
#include "commanditem.h"
#include "maincore.h"
#include "mainwindow.h"

#include "commandsdialog.h"
#include "ui_commandsdialog.h"

CommandsDialog::CommandsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::CommandsDialog),
    m_apiHost("127.0.0.1"),
    m_apiPort(8091),
    m_commandKeyReceiver(nullptr)
{
    ui->setupUi(this);
    ui->commandKeyboardConnect->hide(); // FIXME
}

CommandsDialog::~CommandsDialog()
{
    delete ui;
}

void CommandsDialog::populateTree()
{
    MainCore::instance()->m_settings.sortCommands();
    ui->commandTree->clear();

    for (int i = 0; i < MainCore::instance()->m_settings.getCommandCount(); ++i) {
        addCommandToTree(MainCore::instance()->m_settings.getCommand(i));
    }
}

void CommandsDialog::on_commandNew_clicked()
{
    QStringList groups;
    QString group = "";
    QString description = "";

    for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++) {
        groups.append(ui->commandTree->topLevelItem(i)->text(0));
    }

    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if(item != 0)
    {
        if(item->type() == PGroup) {
            group = item->text(0);
        } else if(item->type() == PItem) {
            group = item->parent()->text(0);
            description = item->text(0);
        }
    }

    Command *command = new Command();
    command->setGroup(group);
    command->setDescription(description);
    EditCommandDialog editCommandDialog(groups, group, this);
    editCommandDialog.fromCommand(*command);

    if (editCommandDialog.exec() == QDialog::Accepted)
    {
        editCommandDialog.toCommand(*command);
        MainCore::instance()->m_settings.addCommand(command);
        ui->commandTree->setCurrentItem(addCommandToTree(command));
        MainCore::instance()->m_settings.sortCommands();
    }
}

void CommandsDialog::on_commandDuplicate_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();
    const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));
    Command *commandCopy = new Command(*command);
    MainCore::instance()->m_settings.addCommand(commandCopy);
    ui->commandTree->setCurrentItem(addCommandToTree(commandCopy));
    MainCore::instance()->m_settings.sortCommands();
}

void CommandsDialog::on_commandEdit_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();
    bool change = false;
    const Command *changedCommand = 0;
    QString newGroupName;

    QStringList groups;

    for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++) {
        groups.append(ui->commandTree->topLevelItem(i)->text(0));
    }

    if(item != 0)
    {
        if (item->type() == PItem)
        {
            const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));

            if (command != 0)
            {
                EditCommandDialog editCommandDialog(groups, command->getGroup(), this);
                editCommandDialog.fromCommand(*command);

                if (editCommandDialog.exec() == QDialog::Accepted)
                {
                    Command* command_mod = const_cast<Command*>(command);
                    editCommandDialog.toCommand(*command_mod);
                    change = true;
                    changedCommand = command;
                }
            }
        }
        else if (item->type() == PGroup)
        {
            AddPresetDialog dlg(groups, item->text(0), this);
            dlg.showGroupOnly();
            dlg.setDialogTitle("Edit command group");
            dlg.setDescriptionBoxTitle("Command details");

            if (dlg.exec() == QDialog::Accepted)
            {
                MainCore::instance()->m_settings.renameCommandGroup(item->text(0), dlg.group());
                newGroupName = dlg.group();
                change = true;
            }
        }
    }

    if (change)
    {
        MainCore::instance()->m_settings.sortCommands();
        ui->commandTree->clear();

        for (int i = 0; i < MainCore::instance()->m_settings.getCommandCount(); ++i)
        {
            QTreeWidgetItem *item_x = addCommandToTree(MainCore::instance()->m_settings.getCommand(i));
            const Command* command_x = qvariant_cast<const Command*>(item_x->data(0, Qt::UserRole));
            if (changedCommand &&  (command_x == changedCommand)) { // set cursor on changed command
                ui->commandTree->setCurrentItem(item_x);
            }
        }

        if (!changedCommand) // on group name change set cursor on the group that has been changed
        {
            for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++)
            {
                QTreeWidgetItem* item = ui->commandTree->topLevelItem(i);

                if (item->text(0) == newGroupName) {
                    ui->commandTree->setCurrentItem(item);
                }
            }
        }
    }
}

void CommandsDialog::on_commandRun_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if (item)
    {
        if (item->type() == PItem) // run individual command
        {
            const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));
            Command* command_mod = const_cast<Command*>(command);
            command_mod->run(m_apiHost, m_apiPort);
        }
        else if (item->type() == PGroup) // run all commands in this group
        {
            QString group = item->text(0);

            for (int i = 0; i < MainCore::instance()->m_settings.getCommandCount(); ++i)
            {
                Command *command_mod = const_cast<Command*>(MainCore::instance()->m_settings.getCommand(i));

                if (command_mod->getGroup() == group) {
                    command_mod->run(m_apiHost, m_apiPort);
                }
            }
        }
    }
}

void CommandsDialog::on_commandOutput_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if ((item != 0) && (item->type() == PItem))
    {
        const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));
        Command* command_mod = const_cast<Command*>(command);
        CommandOutputDialog commandOutputDialog(*command_mod);
        commandOutputDialog.exec();
    }
}

void CommandsDialog::on_commandDelete_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if (item != 0)
    {
        if (item->type() == PItem) // delete individual command
        {
            const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));

            if(command)
            {
                if (QMessageBox::question(this,
                        tr("Delete command"),
                        tr("Do you want to delete command '%1'?")
                            .arg(command->getDescription()), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
                {
                    delete item;
                    MainCore::instance()->m_settings.deleteCommand(command);
                }
            }
        }
        else if (item->type() == PGroup) // delete all commands in this group
        {
            if (QMessageBox::question(this,
                    tr("Delete command group"),
                    tr("Do you want to delete command group '%1'?")
                        .arg(item->text(0)), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
            {
                MainCore::instance()->m_settings.deleteCommandGroup(item->text(0));

                ui->commandTree->clear();

                for (int i = 0; i < MainCore::instance()->m_settings.getCommandCount(); ++i) {
                    addCommandToTree(MainCore::instance()->m_settings.getCommand(i));
                }
            }
        }
    }
}

void CommandsDialog::on_commandKeyboardConnect_toggled(bool checked)
{
    qDebug("CommandsDialog::on_commandKeyboardConnect_toggled: %s", checked ? "true" : "false");

    if (checked) {
        MainWindow::getInstance()->commandKeysConnect(MainWindow::getInstance(), SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    } else {
        MainWindow::getInstance()->commandKeysDisconnect(MainWindow::getInstance(), SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
}

QTreeWidgetItem* CommandsDialog::addCommandToTree(const Command* command)
{
    QTreeWidgetItem* group = 0;

    for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++)
    {
        if(ui->commandTree->topLevelItem(i)->text(0) == command->getGroup())
        {
            group = ui->commandTree->topLevelItem(i);
            break;
        }
    }

    if(group == 0)
    {
        QStringList sl;
        sl.append(command->getGroup());
        group = new QTreeWidgetItem(ui->commandTree, sl, PGroup);
        group->setFirstColumnSpanned(true);
        group->setExpanded(true);
        ui->commandTree->sortByColumn(0, Qt::AscendingOrder);
    }

    QStringList sl;
    sl.append(QString("%1").arg(command->getDescription())); // Descriptions column
    sl.append(QString("%1").arg(command->getAssociateKey() ? command->getRelease() ? "R" : "P" : "-")); // key press/release column
    sl.append(QString("%1").arg(command->getKeyLabel()));   // key column
    CommandItem* item = new CommandItem(group, sl, command->getDescription(), PItem);
    item->setData(0, Qt::UserRole, QVariant::fromValue(command));
    item->setTextAlignment(0, Qt::AlignLeft);
    ui->commandTree->resizeColumnToContents(0); // Resize description column to minimum
    ui->commandTree->resizeColumnToContents(1); // Resize key column to minimum
    ui->commandTree->resizeColumnToContents(2); // Resize key press/release column to minimum

    //updatePresetControls();
    return item;
}
