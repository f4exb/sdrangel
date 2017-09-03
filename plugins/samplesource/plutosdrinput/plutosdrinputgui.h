///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUTGUI_H_
#define PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUTGUI_H_

#include <plugin/plugininstanceui.h>
#include <QObject>
#include <QWidget>

#include "plutosdrinputsettings.h"

class DeviceSourceAPI;

class PlutoSDRInputGui : public QWidget, public PluginInstanceUI {
    Q_OBJECT

public:
    explicit PlutoSDRInputGui(DeviceSourceAPI *deviceAPI, QWidget* parent = 0);
    virtual ~PlutoSDRInputGui();

    virtual void destroy();
    virtual void setName(const QString& name);
    virtual QString getName() const;
    virtual void resetToDefaults();
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual bool handleMessage(const Message& message);

private:
    DeviceSourceAPI* m_deviceAPI;
    PlutoSDRInputSettings m_settings;
};

#endif /* PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUTGUI_H_ */
