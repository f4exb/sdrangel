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

#ifndef SDRGUI_GUI_LOGGINGDIALOG_H_
#define SDRGUI_GUI_LOGGINGDIALOG_H_

#include <QDialog>
#include "settings/mainsettings.h"

namespace Ui {
    class LoggingDialog;
}

class LoggingDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoggingDialog(MainSettings& mainSettings, QWidget* parent = 0);
    ~LoggingDialog();

private:
    Ui::LoggingDialog* ui;
    MainSettings& m_mainSettings;
    QString m_fileName;

    QtMsgType msgLevelFromIndex(int intMsgLevel);
    int msgLevelToIndex(const QtMsgType& msgLevel);

private slots:
    void accept();
    void on_showFileDialog_clicked(bool checked);
};

#endif /* SDRGUI_GUI_LOGGINGDIALOG_H_ */
