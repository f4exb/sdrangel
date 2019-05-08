#ifndef INCLUDE_PLUGININTERFACE_H
#define INCLUDE_PLUGININTERFACE_H

#include <QtPlugin>
#include <QString>

#include "export.h"

struct SDRBASE_API PluginDescriptor {
	// general plugin description
	const QString displayedName;
	const QString version;
	const QString copyright;
	const QString website;
	bool licenseIsGPL;
	const QString sourceCodeURL;
};

class PluginAPI;
class DeviceAPI;
class DeviceUISet;
class PluginInstanceGUI;
class QWidget;
class DeviceSampleSource;
class DeviceSampleSink;
class BasebandSampleSink;
class BasebandSampleSource;
class ChannelSinkAPI;
class ChannelSourceAPI;

class SDRBASE_API PluginInterface {
public:
	struct SamplingDevice
	{
        enum SamplingDeviceType
        {
            PhysicalDevice,
            BuiltInDevice
        };

        enum StreamType
        {
            StreamSingleRx, //!< Exposes a single input stream that can be one of the streams of a physical device
            StreamSingleTx, //!< Exposes a single output stream that can be one of the streams of a physical device
            StreamAny       //!< May expose any number of input and/or output streams
        };

		QString displayedName;    //!< The human readable name
		QString hardwareId;       //!< The internal id that identifies the type of hardware (i.e. HackRF, BladeRF, ...)
		QString id;               //!< The internal plugin ID corresponding to the device (i.e. for HackRF input, for HackRF output ...)
		QString serial;           //!< The device serial number defined by the vendor or a fake one (SDRplay)
		int sequence;             //!< The device sequence. >0 when more than one device of the same type is connected
		SamplingDeviceType type;  //!< The sampling device type for behavior information
		StreamType streamType;    //!< This is the type of stream supported
		int deviceNbItems;        //!< Number of items (or streams) in the device. >1 for composite devices.
		int deviceItemIndex;      //!< For composite devices this is the Rx or Tx stream index. -1 if not initialized
		int claimed;              //!< This is the device set index if claimed else -1

		SamplingDevice(const QString& _displayedName,
                const QString& _hardwareId,
				const QString& _id,
				const QString& _serial,
				int _sequence,
				SamplingDeviceType _type,
				StreamType _streamType,
				int _deviceNbItems,
				int _deviceItemIndex) :
			displayedName(_displayedName),
			hardwareId(_hardwareId),
			id(_id),
			serial(_serial),
			sequence(_sequence),
			type(_type),
			streamType(_streamType),
			deviceNbItems(_deviceNbItems),
			deviceItemIndex(_deviceItemIndex),
			claimed(-1)
		{ }
	};
	typedef QList<SamplingDevice> SamplingDevices;

    virtual ~PluginInterface() { }

	virtual const PluginDescriptor& getPluginDescriptor() const = 0;
	virtual void initPlugin(PluginAPI* pluginAPI) = 0;

	// channel Rx plugins

    virtual PluginInstanceGUI* createRxChannelGUI(
            DeviceUISet *deviceUISet,
            BasebandSampleSink *rxChannel)
    {
        (void) deviceUISet;
        (void) rxChannel;
        return nullptr;
    }

    virtual BasebandSampleSink* createRxChannelBS(
            DeviceAPI *deviceAPI)
    {
        (void) deviceAPI;
        return nullptr;
    }

    virtual ChannelSinkAPI* createRxChannelCS(
            DeviceAPI *deviceAPI)
    {
        (void) deviceAPI;
        return nullptr;
    }

    // channel Tx plugins

	virtual PluginInstanceGUI* createTxChannelGUI(
            DeviceUISet *deviceUISet,
            BasebandSampleSource *txChannel)
    {
        (void) deviceUISet;
        (void) txChannel;
        return nullptr;
    }

    virtual BasebandSampleSource* createTxChannelBS(
            DeviceAPI *deviceAPI)
    {
        (void) deviceAPI;
        return nullptr;
    }

    virtual ChannelSourceAPI* createTxChannelCS(
            DeviceAPI *deviceAPI)
    {
        (void) deviceAPI;
        return nullptr;
    }

    // device source plugins only

	virtual SamplingDevices enumSampleSources() { return SamplingDevices(); }

	virtual PluginInstanceGUI* createSampleSourcePluginInstanceGUI(
            const QString& sourceId,
            QWidget **widget,
            DeviceUISet *deviceUISet)
    {
        (void) sourceId;
        (void) widget;
        (void) deviceUISet;
        return nullptr;
    }

    virtual DeviceSampleSource* createSampleSourcePluginInstanceInput( // creates the input "core"
            const QString& sourceId,
            DeviceAPI *deviceAPI)
    {
        (void) sourceId;
        (void) deviceAPI;
        return nullptr;
    }
	virtual void deleteSampleSourcePluginInstanceGUI(PluginInstanceGUI *ui);
	virtual void deleteSampleSourcePluginInstanceInput(DeviceSampleSource *source);

	// device sink plugins only

	virtual SamplingDevices enumSampleSinks() { return SamplingDevices(); }

	virtual PluginInstanceGUI* createSampleSinkPluginInstanceGUI(
            const QString& sinkId,
            QWidget **widget,
            DeviceUISet *deviceUISet)
    {
        (void) sinkId;
        (void) widget;
        (void) deviceUISet;
        return nullptr;
    }

    virtual DeviceSampleSink* createSampleSinkPluginInstanceOutput( // creates the output "core"
            const QString& sinkId,
            DeviceAPI *deviceAPI)
    {
        (void) sinkId;
        (void) deviceAPI;
        return nullptr;
    }

    virtual void deleteSampleSinkPluginInstanceGUI(PluginInstanceGUI *ui);
    virtual void deleteSampleSinkPluginInstanceOutput(DeviceSampleSink *sink);
};

Q_DECLARE_INTERFACE(PluginInterface, "SDRangel.PluginInterface/0.1");

#endif // INCLUDE_PLUGININTERFACE_H
