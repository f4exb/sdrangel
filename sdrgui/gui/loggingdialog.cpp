///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QFileDialog>

#include "loggingdialog.h"
#include "ui_loggingdialog.h"

LoggingDialog::LoggingDialog(MainSettings& mainSettings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::LoggingDialog),
    m_mainSettings(mainSettings)
{
    ui->setupUi(this);
    ui->consoleLevel->setCurrentIndex(msgLevelToIndex(m_mainSettings.getConsoleMinLogLevel()));
    ui->fileLevel->setCurrentIndex(msgLevelToIndex(m_mainSettings.getFileMinLogLevel()));
    ui->logToFile->setChecked(m_mainSettings.getUseLogFile());
    ui->logFileNameText->setText(m_mainSettings.getLogFileName());
    m_fileName = m_mainSettings.getLogFileName();
}

LoggingDialog::~LoggingDialog()
{
    delete ui;
}

void LoggingDialog::accept()
{
    m_mainSettings.setConsoleMinLogLevel(msgLevelFromIndex(ui->consoleLevel->currentIndex()));
    m_mainSettings.setFileMinLogLevel(msgLevelFromIndex(ui->fileLevel->currentIndex()));
    m_mainSettings.setUseLogFile(ui->logToFile->isChecked());
    m_mainSettings.setLogFileName(m_fileName);
    QDialog::accept();
}

void LoggingDialog::on_showFileDialog_clicked(bool checked __attribute__((unused)))
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save log file"), ".", tr("Log Files (*.log)"));

    if (fileName != "")
    {
        qDebug("LoggingDialog::on_showFileDialog_clicked: selected: %s", qPrintable(fileName));
        m_fileName = fileName;
        ui->logFileNameText->setText(fileName);
    }
}

QtMsgType LoggingDialog::msgLevelFromIndex(int intMsgLevel)
{
    switch (intMsgLevel)
    {
        case 0:
            return QtDebugMsg;
            break;
        case 1:
            return QtInfoMsg;
            break;
        case 2:
            return QtWarningMsg;
            break;
        case 3:
            return QtCriticalMsg;
            break;
        default:
            return QtDebugMsg;
            break;
    }
}

int LoggingDialog::msgLevelToIndex(const QtMsgType& msgLevel)
{
    switch (msgLevel)
    {
        case QtDebugMsg:
            return 0;
            break;
        case QtInfoMsg:
            return 1;
            break;
        case QtWarningMsg:
            return 2;
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            return 3;
            break;
        default:
            return 0;
            break;
    }
}


