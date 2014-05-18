#ifndef INCLUDE_GNURADIOPLUGIN_H
#define INCLUDE_GNURADIOPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class GNURadioPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "org.osmocom.sdr.samplesource.gr-osmosdr")

public:
	explicit GNURadioPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	SampleSourceDevices enumSampleSources();
	PluginGUI* createSampleSource(const QString& sourceName, const QByteArray& address);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_GNURADIOPLUGIN_H
