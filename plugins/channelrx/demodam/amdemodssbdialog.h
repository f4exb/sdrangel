///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
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

#ifndef PLUGINS_CHANNELRX_DEMODAM_AMDEMODSSBDIALOG_H_
#define PLUGINS_CHANNELRX_DEMODAM_AMDEMODSSBDIALOG_H_

#include <QDialog>

namespace Ui {
    class AMDemodSSBDialog;
}

class AMDemodSSBDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AMDemodSSBDialog(bool usb, QWidget* parent = 0);
    ~AMDemodSSBDialog();

    bool isUsb() const { return m_usb; }

private:
    Ui::AMDemodSSBDialog* ui;
    bool m_usb;

private slots:
    void accept();
};

#endif /* PLUGINS_CHANNELRX_DEMODAM_AMDEMODSSBDIALOG_H_ */
