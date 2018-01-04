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

#include "commandoutputdialog.h"
#include "ui_commandoutputdialog.h"
#include "commands/command.h"

#include <QDateTime>

CommandOutputDialog::CommandOutputDialog(Command& command, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::CommandOutputDialog),
    m_command(command)
{
    ui->setupUi(this);
    refresh();
}

CommandOutputDialog::~CommandOutputDialog()
{
    delete ui;
}

void CommandOutputDialog::refresh()
{
    ui->commandText->setText(m_command.getLastProcessCommandLine());
    ui->processPid->setText(QString("%1").arg(m_command.getLastProcessPid()));

    if (m_command.getLastProcessStartTimestamp().tv_sec == 0) {
        ui->startTime->setText(("..."));
    }
    else
    {
        struct timeval tv = m_command.getLastProcessStartTimestamp();
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(tv.tv_sec * 1000LL + tv.tv_usec / 1000LL);
        QString dateStr = dt.toString("yyyy-MM-dd hh:mm:ss.zzz");
        ui->startTime->setText(dateStr);
    }

    if (m_command.getLastProcessFinishTimestamp().tv_sec == 0) {
        ui->endTime->setText(("..."));
    }
    else
    {
        struct timeval tv = m_command.getLastProcessFinishTimestamp();
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(tv.tv_sec * 1000LL + tv.tv_usec / 1000LL);
        QString dateStr = dt.toString("yyyy-MM-dd hh:mm:ss.zzz");
        ui->endTime->setText(dateStr);
    }

    ui->runningState->setChecked(m_command.getLastProcessState() == QProcess::Running);
    QProcess::ProcessError processError;

    if (m_command.getLastProcessStartTimestamp().tv_sec == 0) // not started
    {
        ui->errorText->setText("...");
        ui->exitCode->setText("-");
        ui->exitText->setText("...");
        ui->runningState->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }
    else if (m_command.getLastProcessState() != QProcess::NotRunning) // running
    {
        ui->errorText->setText("...");
        ui->runningState->setStyleSheet("QToolButton { background-color : orange; }");
    }
    else // finished
    {
        if (m_command.getLastProcessError(processError)) // finished
        {
            ui->runningState->setStyleSheet("QToolButton { background-color : red; }");
            setErrorText(processError);
        }
        else
        {
            ui->runningState->setStyleSheet("QToolButton { background-color : green; }");
            ui->errorText->setText("No error");
        }

        int processExitCode;
        QProcess::ExitStatus processExitStatus;

        if (m_command.getLastProcessExit(processExitCode, processExitStatus))
        {
            ui->exitCode->setText(QString("%1").arg(processExitCode));
            setExitText(processExitStatus);
        }
        else
        {
            ui->exitCode->setText("-");
            ui->exitText->setText("...");
        }
    }

    ui->logEdit->setPlainText(m_command.getLastProcessLog());
}

void CommandOutputDialog::setErrorText(const QProcess::ProcessError& processError)
{
    switch(processError)
    {
    case QProcess::FailedToStart:
        ui->errorText->setText("Failed to start");
        break;
    case QProcess::Crashed:
        ui->errorText->setText("Crashed");
        break;
    case QProcess::Timedout:
        ui->errorText->setText("Timed out");
        break;
    case QProcess::WriteError:
        ui->errorText->setText("Write error");
        break;
    case QProcess::ReadError:
        ui->errorText->setText("Read error");
        break;
    case QProcess::UnknownError:
    default:
        ui->errorText->setText("Unknown error");
        break;
    }
}

void CommandOutputDialog::setExitText(const QProcess::ExitStatus& processExit)
{
    switch(processExit)
    {
    case QProcess::NormalExit:
        ui->exitText->setText("Normal exit");
        break;
    case QProcess::CrashExit:
        ui->exitText->setText("Program crashed");
        break;
    default:
        ui->exitText->setText("Unknown state");
        break;
    }
}

void CommandOutputDialog::on_processRefresh_toggled(bool checked)
{
    if (checked)
    {
        refresh();
        ui->processRefresh->setChecked(false);
    }
}

void CommandOutputDialog::on_processKill_toggled(bool checked)
{
    if (checked)
    {
        m_command.kill();
        ui->processKill->setChecked(false);
    }
}

