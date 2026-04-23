#ifndef INCLUDE_GEOSCANPLUGIN_H
#define INCLUDE_GEOSCANPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class GeoscanPlugin : public QObject, PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "sdrangel.channel.geoscandemod")

public:
    explicit GeoscanPlugin(QObject* parent = nullptr);

    const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* pluginAPI);

    virtual void createRxChannel(DeviceAPI *deviceAPI,
                                 BasebandSampleSink **bs,
                                 ChannelAPI **cs) const;
    virtual ChannelGUI* createRxChannelGUI(DeviceUISet *deviceUISet,
                                           BasebandSampleSink *rxChannel) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
    static const PluginDescriptor m_pluginDescriptor;
};

#endif // INCLUDE_GEOSCANPLUGIN_H