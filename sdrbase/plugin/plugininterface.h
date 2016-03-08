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
	struct SampleSourceDevice
	{
		QString displayedName;
		QString id;
		QString serial;
		int sequence;

		SampleSourceDevice(const QString& _displayedName,
				const QString& _id,
				const QString& _serial,
				int _sequence) :
			displayedName(_displayedName),
			id(_id),
			serial(_serial),
			sequence(_sequence)
		{ }
	};
	typedef QList<SampleSourceDevice> SampleSourceDevices;

	virtual ~PluginInterface() { };

	virtual const PluginDescriptor& getPluginDescriptor() const = 0;
	virtual void initPlugin(PluginAPI* pluginAPI) = 0;

	virtual PluginGUI* createChannel(const QString& channelName) { return 0; }

	virtual SampleSourceDevices enumSampleSources() { return SampleSourceDevices(); }
	virtual PluginGUI* createSampleSourcePluginGUI(const QString& sourceId) { return 0; }
};

Q_DECLARE_INTERFACE(PluginInterface, "SDRangel.PluginInterface/0.1");

#endif // INCLUDE_PLUGININTERFACE_H
