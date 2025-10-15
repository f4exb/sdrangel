#include <QtPlugin>
#ifndef SERVER_MODE
#include <QAction>

#include "gui/glspectrum.h"
#endif
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

#ifndef SERVER_MODE
#include "device/deviceuiset.h"
#endif

#include "filesinktxt.h"
#include "filesinktxtplugin.h"

#ifndef SERVER_MODE
#include "filesinktxtgui.h"
#endif

#include "filesinktxtwebapiadapter.h"

const PluginDescriptor FileSinkTxtPlugin::m_pluginDescriptor = {
    FileSinkTxt::m_channelId,
    QStringLiteral("File Sink Text"),
    QStringLiteral("7.22.8"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

FileSinkTxtPlugin::FileSinkTxtPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& FileSinkTxtPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void FileSinkTxtPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerRxChannel(FileSinkTxt::m_channelIdURI, FileSinkTxt::m_channelId, this);
}

void FileSinkTxtPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        FileSinkTxt *instance = new FileSinkTxt(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifndef SERVER_MODE
ChannelGUI* FileSinkTxtPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return FileSinkTxtGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* FileSinkTxtPlugin::createChannelWebAPIAdapter() const
{
    return new FileSinkTxtWebAPIAdapter();
}
