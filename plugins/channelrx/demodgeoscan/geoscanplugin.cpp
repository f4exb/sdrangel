#include "geoscanplugin.h"
#include "geoscandemod.h"
#include "plugin/pluginapi.h"
#include "geoscandemodgui.h"

// Исправлено: используем const char* вместо QString, добавлено поле isTxChannel
const PluginDescriptor GeoscanPlugin::m_pluginDescriptor = {
    GeoscanDemod::m_channelId,
    "Geoscan Satellites Decoder",
    "1.0.0",
    "(c) Our Team",
    "",
    "",
    "sdrangel.channel.geoscandemod"
};

GeoscanPlugin::GeoscanPlugin(QObject* parent) : QObject(parent) {}

const PluginDescriptor& GeoscanPlugin::getPluginDescriptor() const { return m_pluginDescriptor; }

void GeoscanPlugin::initPlugin(PluginAPI* pluginAPI) {
    pluginAPI->registerRxChannel(
        GeoscanDemod::m_channelIdURI,
        GeoscanDemod::m_channelId,
        this
    );
}

void GeoscanPlugin::createRxChannel(DeviceAPI *deviceAPI,
                                    BasebandSampleSink **bs,
                                    ChannelAPI **cs) const {
    GeoscanDemod *demod = new GeoscanDemod(deviceAPI);
    *bs = demod;
    *cs = demod;
}

ChannelGUI* GeoscanPlugin::createRxChannelGUI(DeviceUISet *deviceUISet,
                                              BasebandSampleSink *rxChannel) const {
    return new GeoscanDemodGUI(deviceUISet, rxChannel);
}

ChannelWebAPIAdapter* GeoscanPlugin::createChannelWebAPIAdapter() const {
    return nullptr;
}
