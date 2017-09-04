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

#include <stdio.h>

#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"
#include "ui_plutosdrinputgui.h"
#include "plutosdrinputgui.h"

PlutoSDRInputGui::PlutoSDRInputGui(DeviceSourceAPI *deviceAPI, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::PlutoSDRInputGUI),
    m_deviceAPI(deviceAPI),
    m_settings()
{
    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}

PlutoSDRInputGui::~PlutoSDRInputGui()
{
    delete m_fileSink;
    delete ui;
}

void PlutoSDRInputGui::destroy()
{
    delete this;
}

void PlutoSDRInputGui::setName(const QString& name)
{
    setObjectName(name);
}

QString PlutoSDRInputGui::getName() const
{
    return objectName();
}

void PlutoSDRInputGui::resetToDefaults()
{

}

qint64 PlutoSDRInputGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PlutoSDRInputGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
}

QByteArray PlutoSDRInputGui::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDRInputGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
//        displaySettings();
//        m_forceSettings = true;
//        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool PlutoSDRInputGui::handleMessage(const Message& message)
{
    return false;
}
