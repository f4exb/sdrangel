#include "ft8plugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"
#ifndef SERVER_MODE
#include "ft8demodgui.h"
#endif
#include "ft8demod.h"
#include "ft8demodwebapiadapter.h"
#include "ft8plugin.h"

const PluginDescriptor FT8Plugin::m_pluginDescriptor = {
    FT8Demod::m_channelId,
	QStringLiteral("FT8 Demodulator"),
    QStringLiteral("7.9.0"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

FT8Plugin::FT8Plugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& FT8Plugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FT8Plugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerRxChannel(FT8Demod::m_channelIdURI, FT8Demod::m_channelId, this);
}

void FT8Plugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		FT8Demod *instance = new FT8Demod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* FT8Plugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* FT8Plugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return FT8DemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* FT8Plugin::createChannelWebAPIAdapter() const
{
	return new FT8DemodWebAPIAdapter();
}
