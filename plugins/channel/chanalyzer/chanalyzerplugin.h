#ifndef INCLUDE_CHANALYZERPLUGIN_H
#define INCLUDE_CHANALYZERPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class ChannelAnalyzerPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "org.f4exb.sdrangelove.channel.chanalyzer")

public:
	explicit ChannelAnalyzerPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceChannelAnalyzer();
};

#endif // INCLUDE_CHANALYZERPLUGIN_H
