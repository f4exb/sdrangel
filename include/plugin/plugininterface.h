#ifndef INCLUDE_PLUGININTERFACE_H
#define INCLUDE_PLUGININTERFACE_H

#include <QtPlugin>
#include <QString>

struct PluginDescriptor {
	// general plugin description
	const QString displayedName;
	const QString version;
	const QString copyright;
	const QString website;
	bool licenseIsGPL;
	const QString sourceCodeURL;
};

class PluginAPI;
class PluginGUI;

class PluginInterface {
public:
	struct SampleSourceDevice {
		QString displayedName;
		QString name;
		QByteArray address;

		SampleSourceDevice(const QString& _displayedName, const QString& _name, const QByteArray& _address) :
			displayedName(_displayedName),
			name(_name),
			address(_address)
		{ }
	};
	typedef QList<SampleSourceDevice> SampleSourceDevices;

	virtual ~PluginInterface() { };

	virtual const PluginDescriptor& getPluginDescriptor() const = 0;
	virtual void initPlugin(PluginAPI* pluginAPI) = 0;

	virtual PluginGUI* createChannel(const QString& channelName) { return NULL; }

	virtual SampleSourceDevices enumSampleSources() { return SampleSourceDevices(); }
	virtual PluginGUI* createSampleSource(const QString& sourceName, const QByteArray& address) { return NULL; }
};

Q_DECLARE_INTERFACE(PluginInterface, "de.maintech.SDRangelove.PluginInterface/0.1");

#endif // INCLUDE_PLUGININTERFACE_H
