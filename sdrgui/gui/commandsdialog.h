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

#ifndef SDRGUI_GUI_COMMANDSDIALOG_H_
#define SDRGUI_GUI_COMMANDSDIALOG_H_

#include <QDialog>

#include "export.h"

class CommandKeyReceiver;

namespace Ui {
    class CommandsDialog;
}

class SDRGUI_API CommandsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CommandsDialog(QWidget* parent = nullptr);
    ~CommandsDialog();

    void setApiHost(const QString& apiHost) { m_apiHost = apiHost; }
    void setApiPort(int apiPort) { m_apiPort = apiPort; }
    void setCommandKeyReceiver(CommandKeyReceiver *commandKeyReceiver) { m_commandKeyReceiver = commandKeyReceiver; }
    void populateTree();

private:
    enum {
		PGroup,
		PItem
	};

    Ui::CommandsDialog* ui;
    QString m_apiHost;
    int m_apiPort;
    CommandKeyReceiver *m_commandKeyReceiver;

    QTreeWidgetItem* addCommandToTree(const Command* command);

private slots:
    void on_commandNew_clicked();
    void on_commandDuplicate_clicked();
    void on_commandEdit_clicked();
    void on_commandRun_clicked();
    void on_commandOutput_clicked();
    void on_commandDelete_clicked();
    void on_commandKeyboardConnect_toggled(bool checked);
};

#endif // SDRGUI_GUI_COMMANDSDIALOG_H_
