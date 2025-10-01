#ifndef INCLUDE_FILESINKTXTPLUGIN_H
#define INCLUDE_FILESINKTXTPLUGIN_H

#include <QtPlugin>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSink;

class FileSinkTxtPlugin : public QObject, PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "sdrangel.channel.filesinktxt")

public:
    explicit FileSinkTxtPlugin(QObject* parent = nullptr);

    const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* pluginAPI);

    virtual void createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const;
    virtual ChannelGUI* createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
    static const PluginDescriptor m_pluginDescriptor;
    PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_FILESINKTXTPLUGIN_H
