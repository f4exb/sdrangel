///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
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

#include "dsdstatustextdialog.h"
#include "ui_dsdstatustextdialog.h"

#include <QDateTime>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

DSDStatusTextDialog::DSDStatusTextDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::DSDStatusTextDialog)
{
    ui->setupUi(this);
}

DSDStatusTextDialog::~DSDStatusTextDialog()
{
    delete ui;
}

void DSDStatusTextDialog::addLine(const QString& line)
{
    if ((line.size() > 0) && (line != m_lastLine))
    {
        QDateTime dt = QDateTime::currentDateTime();
        QString dateStr = dt.toString("HH:mm:ss");
        QTextCursor cursor = ui->logEdit->textCursor();
        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        cursor.insertText(tr("%1 %2\n").arg(dateStr).arg(line));
        if (ui->pinToLastLine->isChecked()) {
            ui->logEdit->verticalScrollBar()->setValue(ui->logEdit->verticalScrollBar()->maximum());
        }
        m_lastLine = line;
    }
}

void DSDStatusTextDialog::on_clear_clicked()
{
    ui->logEdit->clear();
}

void DSDStatusTextDialog::on_saveLog_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                    tr("Open log file"), ".", tr("Log files (*.log)"),  0, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        QFileInfo fileInfo(fileName);

        if (fileInfo.suffix() != "log") {
            fileName += ".log";
        }

        QFile exportFile(fileName);

        if (exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream outstream(&exportFile);
            outstream << ui->logEdit->toPlainText();
            exportFile.close();
        }
        else
        {
            QMessageBox::information(this, tr("Message"), tr("Cannot open file for writing"));
        }
    }

}
