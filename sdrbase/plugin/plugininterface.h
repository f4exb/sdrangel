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
class DeviceUISet;
class DeviceSinkAPI;
class PluginInstanceGUI;
class QWidget;
class DeviceSampleSource;
class DeviceSampleSink;
class BasebandSampleSink;
class BasebandSampleSource;

class PluginInterface {
public:
	struct SamplingDevice
	{
        enum SamplingDeviceType
        {
            PhysicalDevice,
            BuiltInDevice
        };

		QString displayedName;    //!< The human readable name
		QString hardwareId;       //!< The internal id that identifies the type of hardware (i.e. HackRF, BladeRF, ...)
		QString id;               //!< The internal plugin ID corresponding to the device (i.e. for HackRF input, for HackRF output ...)
		QString serial;           //!< The device serial number defined by the vendor or a fake one (SDRplay)
		int sequence;             //!< The device sequence. >0 when more than one device of the same type is connected
		SamplingDeviceType type;  //!< The sampling device type for behavior information
		bool rxElseTx;            //!< This is the Rx part else the Tx part of the device
		int deviceNbItems;        //!< Number of items (or streams) in the device. >1 for composite devices.
		int deviceItemIndex;      //!< For composite devices this is the Rx or Tx stream index. -1 if not initialized
		int claimed;              //!< This is the device set index if claimed else -1

		SamplingDevice(const QString& _displayedName,
                const QString& _hardwareId,
				const QString& _id,
				const QString& _serial,
				int _sequence,
				SamplingDeviceType _type,
				bool _rxElseTx,
				int _deviceNbItems,
				int _deviceItemIndex) :
			displayedName(_displayedName),
			hardwareId(_hardwareId),
			id(_id),
			serial(_serial),
			sequence(_sequence),
			type(_type),
			rxElseTx(_rxElseTx),
			deviceNbItems(_deviceNbItems),
			deviceItemIndex(_deviceItemIndex),
			claimed(-1)
		{ }
	};
	typedef QList<SamplingDevice> SamplingDevices;

	virtual ~PluginInterface() { };

	virtual const PluginDescriptor& getPluginDescriptor() const = 0;
	virtual void initPlugin(PluginAPI* pluginAPI) = 0;

	// channel Rx plugins

    virtual PluginInstanceGUI* createRxChannelGUI(
            const QString& channelName __attribute__((unused)),
            DeviceUISet *deviceUISet __attribute__((unused)),
            BasebandSampleSink *rxChannel __attribute__((unused)))
    { return 0; }

    virtual BasebandSampleSink* createRxChannel(
            const QString& channelName __attribute__((unused)),
            DeviceSourceAPI *deviceAPI __attribute__((unused)) )
    { return 0; }

    // channel Tx plugins

	virtual PluginInstanceGUI* createTxChannelGUI(
	        const QString& channelName __attribute__((unused)),
	        DeviceUISet *deviceUISet __attribute__((unused)),
	        BasebandSampleSource *txChannel __attribute__((unused)))
	{ return 0; }

    virtual BasebandSampleSource* createTxChannel(
            const QString& channelName __attribute__((unused)),
            DeviceSinkAPI *deviceAPI __attribute__((unused)) )
    { return 0; }

	// device source plugins only

	virtual SamplingDevices enumSampleSources() { return SamplingDevices(); }

	virtual PluginInstanceGUI* createSampleSourcePluginInstanceGUI(
	        const QString& sourceId __attribute__((unused)),
	        QWidget **widget __attribute__((unused)),
	        DeviceUISet *deviceUISet __attribute__((unused)))
	{ return 0; }

	virtual DeviceSampleSource* createSampleSourcePluginInstanceInput(const QString& sourceId __attribute__((unused)), DeviceSourceAPI *deviceAPI __attribute__((unused))) { return 0; } // creates the input "core"
	virtual void deleteSampleSourcePluginInstanceGUI(PluginInstanceGUI *ui);
	virtual void deleteSampleSourcePluginInstanceInput(DeviceSampleSource *source);

	// device sink plugins only

	virtual SamplingDevices enumSampleSinks() { return SamplingDevices(); }

	virtual PluginInstanceGUI* createSampleSinkPluginInstanceGUI(
	        const QString& sinkId __attribute__((unused)),
	        QWidget **widget __attribute__((unused)),
	        DeviceUISet *deviceUISet __attribute__((unused)))
	{ return 0; }

	virtual DeviceSampleSink* createSampleSinkPluginInstanceOutput(const QString& sinkId __attribute__((unused)), DeviceSinkAPI *deviceAPI __attribute__((unused))) { return 0; } // creates the output "core"
    virtual void deleteSampleSinkPluginInstanceGUI(PluginInstanceGUI *ui);
    virtual void deleteSampleSinkPluginInstanceOutput(DeviceSampleSink *sink);
};

Q_DECLARE_INTERFACE(PluginInterface, "SDRangel.PluginInterface/0.1");

#endif // INCLUDE_PLUGININTERFACE_H
