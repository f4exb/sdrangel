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

#ifndef PLUGINS_CHANNELRX_DEMODDSD_DSDSTATUSTEXTDIALOG_H_
#define PLUGINS_CHANNELRX_DEMODDSD_DSDSTATUSTEXTDIALOG_H_

#include <QDialog>

namespace Ui {
    class DSDStatusTextDialog;
}

class DSDStatusTextDialog : public QDialog {
    Q_OBJECT

public:
    explicit DSDStatusTextDialog(QWidget* parent = 0);
    ~DSDStatusTextDialog();

    void addLine(const QString& line);

private:
    Ui::DSDStatusTextDialog* ui;
    QString m_lastLine;

private slots:
    void on_clear_clicked();
    void on_saveLog_clicked();
};


#endif /* PLUGINS_CHANNELRX_DEMODDSD_DSDSTATUSTEXTDIALOG_H_ */
