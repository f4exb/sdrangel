///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 F4EXB                                                      //
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

#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>

#include "fftwisdomdialog.h"
#include "ui_fftwisdomdialog.h"

FFTWisdomDialog::FFTWisdomDialog(QProcess *process, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FFTWisdomDialog),
    m_process(process)
{
    ui->setupUi(this);

    QString pathVar = qgetenv("PATH");
    QStringList findPaths = pathVar.split(QDir::listSeparator());
    findPaths.append(QCoreApplication::applicationDirPath());
    QString exePath = QStandardPaths::findExecutable("fftwf-wisdom", findPaths);

    if (exePath.length() != 0)
    {
        m_fftwExecPath = exePath;
        ui->executable->setText(exePath);
    }

    updateArguments(3, false);
}

FFTWisdomDialog::~FFTWisdomDialog()
{
    delete ui;
}

void FFTWisdomDialog::on_showFileDialog_clicked()
{
    QFileDialog fileDialog(this, "Select FFTW Wisdom file generator");
    fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
    fileDialog.selectFile(m_fftwExecPath);

    if (fileDialog.exec() == QDialog::Accepted)
    {
        QStringList fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0) {
            m_fftwExecPath = fileNames.at(0);
        }
    }
}

void FFTWisdomDialog::on_fftwMaxSize_currentIndexChanged(int index)
{
    updateArguments(index, ui->fftwReverse->isChecked());
}

void FFTWisdomDialog::on_fftwReverse_toggled(bool checked)
{
    updateArguments(ui->fftwMaxSize->currentIndex(), checked);
}

void FFTWisdomDialog::accept()
{
    m_process->start(m_fftwExecPath, m_fftwArguments);
    qDebug("FFTWisdomDialog::accept: process started");
    QDialog::accept();
}

void FFTWisdomDialog::reject()
{
    QDialog::reject();
}

void FFTWisdomDialog::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug("FFTWisdomDialog::processFinished: process finished rc=%d (%d)", exitCode, (int) exitStatus);

    if ((exitCode != 0) || (exitStatus != QProcess::NormalExit))
    {
        QMessageBox::critical(this, "FFTW Wisdom", "fftwf-widdsom program failed");
    }
    else
    {
        QString log = m_process->readAllStandardOutput();
        QMessageBox::information(this, "FFTW Wisdom", QString("Success\n%1").arg(log));

    }

    delete m_process;
}

void FFTWisdomDialog::updateArguments(int fftMaxLog2, bool includeReverse)
{
    QString filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    filePath += QDir::separator();
    filePath += "fftw-wisdom";

    m_fftwArguments.clear();
    m_fftwArguments.append("-v");
    m_fftwArguments.append("-n");
    m_fftwArguments.append("-o");
    m_fftwArguments.append(filePath);

    for (int i = 7; i <= 7+fftMaxLog2; i++)
    {
        m_fftwArguments.append(QString("%1").arg(1<<i));

        if (includeReverse) {
            m_fftwArguments.append(QString("b%1").arg(1<<i));
        }
    }

    QString argStr = m_fftwArguments.join(' ');

    qDebug("FFTWisdomDialog::updateArguments: %s %s", qPrintable(m_fftwExecPath), qPrintable(argStr));
    ui->fftwCommand->setText(m_fftwExecPath + " " + argStr);
}
