///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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


#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "datvdemodgui.h"
#include "datvdemodplugin.h"
#include "datvdemodwebapiadapter.h"

const PluginDescriptor DATVDemodPlugin::m_ptrPluginDescriptor =
{
    DATVDemod::m_channelId,
    QStringLiteral("DATV Demodulator"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) F4HKW for SDRAngel using LeanSDR framework (c) F4DAV"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

DATVDemodPlugin::DATVDemodPlugin(QObject* ptrParent) :
    QObject(ptrParent),
    m_ptrPluginAPI(nullptr)
{

}

const PluginDescriptor& DATVDemodPlugin::getPluginDescriptor() const
{
    return m_ptrPluginDescriptor;

}

void DATVDemodPlugin::initPlugin(PluginAPI* ptrPluginAPI)
{
    m_ptrPluginAPI = ptrPluginAPI;

    // register DATV demodulator
    m_ptrPluginAPI->registerRxChannel(DATVDemod::m_channelIdURI, DATVDemod::m_channelId, this);
}

void DATVDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		DATVDemod *instance = new DATVDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

ChannelGUI* DATVDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return DATVDemodGUI::create(m_ptrPluginAPI, deviceUISet, rxChannel);
}

ChannelWebAPIAdapter* DATVDemodPlugin::createChannelWebAPIAdapter() const
{
	return new DATVDemodWebAPIAdapter();
}
