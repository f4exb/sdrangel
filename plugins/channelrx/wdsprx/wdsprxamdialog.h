///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
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
#ifndef INCLUDE_WDSPRXAMDIALOG_H
#define INCLUDE_WDSPRXAMDIALOG_H

#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxAMDialog;
}

class WDSPRxAMDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedFadeLevel,
    };

    explicit WDSPRxAMDialog(QWidget* parent = nullptr);
    ~WDSPRxAMDialog();

    void setFadeLevel(bool fadeLevel);
    bool getFadeLevel() const { return m_fadeLevel; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxAMDialog *ui;
    bool m_fadeLevel;

private slots:
    void on_fadeLevel_clicked(bool checked);
};

#endif
