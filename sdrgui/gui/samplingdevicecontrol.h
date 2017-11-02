///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_GUI_SAMPLINGDEVICECONTROL_H_
#define SDRBASE_GUI_SAMPLINGDEVICECONTROL_H_


#include <QWidget>
#include <QComboBox>
#include <QPushButton>

#include "util/export.h"

namespace Ui {
    class SamplingDeviceControl;
}

class ChannelMarker;
class PluginManager;
class DeviceSourceAPI;
class DeviceSinkAPI;

class SDRANGEL_API SamplingDeviceControl : public QWidget {
    Q_OBJECT

public:
    explicit SamplingDeviceControl(int tabIndex, bool rxElseTx, QWidget* parent = 0);
    ~SamplingDeviceControl();

    int getSelectedDeviceIndex() const { return m_selectedDeviceIndex; }
    void setSelectedDeviceIndex(int index);
    void removeSelectedDeviceIndex();

    void setPluginManager(PluginManager *pluginManager) { m_pluginManager = pluginManager; }
    QComboBox *getChannelSelector();
    QPushButton *getAddChannelButton();

private slots:
    void on_deviceChange_clicked();
    void on_deviceReload_clicked();

private:
    Ui::SamplingDeviceControl* ui;
    PluginManager *m_pluginManager;
    int m_deviceTabIndex;
    bool m_rxElseTx;
    int m_selectedDeviceIndex;

signals:
    void changed();
};


#endif /* SDRBASE_GUI_SAMPLINGDEVICECONTROL_H_ */
