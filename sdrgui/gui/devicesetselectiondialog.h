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

#ifndef SDRGUI_GUI_DEVICESETSELECTIONDIALOG_H_
#define SDRGUI_GUI_DEVICESETSELECTIONDIALOG_H_

#include <QDialog>

#include "export.h"

namespace Ui {
    class WorkspaceSelectionDialog;
}

class DeviceUISet;

class SDRGUI_API DeviceSetSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeviceSetSelectionDialog(std::vector<DeviceUISet*>& deviceUIs, int channelDeviceSetIndex, QWidget *parent = nullptr);
    ~DeviceSetSelectionDialog();

    bool hasChanged() const { return m_hasChanged; }
    int getSelectedIndex() const { return m_selectedDeviceSetIndex; }

private:
    Ui::WorkspaceSelectionDialog *ui;
    std::vector<DeviceUISet*>&  m_deviceUIs;
    std::vector<int> m_deviceSetIndexes;
    int m_channelDeviceSetIndex;
    int m_selectedDeviceSetIndex;
    bool m_hasChanged;

    QString getDeviceTypeChar(int deviceType)
    {
        switch(deviceType)
        {
            case 0:
                return "R";
                break;
            case 1:
                return "T";
                break;
            case 2:
                return "M";
                break;
            default:
                return "X";
                break;
        }
    }

private slots:
    void accept();
};

#endif // SDRGUI_GUI_WORKSPACESELECTIONDIALOG_H_
