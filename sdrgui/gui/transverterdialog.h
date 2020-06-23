///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#ifndef SDRBASE_GUI_TRANSVERTERDIALOG_H_
#define SDRBASE_GUI_TRANSVERTERDIALOG_H_

#include <QDialog>

#include "export.h"

namespace Ui {
    class TransverterDialog;
}

class SDRGUI_API TransverterDialog : public QDialog {
    Q_OBJECT

public:
    explicit TransverterDialog(qint64& deltaFrequency, bool& deltaFrequencyActive, bool& iqOrder, QWidget* parent = 0);
    ~TransverterDialog();

private:
    Ui::TransverterDialog* ui;
    qint64& m_deltaFrequency;
    bool& m_deltaFrequencyActive;
    bool& m_iqOrder;

private slots:
    void accept();
    void on_iqOrder_toggled(bool checked);
};



#endif /* SDRBASE_GUI_TRANSVERTERDIALOG_H_ */
