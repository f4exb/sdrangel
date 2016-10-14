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
class DeviceSourceAPI;
class DeviceSinkAPI;
class PluginGUI;
class QWidget;

class PluginInterface {
public:
	struct SamplingDevice
	{
		QString displayedName;
		QString id;
		QString serial;
		int sequence;

		SamplingDevice(const QString& _displayedName,
				const QString& _id,
				const QString& _serial,
				int _sequence) :
			displayedName(_displayedName),
			id(_id),
			serial(_serial),
			sequence(_sequence)
		{ }
	};
	typedef QList<SamplingDevice> SamplingDevices;

	virtual ~PluginInterface() { };

	virtual const PluginDescriptor& getPluginDescriptor() const = 0;
	virtual void initPlugin(PluginAPI* pluginAPI) = 0;

	// channel Rx plugins
	virtual PluginGUI* createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI) { return 0; }

	// channel Tx plugins
	virtual PluginGUI* createTxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI) { return 0; }

	// device source plugins only
	virtual SamplingDevices enumSampleSources() { return SamplingDevices(); }
	virtual PluginGUI* createSampleSourcePluginGUI(const QString& sourceId, QWidget **widget, DeviceSourceAPI *deviceAPI) { return 0; }

	// device sink plugins only
	virtual SamplingDevices enumSampleSinks() { return SamplingDevices(); }
	virtual PluginGUI* createSampleSinkPluginGUI(const QString& sinkId, QWidget **widget, DeviceSinkAPI *deviceAPI) { return 0; }
};

Q_DECLARE_INTERFACE(PluginInterface, "SDRangel.PluginInterface/0.1");

#endif // INCLUDE_PLUGININTERFACE_H
