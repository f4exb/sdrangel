///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"

#include "ui_daemonsrcgui.h"
#include "daemonsrcgui.h"

DaemonSrcGUI* DaemonSrcGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    DaemonSrcGUI* gui = new DaemonSrcGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void DaemonSrcGUI::destroy()
{
    delete this;
}

void DaemonSrcGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString DaemonSrcGUI::getName() const
{
    return objectName();
}

qint64 DaemonSrcGUI::getCenterFrequency() const {
    return 0;
}

void DaemonSrcGUI::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

void DaemonSrcGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray DaemonSrcGUI::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSrcGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool DaemonSrcGUI::handleMessage(const Message& message __attribute__((unused)))
{
    return false;
}

DaemonSrcGUI::DaemonSrcGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx __attribute__((unused)), QWidget* parent) :
        RollupWidget(parent),
        ui(new Ui::DaemonSrcGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
}

DaemonSrcGUI::~DaemonSrcGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
    delete ui;
}

void DaemonSrcGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DaemonSrcGUI::applySettings(bool force __attribute((unused)))
{
}

void DaemonSrcGUI::displaySettings()
{
}
