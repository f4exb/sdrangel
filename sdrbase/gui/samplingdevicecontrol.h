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

class SDRANGEL_API SamplingDeviceControl : public QWidget {
    Q_OBJECT

public:
    explicit SamplingDeviceControl(QWidget* parent = NULL);
    ~SamplingDeviceControl();

    void setPluginManager(PluginManager *pluginManager) { m_pluginManager = pluginManager; }
    void setDeviceAPI(DeviceSourceAPI *devieAPI) { m_deviceAPI = devieAPI; }
    QComboBox *getDeviceSelector();
    QPushButton *getDeviceSelectionConfirm();
    QComboBox *getChannelSelector();
    QPushButton *getAddChannelButton();

private:
    Ui::SamplingDeviceControl* ui;
    PluginManager *m_pluginManager;
    DeviceSourceAPI *m_deviceAPI;
};


#endif /* SDRBASE_GUI_SAMPLINGDEVICECONTROL_H_ */
