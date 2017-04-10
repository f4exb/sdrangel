///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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


#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "atvdemodgui.h"
#include "atvdemodplugin.h"

const PluginDescriptor ATVDemodPlugin::m_ptrPluginDescriptor =
{
	QString("ATV Demodulator"),
    QString("3.3.4"),
    QString("(c) F4HKW for F4EXB / SDRAngel"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

ATVDemodPlugin::ATVDemodPlugin(QObject* ptrParent) :
    QObject(ptrParent),
    m_ptrPluginAPI(NULL)
{

}

const PluginDescriptor& ATVDemodPlugin::getPluginDescriptor() const
{
    return m_ptrPluginDescriptor;
}

void ATVDemodPlugin::initPlugin(PluginAPI* ptrPluginAPI)
{
    m_ptrPluginAPI = ptrPluginAPI;

	// register ATV demodulator
    m_ptrPluginAPI->registerRxChannel(ATVDemodGUI::m_strChannelID, this);
}

PluginGUI* ATVDemodPlugin::createRxChannel(const QString& strChannelName, DeviceSourceAPI *ptrDeviceAPI)
{
    if(strChannelName == ATVDemodGUI::m_strChannelID)
	{
        ATVDemodGUI* ptrGui = ATVDemodGUI::create(m_ptrPluginAPI, ptrDeviceAPI);
        return ptrGui;
    }
    else
    {
		return NULL;
	}
}

void ATVDemodPlugin::createInstanceDemodATV(DeviceSourceAPI *ptrDeviceAPI)
{
    ATVDemodGUI::create(m_ptrPluginAPI, ptrDeviceAPI);
}
