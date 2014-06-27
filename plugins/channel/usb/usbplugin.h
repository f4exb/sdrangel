#ifndef INCLUDE_USBPLUGIN_H
#define INCLUDE_USBPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class USBPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.channel.usb")

public:
	explicit USBPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceUSB();
};

#endif // INCLUDE_USBPLUGIN_H
