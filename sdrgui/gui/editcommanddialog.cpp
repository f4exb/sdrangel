///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "editcommanddialog.h"
#include "ui_editcommanddialog.h"
#include "commands/command.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QKeyEvent>
#include <algorithm>

const std::vector<Qt::Key> EditCommandDialog::m_composeKeys = {Qt::Key_Shift, Qt::Key_Control, Qt::Key_Meta, Qt::Key_Alt, Qt::Key_AltGr};

EditCommandDialog::EditCommandDialog(const QStringList& groups, const QString& group, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::EditCommandDialog),
    m_key(static_cast<Qt::Key>(0))
{
    ui->setupUi(this);
    ui->group->addItems(groups);
    ui->group->lineEdit()->setText(group);
    setKeyAssociate();
    setKeyLabel();
}

EditCommandDialog::~EditCommandDialog()
{
    delete ui;
}

QString EditCommandDialog::getGroup() const
{
    return ui->group->lineEdit()->text();
}

QString EditCommandDialog::getDescription() const
{
    return ui->description->text();
}

void EditCommandDialog::setGroup(const QString& group)
{
    ui->group->lineEdit()->setText(group);
}

void EditCommandDialog::setDescription(const QString& description)
{
    ui->description->setText(description);
}

QString EditCommandDialog::getCommand() const
{
    return ui->command->text();
}

void EditCommandDialog::setCommand(const QString& command)
{
    ui->command->setText(command);
}

QString EditCommandDialog::getArguments() const
{
    return ui->args->text();
}

void EditCommandDialog::setArguments(const QString& arguments)
{
    ui->args->setText(arguments);
}

Qt::Key EditCommandDialog::getKey() const
{
    return m_key;
}

Qt::KeyboardModifiers EditCommandDialog::getKeyModifiers() const
{
    return m_keyModifiers;
}

void EditCommandDialog::setKey(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    m_key = key;
    m_keyModifiers = modifiers;
    setKeyAssociate();
    setKeyLabel();
}

bool EditCommandDialog::getAssociateKey() const
{
    return ui->keyAssociate->isChecked();
}

void EditCommandDialog::setAssociateKey(bool release)
{
    ui->keyAssociate->setChecked(release);
}

bool EditCommandDialog::getRelease() const
{
    return ui->keyRelease->isChecked();
}

void EditCommandDialog::setRelease(bool release)
{
    ui->keyRelease->setChecked(release);
}

void EditCommandDialog::on_showFileDialog_clicked(bool checked __attribute__((unused)))
{
    QString commandFileName = ui->command->text();
    QFileInfo commandFileInfo(commandFileName);
    QString commandFolderName = commandFileInfo.baseName();
    QFileInfo commandDirInfo(commandFolderName);
    QString dirStr;

    if (commandFileInfo.exists()) {
        dirStr = commandFileName;
    } else if (commandDirInfo.exists()) {
        dirStr = commandFolderName;
    } else {
        dirStr = ".";
    }

    QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("Select command"),
            dirStr,
            tr("Python (*.py);;Shell (*.sh *.bat);;Binary (*.bin *.exe);;All (*.*)"));

    if (fileName != "") {
        ui->command->setText(fileName);
    }
}

void EditCommandDialog::on_keyCapture_toggled(bool checked)
{
    if (checked)
    {
        ui->keyCapture->setFocus();
        ui->keyCapture->setFocusPolicy(Qt::StrongFocus);
    }
    else
    {
        ui->keyCapture->setFocusPolicy(Qt::NoFocus);
        ui->keyCapture->clearFocus();
    }
}

void EditCommandDialog::keyPressEvent(QKeyEvent *e)
{
    if (ui->keyCapture->isChecked() && (!isComposeKey(static_cast<Qt::Key>(e->key()))))
    {
        m_key = static_cast<Qt::Key>(e->key());

        if (e->modifiers())
        {
            //qDebug("EditCommandDialog::keyPressEvent: has modifiers: %x", QFlags<Qt::KeyboardModifier>::Int(e->modifiers()));
            m_keyModifiers = e->modifiers();
        }
        else
        {
            m_keyModifiers = Qt::NoModifier;
        }

        setKeyAssociate();
        setKeyLabel();
        //qDebug("EditCommandDialog::keyPressEvent: key: %x", m_key);

        ui->keyCapture->setChecked(false);
        ui->keyCapture->setFocusPolicy(Qt::NoFocus);
        ui->keyCapture->clearFocus();
    }
}

void EditCommandDialog::toCommand(Command& command) const
{
    command.setGroup(ui->group->currentText());
    command.setDescription(ui->description->text());
    command.setCommand(ui->command->text());
    command.setArgString(ui->args->text());
    command.setAssociateKey(ui->keyAssociate->isChecked());
    command.setKey(m_key);
    command.setKeyModifiers(m_keyModifiers);
    command.setRelease(ui->keyRelease->isChecked());
}

void EditCommandDialog::fromCommand(const Command& command)
{
    ui->group->lineEdit()->setText(command.getGroup());
    ui->description->setText(command.getDescription());
    ui->command->setText(command.getCommand());
    ui->args->setText(command.getArgString());
    ui->keyAssociate->setChecked(command.getAssociateKey());
    m_key = command.getKey();
    m_keyModifiers = command.getKeyModifiers();
    setKeyAssociate();
    setKeyLabel();
    ui->keyRelease->setChecked(command.getRelease());
}

bool EditCommandDialog::isComposeKey(Qt::Key key)
{
    auto it = std::find(m_composeKeys.begin(), m_composeKeys.end(), key);
    return it != m_composeKeys.end();
}

void EditCommandDialog::setKeyLabel()
{
    if (m_key == 0)
    {
        ui->keyLabel->setText("");
    }
    else if (m_keyModifiers != Qt::NoModifier)
    {
        QString altGrStr = m_keyModifiers & Qt::GroupSwitchModifier ? "Gr " : "";
        int maskedModifiers = (m_keyModifiers & 0x3FFFFFFF) + ((m_keyModifiers & 0x40000000)>>3);
        ui->keyLabel->setText(altGrStr + QKeySequence(maskedModifiers, m_key).toString());
    }
    else
    {
        ui->keyLabel->setText(QKeySequence(m_key).toString());
    }
}

void EditCommandDialog::setKeyAssociate()
{
    if (m_key == 0)
    {
        ui->keyAssociate->setChecked(false);
        ui->keyAssociate->setEnabled(false);
    }
    else
    {
        ui->keyAssociate->setEnabled(true);
    }
}

