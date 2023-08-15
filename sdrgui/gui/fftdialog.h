///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GUI_FFTDIALOG_H_
#define SDRGUI_GUI_FFTDIALOG_H_

#include <QDialog>

#include "settings/mainsettings.h"
#include "export.h"

namespace Ui {
    class FFTDialog;
}

class SDRGUI_API FFTDialog : public QDialog {
    Q_OBJECT
public:
    explicit FFTDialog(MainSettings& mainSettings, QWidget* parent = nullptr);
    ~FFTDialog();

private slots:
    void accept();

private:
    Ui::FFTDialog *ui;
    MainSettings& m_mainSettings;
};

#endif // SDRGUI_GUI_FFTDIALOG_H_
