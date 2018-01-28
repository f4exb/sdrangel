///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>
#include <QSysInfo>
#include <unistd.h>

#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "device/deviceset.h"
#include "device/deviceenumerator.h"
#include "plugin/pluginmanager.h"
#include "loggerwithfile.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"
#include "webapi/webapiadaptersrv.h"

#include "maincore.h"

MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteInstance, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgLoadPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSavePreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeletePreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgRemoveLastDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSetDevice, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddChannel, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteChannel, Message)

MainCore *MainCore::m_instance = 0;

MainCore::MainCore(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent) :
    QObject(parent),
    m_settings(),
    m_masterTabIndex(-1),
    m_dspEngine(DSPEngine::instance()),
    m_lastEngineState((DSPDeviceSourceEngine::State)-1),
    m_logger(logger)
{
    qDebug() << "MainCore::MainCore: start";

    m_instance = this;
    m_settings.setAudioDeviceInfo(&m_audioDeviceInfo);

    m_pluginManager = new PluginManager(this);
    m_pluginManager->loadPlugins(QString("pluginssrv"));

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);
    m_masterTimer.start(50);

	loadSettings();

    m_apiAdapter = new WebAPIAdapterSrv(*this);
    m_requestMapper = new WebAPIRequestMapper(this);
    m_requestMapper->setAdapter(m_apiAdapter);
    m_apiServer = new WebAPIServer(parser.getServerAddress(), parser.getServerPort(), m_requestMapper);
    m_apiServer->start();

    qDebug() << "MainCore::MainCore: end";
}

MainCore::~MainCore()
{
	m_apiServer->stop();
	m_settings.save();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_pluginManager;

    qDebug() << "MainCore::~MainCore: end";
    delete m_logger;
}

bool MainCore::handleMessage(const Message& cmd)
{
    if (MsgDeleteInstance::match(cmd))
    {
        while (m_deviceSets.size() > 0)
        {
            removeLastDevice();
        }

        emit finished();
        return true;
    }
    else if (MsgLoadPreset::match(cmd))
    {
        MsgLoadPreset& notif = (MsgLoadPreset&) cmd;
        loadPresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        return true;
    }
    else if (MsgSavePreset::match(cmd))
    {
        MsgSavePreset& notif = (MsgSavePreset&) cmd;
        savePresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        m_settings.sortPresets();
        m_settings.save();
        return true;
    }
    else if (MsgDeletePreset::match(cmd))
    {
        MsgDeletePreset& notif = (MsgDeletePreset&) cmd;
        const Preset *presetToDelete = notif.getPreset();
        // remove preset from settings
        m_settings.deletePreset(presetToDelete);
        return true;
    }
    else if (MsgAddDeviceSet::match(cmd))
    {
        MsgAddDeviceSet& notif = (MsgAddDeviceSet&) cmd;

        if (notif.isTx()) {
            addSinkDevice();
        } else {
            addSourceDevice();
        }

        return true;
    }
    else if (MsgRemoveLastDeviceSet::match(cmd))
    {
        if (m_deviceSets.size() > 0) {
            removeLastDevice();
        }

        return true;
    }
    else if (MsgSetDevice::match(cmd))
    {
        MsgSetDevice& notif = (MsgSetDevice&) cmd;

        if (notif.isTx()) {
            changeSampleSink(notif.getDeviceSetIndex(), notif.getDeviceIndex());
        } else {
            changeSampleSource(notif.getDeviceSetIndex(), notif.getDeviceIndex());
        }

        return true;
    }
    else if (MsgAddChannel::match(cmd))
    {
        MsgAddChannel& notif = (MsgAddChannel&) cmd;
        addChannel(notif.getDeviceSetIndex(), notif.getChannelRegistrationIndex());
        return true;
    }
    else if (MsgDeleteChannel::match(cmd))
    {
        MsgDeleteChannel& notif = (MsgDeleteChannel&) cmd;
        deleteChannel(notif.getDeviceSetIndex(), notif.getChannelIndex());
        return true;
    }
    else
    {
        return false;
    }
}

void MainCore::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("MainCore::handleMessages: message: %s", message->getIdentifier());
        handleMessage(*message);
        delete message;
    }
}

void MainCore::loadSettings()
{
	qDebug() << "MainCore::loadSettings";

    m_settings.load();
    m_settings.sortPresets();
    setLoggingOptions();
}

void MainCore::setLoggingOptions()
{
    m_logger->setConsoleMinMessageLevel(m_settings.getConsoleMinLogLevel());

    if (m_settings.getUseLogFile())
    {
        qtwebapp::FileLoggerSettings fileLoggerSettings; // default values

        if (m_logger->hasFileLogger()) {
            fileLoggerSettings = m_logger->getFileLoggerSettings(); // values from file logger if it exists
        }

        fileLoggerSettings.fileName = m_settings.getLogFileName(); // put new values
        m_logger->createOrSetFileLogger(fileLoggerSettings, 2000); // create file logger if it does not exist and apply settings in any case
    }

    if (m_logger->hasFileLogger()) {
        m_logger->setFileMinMessageLevel(m_settings.getFileMinLogLevel());
    }

    m_logger->setUseFileLogger(m_settings.getUseLogFile());

    if (m_settings.getUseLogFile())
    {
#if QT_VERSION >= 0x050400
        QString appInfoStr(tr("%1 %2 Qt %3 %4b %5 %6 DSP Rx:%7b Tx:%8b PID %9")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(QSysInfo::currentCpuArchitecture())
                .arg(QSysInfo::prettyProductName())
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_TX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid()));
#else
        QString appInfoStr(tr("%1 %2 Qt %3 %4b DSP Rx:%5b Tx:%6b PID %7")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_RX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid());
 #endif
        m_logger->logToFile(QtInfoMsg, appInfoStr);
    }
}

void MainCore::addSinkDevice()
{
    DSPDeviceSinkEngine *dspDeviceSinkEngine = m_dspEngine->addDeviceSinkEngine();
    dspDeviceSinkEngine->start();

    uint dspDeviceSinkEngineUID =  dspDeviceSinkEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSinkEngineUID);

    int deviceTabIndex = m_deviceSets.size();
    m_deviceSets.push_back(new DeviceSet(deviceTabIndex));
    m_deviceSets.back()->m_deviceSourceEngine = 0;
    m_deviceSets.back()->m_deviceSinkEngine = dspDeviceSinkEngine;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "T%d", deviceTabIndex);

    DeviceSinkAPI *deviceSinkAPI = new DeviceSinkAPI(deviceTabIndex, dspDeviceSinkEngine);

    m_deviceSets.back()->m_deviceSourceAPI = 0;
    m_deviceSets.back()->m_deviceSinkAPI = deviceSinkAPI;
    QList<QString> channelNames;

    // create a file sink by default
    int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileSinkDeviceIndex();
    PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
    m_deviceSets.back()->m_deviceSinkAPI->setSampleSinkSequence(samplingDevice.sequence);
    m_deviceSets.back()->m_deviceSinkAPI->setNbItems(samplingDevice.deviceNbItems);
    m_deviceSets.back()->m_deviceSinkAPI->setItemIndex(samplingDevice.deviceItemIndex);
    m_deviceSets.back()->m_deviceSinkAPI->setHardwareId(samplingDevice.hardwareId);
    m_deviceSets.back()->m_deviceSinkAPI->setSampleSinkId(samplingDevice.id);
    m_deviceSets.back()->m_deviceSinkAPI->setSampleSinkSerial(samplingDevice.serial);
    m_deviceSets.back()->m_deviceSinkAPI->setSampleSinkDisplayName(samplingDevice.displayedName);
    m_deviceSets.back()->m_deviceSinkAPI->setSampleSinkPluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(fileSinkDeviceIndex));

    // delete previous plugin instance
    //m_deviceSets.back()->m_deviceSinkAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput()

    DeviceSampleSink *sink = m_deviceSets.back()->m_deviceSinkAPI->getPluginInterface()->createSampleSinkPluginInstanceOutput(
            m_deviceSets.back()->m_deviceSinkAPI->getSampleSinkId(), m_deviceSets.back()->m_deviceSinkAPI);
    m_deviceSets.back()->m_deviceSinkAPI->setSampleSink(sink);
}

void MainCore::addSourceDevice()
{
    DSPDeviceSourceEngine *dspDeviceSourceEngine = m_dspEngine->addDeviceSourceEngine();
    dspDeviceSourceEngine->start();

    uint dspDeviceSourceEngineUID =  dspDeviceSourceEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSourceEngineUID);

    int deviceTabIndex = m_deviceSets.size();
    m_deviceSets.push_back(new DeviceSet(deviceTabIndex));
    m_deviceSets.back()->m_deviceSourceEngine = dspDeviceSourceEngine;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "R%d", deviceTabIndex);

    DeviceSourceAPI *deviceSourceAPI = new DeviceSourceAPI(deviceTabIndex, dspDeviceSourceEngine);

    m_deviceSets.back()->m_deviceSourceAPI = deviceSourceAPI;

    // Create a file source instance by default
    int fileSourceDeviceIndex = DeviceEnumerator::instance()->getFileSourceDeviceIndex();
    PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(fileSourceDeviceIndex);
    m_deviceSets.back()->m_deviceSourceAPI->setSampleSourceSequence(samplingDevice.sequence);
    m_deviceSets.back()->m_deviceSourceAPI->setNbItems(samplingDevice.deviceNbItems);
    m_deviceSets.back()->m_deviceSourceAPI->setItemIndex(samplingDevice.deviceItemIndex);
    m_deviceSets.back()->m_deviceSourceAPI->setHardwareId(samplingDevice.hardwareId);
    m_deviceSets.back()->m_deviceSourceAPI->setSampleSourceId(samplingDevice.id);
    m_deviceSets.back()->m_deviceSourceAPI->setSampleSourceSerial(samplingDevice.serial);
    m_deviceSets.back()->m_deviceSourceAPI->setSampleSourceDisplayName(samplingDevice.displayedName);
    m_deviceSets.back()->m_deviceSourceAPI->setSampleSourcePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(fileSourceDeviceIndex));

    DeviceSampleSource *source = m_deviceSets.back()->m_deviceSourceAPI->getPluginInterface()->createSampleSourcePluginInstanceInput(
            m_deviceSets.back()->m_deviceSourceAPI->getSampleSourceId(), m_deviceSets.back()->m_deviceSourceAPI);
    m_deviceSets.back()->m_deviceSourceAPI->setSampleSource(source);
}

void MainCore::removeLastDevice()
{
    if (m_deviceSets.back()->m_deviceSourceEngine) // source set
    {
        DSPDeviceSourceEngine *lastDeviceEngine = m_deviceSets.back()->m_deviceSourceEngine;
        lastDeviceEngine->stopAcquistion();

        // deletes old UI and input object
        m_deviceSets.back()->freeRxChannels();      // destroys the channel instances
        m_deviceSets.back()->m_deviceSourceAPI->resetSampleSourceId();
        m_deviceSets.back()->m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                m_deviceSets.back()->m_deviceSourceAPI->getSampleSource());
        m_deviceSets.back()->m_deviceSourceAPI->clearBuddiesLists(); // clear old API buddies lists

        delete m_deviceSets.back();

        lastDeviceEngine->stop();
        m_dspEngine->removeLastDeviceSourceEngine();
    }
    else if (m_deviceSets.back()->m_deviceSinkEngine) // sink set
    {
        DSPDeviceSinkEngine *lastDeviceEngine = m_deviceSets.back()->m_deviceSinkEngine;
        lastDeviceEngine->stopGeneration();

        // deletes old UI and output object
        m_deviceSets.back()->freeTxChannels();
        m_deviceSets.back()->m_deviceSinkAPI->resetSampleSinkId();
        m_deviceSets.back()->m_deviceSinkAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                m_deviceSets.back()->m_deviceSinkAPI->getSampleSink());
        m_deviceSets.back()->m_deviceSinkAPI->clearBuddiesLists(); // clear old API buddies lists

        delete m_deviceSets.back();

        lastDeviceEngine->stop();
        m_dspEngine->removeLastDeviceSinkEngine();
    }

    m_deviceSets.pop_back();
}

void MainCore::changeSampleSource(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainCore::changeSampleSource: deviceSet at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceSourceAPI->saveSourceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceSet->m_deviceSourceAPI->stopAcquisition();

        // deletes old UI and input object
        deviceSet->m_deviceSourceAPI->resetSampleSourceId();
        deviceSet->m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                deviceSet->m_deviceSourceAPI->getSampleSource());
        deviceSet->m_deviceSourceAPI->clearBuddiesLists(); // clear old API buddies lists

        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(selectedDeviceIndex);
        deviceSet->m_deviceSourceAPI->setSampleSourceSequence(samplingDevice.sequence);
        deviceSet->m_deviceSourceAPI->setNbItems(samplingDevice.deviceNbItems);
        deviceSet->m_deviceSourceAPI->setItemIndex(samplingDevice.deviceItemIndex);
        deviceSet->m_deviceSourceAPI->setHardwareId(samplingDevice.hardwareId);
        deviceSet->m_deviceSourceAPI->setSampleSourceId(samplingDevice.id);
        deviceSet->m_deviceSourceAPI->setSampleSourceSerial(samplingDevice.serial);
        deviceSet->m_deviceSourceAPI->setSampleSourceDisplayName(samplingDevice.displayedName);
        deviceSet->m_deviceSourceAPI->setSampleSourcePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(selectedDeviceIndex));

        // add to buddies list
        std::vector<DeviceSet*>::iterator it = m_deviceSets.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceSets.end(); ++it)
        {
            if (*it != deviceSet) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceSet->m_deviceSourceAPI->getHardwareId() == (*it)->m_deviceSourceAPI->getHardwareId()) &&
                        (deviceSet->m_deviceSourceAPI->getSampleSourceSerial() == (*it)->m_deviceSourceAPI->getSampleSourceSerial()))
                    {
                        (*it)->m_deviceSourceAPI->addSourceBuddy(deviceSet->m_deviceSourceAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceSet->m_deviceSourceAPI->getHardwareId() == (*it)->m_deviceSinkAPI->getHardwareId()) &&
                        (deviceSet->m_deviceSourceAPI->getSampleSourceSerial() == (*it)->m_deviceSinkAPI->getSampleSinkSerial()))
                    {
                        (*it)->m_deviceSinkAPI->addSourceBuddy(deviceSet->m_deviceSourceAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceSet->m_deviceSourceAPI->setBuddyLeader(true);
        }

        // constructs new GUI and input object
        DeviceSampleSource *source = deviceSet->m_deviceSourceAPI->getPluginInterface()->createSampleSourcePluginInstanceInput(
                deviceSet->m_deviceSourceAPI->getSampleSourceId(), deviceSet->m_deviceSourceAPI);
        deviceSet->m_deviceSourceAPI->setSampleSource(source);

        deviceSet->m_deviceSourceAPI->loadSourceSettings(m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainCore::changeSampleSink(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainCore::changeSampleSink: device set at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceSinkAPI->saveSinkSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceSet->m_deviceSinkAPI->stopGeneration();

        // deletes old UI and output object
        deviceSet->m_deviceSinkAPI->resetSampleSinkId();
        deviceSet->m_deviceSinkAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                deviceSet->m_deviceSinkAPI->getSampleSink());
        deviceSet->m_deviceSinkAPI->clearBuddiesLists(); // clear old API buddies lists

        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(selectedDeviceIndex);
        deviceSet->m_deviceSinkAPI->setSampleSinkSequence(samplingDevice.sequence);
        deviceSet->m_deviceSinkAPI->setNbItems(samplingDevice.deviceNbItems);
        deviceSet->m_deviceSinkAPI->setItemIndex(samplingDevice.deviceItemIndex);
        deviceSet->m_deviceSinkAPI->setHardwareId(samplingDevice.hardwareId);
        deviceSet->m_deviceSinkAPI->setSampleSinkId(samplingDevice.id);
        deviceSet->m_deviceSinkAPI->setSampleSinkSerial(samplingDevice.serial);
        deviceSet->m_deviceSinkAPI->setSampleSinkDisplayName(samplingDevice.displayedName);
        deviceSet->m_deviceSinkAPI->setSampleSinkPluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(selectedDeviceIndex));

        // add to buddies list
        std::vector<DeviceSet*>::iterator it = m_deviceSets.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceSets.end(); ++it)
        {
            if (*it != deviceSet) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceSet->m_deviceSinkAPI->getHardwareId() == (*it)->m_deviceSourceAPI->getHardwareId()) &&
                        (deviceSet->m_deviceSinkAPI->getSampleSinkSerial() == (*it)->m_deviceSourceAPI->getSampleSourceSerial()))
                    {
                        (*it)->m_deviceSourceAPI->addSinkBuddy(deviceSet->m_deviceSinkAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceSet->m_deviceSinkAPI->getHardwareId() == (*it)->m_deviceSinkAPI->getHardwareId()) &&
                        (deviceSet->m_deviceSinkAPI->getSampleSinkSerial() == (*it)->m_deviceSinkAPI->getSampleSinkSerial()))
                    {
                        (*it)->m_deviceSinkAPI->addSinkBuddy(deviceSet->m_deviceSinkAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceSet->m_deviceSinkAPI->setBuddyLeader(true);
        }

        // constructs new GUI and output object
        DeviceSampleSink *sink = deviceSet->m_deviceSinkAPI->getPluginInterface()->createSampleSinkPluginInstanceOutput(
                deviceSet->m_deviceSinkAPI->getSampleSinkId(), deviceSet->m_deviceSinkAPI);
        deviceSet->m_deviceSinkAPI->setSampleSink(sink);

        deviceSet->m_deviceSinkAPI->loadSinkSettings(m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainCore::addChannel(int deviceSetIndex, int selectedChannelIndex)
{
    if (deviceSetIndex >= 0)
    {
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // source device => Rx channels
        {
            deviceSet->addRxChannel(selectedChannelIndex, m_pluginManager->getPluginAPI());
        }
        else if (deviceSet->m_deviceSinkEngine) // sink device => Tx channels
        {
            deviceSet->addTxChannel(selectedChannelIndex, m_pluginManager->getPluginAPI());
        }
    }
}

void MainCore::deleteChannel(int deviceSetIndex, int channelIndex)
{
    if (deviceSetIndex >= 0)
    {
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // source device => Rx channels
        {
            deviceSet->deleteRxChannel(channelIndex);
        }
        else if (deviceSet->m_deviceSinkEngine) // sink device => Tx channels
        {
            deviceSet->deleteTxChannel(channelIndex);
        }
    }
}

void MainCore::loadPresetSettings(const Preset* preset, int tabIndex)
{
	qDebug("MainCore::loadPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	if (tabIndex >= 0)
	{
        DeviceSet *deviceSet = m_deviceSets[tabIndex];

        if (deviceSet->m_deviceSourceEngine) // source device
        {
        	deviceSet->m_deviceSourceAPI->loadSourceSettings(preset);
        	deviceSet->loadRxChannelSettings(preset, m_pluginManager->getPluginAPI());
        }
        else if (deviceSet->m_deviceSinkEngine) // sink device
        {
        	deviceSet->m_deviceSinkAPI->loadSinkSettings(preset);
        	deviceSet->loadTxChannelSettings(preset, m_pluginManager->getPluginAPI());
        }
	}
}

void MainCore::savePresetSettings(Preset* preset, int tabIndex)
{
    qDebug("MainCore::savePresetSettings: preset [%s | %s]",
        qPrintable(preset->getGroup()),
        qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    DeviceSet *deviceSet = m_deviceSets[tabIndex];

    if (deviceSet->m_deviceSourceEngine) // source device
    {
        preset->clearChannels();
        deviceSet->saveRxChannelSettings(preset);
        deviceSet->m_deviceSourceAPI->saveSourceSettings(preset);
    }
    else if (deviceSet->m_deviceSinkEngine) // sink device
    {
        preset->clearChannels();
        preset->setSourcePreset(false);
        deviceSet->saveTxChannelSettings(preset);
        deviceSet->m_deviceSinkAPI->saveSinkSettings(preset);
    }
}

