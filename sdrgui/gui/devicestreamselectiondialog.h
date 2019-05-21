///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#ifndef _SDRGUI_DEVICESTREAMSELECTIONDIALOG_H_
#define _SDRGUI_DEVICESTREAMSELECTIONDIALOG_H_

#include <QDialog>

#include "../../exports/export.h"

namespace Ui {
    class DeviceStreamSelectionDialog;
}

class SDRGUI_API DeviceStreamSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeviceStreamSelectionDialog(QWidget *parent = nullptr);
    ~DeviceStreamSelectionDialog();
    bool hasChanged() const { return m_hasChanged; }
    void setNumberOfStreams(int nbStreams);
    void setStreamIndex(int index);
    int getSelectedStreamIndex() const { return m_streamIndex; };

private slots:
    void accept();

private:
    Ui::DeviceStreamSelectionDialog *ui;
    bool m_hasChanged;
    int m_streamIndex;
};

#endif // _SDRGUI_DEVICESTREAMSELECTIONDIALOG_H_