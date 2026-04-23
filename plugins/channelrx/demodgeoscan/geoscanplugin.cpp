#include "geoscanplugin.h"
#include "geoscandemod.h"
#include "plugin/pluginapi.h"

const PluginDescriptor GeoscanPlugin::m_pluginDescriptor = {
    GeoscanDemod::m_channelId,
    QString("Geoscan Satellites Decoder"),
    QString("1.0.0"),
    QString("(c) Your Team"),
    QString(""),
    true,
    QString("sdrangel.channel.geoscandemod")
};

GeoscanPlugin::GeoscanPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& GeoscanPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void GeoscanPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerRxChannel(
        GeoscanDemod::m_channelIdURI,
        GeoscanDemod::m_channelId,
        this
    );
}

void GeoscanPlugin::createRxChannel(DeviceAPI *deviceAPI,
                                    BasebandSampleSink **bs,
                                    ChannelAPI **cs) const
{
    GeoscanDemod *demod = new GeoscanDemod(deviceAPI);
    *bs = demod;
    *cs = demod;
}

ChannelGUI* GeoscanPlugin::createRxChannelGUI(DeviceUISet *deviceUISet,
                                              BasebandSampleSink *rxChannel) const
{
    (void)deviceUISet; (void)rxChannel;
    return nullptr;
}

ChannelWebAPIAdapter* GeoscanPlugin::createChannelWebAPIAdapter() const
{
    return nullptr;
}