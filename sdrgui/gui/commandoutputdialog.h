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

#ifndef SDRGUI_GUI_COMMANDOUTPUTDIALOG_H_
#define SDRGUI_GUI_COMMANDOUTPUTDIALOG_H_

#include <QDialog>
#include <QProcess>

namespace Ui {
    class CommandOutputDialog;
}

class Command;

class CommandOutputDialog : public QDialog {
    Q_OBJECT

public:
    explicit CommandOutputDialog(Command& command, QWidget* parent = 0);
    ~CommandOutputDialog();

    void fromCommand(const Command& command);

private:
    Ui::CommandOutputDialog* ui;
    Command& m_command;

    void refresh();
    void setErrorText(const QProcess::ProcessError& processError);
    void setExitText(const QProcess::ExitStatus& processExit);

private slots:
    void on_processRefresh_toggled(bool checked);
    void on_processKill_toggled(bool checked);
};


#endif /* SDRGUI_GUI_COMMANDOUTPUTDIALOG_H_ */
