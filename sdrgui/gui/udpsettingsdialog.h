///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                          //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_UDPSETTINGSDIALOG_H
#define INCLUDE_UDPSETTINGSDIALOG_H

#include <QDialog>
#include <QString>

#include "export.h"

namespace Ui {
    class UDPSettingsDialog;
}

// Where to send data over UDP
class SDRGUI_API UDPSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit UDPSettingsDialog(const QString& address, uint16_t port, QWidget *parent = nullptr);
    ~UDPSettingsDialog();

    QString address() const;
    uint16_t port() const;

private:
    Ui::UDPSettingsDialog *ui;
};

#endif // INCLUDE_UDPSETTINGSDIALOG_H
