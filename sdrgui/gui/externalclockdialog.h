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
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_GUI_EXTERNALCLOCKDIALOG_H_
#define SDRBASE_GUI_EXTERNALCLOCKDIALOG_H_

#include <QDialog>

namespace Ui {
    class ExternalClockDialog;
}

class ExternalClockDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExternalClockDialog(qint64& externalClockFrequency, bool& externalClockFrequencyActive, QWidget* parent = 0);
    ~ExternalClockDialog();

private:
    Ui::ExternalClockDialog* ui;
    qint64& m_externalClockFrequency;
    bool& m_externalClockFrequencyActive;

private slots:
    void accept();
};



#endif /* SDRBASE_GUI_EXTERNALCLOCKDIALOG_H_ */
