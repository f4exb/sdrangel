///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
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

#ifndef SDRGUI_GUI_SAMPLINGDEVICESDOCK_H_
#define SDRGUI_GUI_SAMPLINGDEVICESDOCK_H_

#include <QDockWidget>
#include <QList>

#include "samplingdevicedialog.h"

class QHBoxLayout;
class QLabel;
class QPushButton;
class SamplingDeviceDialog;

class SamplingDevicesDock : public QDockWidget
{
    Q_OBJECT
public:
    SamplingDevicesDock(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~SamplingDevicesDock();

    void addDevice(int deviceType, int deviceTabIndex);
    void removeLastDevice();
    void setCurrentTabIndex(int deviceTabIndex);
    void setSelectedDeviceIndex(int deviceTabIndex, int deviceIndex);

private:
    struct DeviceInfo
    {
        DeviceInfo(int deviceType, int deviceTabIndex, SamplingDeviceDialog *samplingDeviceDialog) :
            m_deviceType(deviceType),
            m_deviceTabIndex(deviceTabIndex),
            m_samplingDeviceDialog(samplingDeviceDialog)
        {}
        DeviceInfo(const DeviceInfo& other) :
            m_deviceType(other.m_deviceType),
            m_deviceTabIndex(other.m_deviceTabIndex),
            m_samplingDeviceDialog(other.m_samplingDeviceDialog)
        {}
        int m_deviceType;
        int m_deviceTabIndex;
        SamplingDeviceDialog *m_samplingDeviceDialog;
    };

    QPushButton *m_changeDeviceButton;
    QPushButton *m_reloadDeviceButton;
    QWidget *m_titleBar;
    QHBoxLayout *m_titleBarLayout;
    QLabel *m_titleLabel;
    QPushButton *m_normalButton;
    QPushButton *m_closeButton;
    QList<DeviceInfo> m_devicesInfo;
    int m_currentTabIndex;

private slots:
    void toggleFloating();
    void reloadDevice();
    void openChangeDeviceDialog();

signals:
    void deviceChanged(int deviceType, int deviceTabIndex, int newDeviceIndex);
};

#endif // SDRGUI_GUI_SAMPLINGDEVICESDOCK_H_