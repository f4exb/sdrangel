///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel Contributors                                      //
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

#include "cwmodgui.h"
#include "cwmod.h"

CWModGUI* CWModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel)
{
    return new CWModGUI(pluginAPI, deviceUISet, txChannel);
}

CWModGUI::CWModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel, QWidget* parent) :
    ChannelGUI(parent),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_cwMod(qobject_cast<CWMod*>(txChannel))
{
}

void CWModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

QByteArray CWModGUI::serialize() const
{
    return m_settings.serialize();
}

bool CWModGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool CWModGUI::handleMessage(const Message& message)
{
    (void) message;
    return false;
}
