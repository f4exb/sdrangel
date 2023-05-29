///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "simplepttcommandoutputdialog.h"
#include "ui_simplepttcommandoutputdialog.h"
#include "simplepttcommand.h"

#include <QDateTime>

SimplePTTCommandOutputDialog::SimplePTTCommandOutputDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SimplePTTCommandOutputDialog)
{
    ui->setupUi(this);
    setStatusIndicator(StatusIndicatorUnknown);
}

SimplePTTCommandOutputDialog::~SimplePTTCommandOutputDialog()
{
    delete ui;
}

void SimplePTTCommandOutputDialog::setErrorText(const QProcess::ProcessError& processError)
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
        ui->errorText->setText("No or unknown error");
        break;
    }
}

void SimplePTTCommandOutputDialog::setExitText(const QProcess::ExitStatus& processExit)
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

void SimplePTTCommandOutputDialog::setExitCode(int exitCode)
{
    ui->exitCode->setText(tr("%1").arg(exitCode));
}

void SimplePTTCommandOutputDialog::setLog(const QString& log)
{
    ui->logEdit->setPlainText(log);
}

void SimplePTTCommandOutputDialog::setStatusIndicator(StatusIndicator indicator)
{
    QString statusColor;

    switch (indicator)
    {
        case StatusIndicatorOK:
            statusColor = "rgb(85, 232, 85)";
            break;
        case StatusIndicatorKO:
            statusColor = "rgb(232, 85, 85)";
            break;
        default:
            statusColor = "gray";
    }

    ui->statusIndicator->setStyleSheet("QLabel { background-color: " +
        statusColor + "; border-radius: 12px; }");
}

void SimplePTTCommandOutputDialog::setEndTime(const QDateTime& dt)
{
    QString dateStr = dt.toString("yyyy-MM-dd HH:mm:ss.zzz");
    ui->endTime->setText(dateStr);
}
