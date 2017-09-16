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
class PluginInstanceUI;
class QWidget;
class DeviceSampleSource;
class DeviceSampleSink;

class PluginInterface {
public:
	struct SamplingDevice
	{
		QString displayedName;
		QString hardwareId;
		QString id;
		QString serial;
		int sequence;

		SamplingDevice(const QString& _displayedName,
                const QString& _hardwareId,
				const QString& _id,
				const QString& _serial,
				int _sequence) :
			displayedName(_displayedName),
			hardwareId(_hardwareId),
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
	virtual PluginInstanceUI* createRxChannel(const QString& channelName __attribute__((unused)), DeviceSourceAPI *deviceAPI __attribute__((unused)) ) { return 0; }

	// channel Tx plugins
	virtual PluginInstanceUI* createTxChannel(const QString& channelName __attribute__((unused)), DeviceSinkAPI *deviceAPI __attribute__((unused)) ) { return 0; }

	// device source plugins only
	virtual SamplingDevices enumSampleSources() { return SamplingDevices(); }
	virtual PluginInstanceUI* createSampleSourcePluginInstanceGUI(const QString& sourceId __attribute__((unused)), QWidget **widget __attribute__((unused)), DeviceSourceAPI *deviceAPI __attribute__((unused))) { return 0; }
	virtual DeviceSampleSource* createSampleSourcePluginInstanceInput(const QString& sourceId __attribute__((unused)), DeviceSourceAPI *deviceAPI __attribute__((unused))) { return 0; } // creates the input "core"
	virtual void deleteSampleSourcePluginInstanceGUI(PluginInstanceUI *ui);
	virtual void deleteSampleSourcePluginInstanceInput(DeviceSampleSource *source);

	// device sink plugins only
	virtual SamplingDevices enumSampleSinks() { return SamplingDevices(); }
	virtual PluginInstanceUI* createSampleSinkPluginInstanceGUI(const QString& sinkId __attribute__((unused)), QWidget **widget __attribute__((unused)), DeviceSinkAPI *deviceAPI __attribute__((unused))) { return 0; }
    virtual DeviceSampleSink* createSampleSinkPluginInstanceOutput(const QString& sinkId __attribute__((unused)), DeviceSinkAPI *deviceAPI __attribute__((unused))) { return 0; } // creates the output "core"
    virtual void deleteSampleSinkPluginInstanceGUI(PluginInstanceUI *ui);
};

Q_DECLARE_INTERFACE(PluginInterface, "SDRangel.PluginInterface/0.1");

#endif // INCLUDE_PLUGININTERFACE_H
