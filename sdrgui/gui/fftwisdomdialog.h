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

#ifndef SDRGUI_GUI_FFTWISDOMDIALOG_H_
#define SDRGUI_GUI_FFTWISDOMDIALOG_H_

#include <QDialog>

#include "export.h"

namespace Ui {
    class FFTWisdomDialog;
}

class QProcess;

class SDRGUI_API FFTWisdomDialog : public QDialog {
    Q_OBJECT
public:
    explicit FFTWisdomDialog(QProcess *process, QWidget* parent = nullptr);
    ~FFTWisdomDialog();
    QProcess *getProcess() { return m_process; }

private slots:
    void on_showFileDialog_clicked();
    void on_fftwMaxSize_currentIndexChanged(int index);
    void on_fftwReverse_toggled(bool checked);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void accept();
    void reject();

private:
    void updateArguments(int fftMaxLog2, bool includeReverse);

    Ui::FFTWisdomDialog *ui;
    QString m_fftwExecPath;
    QStringList m_fftwArguments;
    QProcess *m_process;
};


#endif // SDRGUI_GUI_FFTWISDOMDIALOG_H_
