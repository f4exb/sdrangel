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

#ifndef PLUGINS_CHANNELRX_DEMODM17_M17STATUSTEXTDIALOG_H_
#define PLUGINS_CHANNELRX_DEMODM17_M17STATUSTEXTDIALOG_H_

#include <QDialog>

namespace Ui {
    class M17StatusTextDialog;
}

class M17StatusTextDialog : public QDialog {
    Q_OBJECT

public:
    explicit M17StatusTextDialog(QWidget* parent = nullptr);
    ~M17StatusTextDialog();

    void addLine(const QString& line);

private:
    Ui::M17StatusTextDialog* ui;

private slots:
    void on_clear_clicked();
    void on_saveLog_clicked();
};


#endif /* PLUGINS_CHANNELRX_DEMODM17_M17STATUSTEXTDIALOG_H_ */
