///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include <QResource>

#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "device/deviceapi.h"
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
MESSAGE_CLASS_DEFINITION(MainCore::MsgApplySettings, Message)

MainCore *MainCore::m_instance = 0;

MainCore::MainCore(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent) :
    QObject(parent),
    m_settings(),
    m_masterTabIndex(-1),
    m_dspEngine(DSPEngine::instance()),
    m_lastEngineState(DSPDeviceSourceEngine::StNotStarted),
    m_logger(logger)
{
    qDebug() << "MainCore::MainCore: start";

    m_instance = this;
    m_settings.setAudioDeviceManager(m_dspEngine->getAudioDeviceManager());
    m_settings.setAMBEEngine(m_dspEngine->getAMBEEngine());

    qDebug() << "MainCore::MainCore: create FFT factory...";
    m_dspEngine->createFFTFactory(parser.getFFTWFWisdomFileName());

    qDebug() << "MainCore::MainCore: load plugins...";
    m_pluginManager = new PluginManager(this);
    m_pluginManager->loadPlugins(QString("pluginssrv"));

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);
    m_masterTimer.start(50);

    qDebug() << "MainCore::MainCore: load setings...";
	loadSettings();

    qDebug() << "MainCore::MainCore: finishing...";
    QString applicationDirPath = QCoreApplication::instance()->applicationDirPath();

    m_apiAdapter = new WebAPIAdapterSrv(*this);
    m_requestMapper = new WebAPIRequestMapper(this);
    m_requestMapper->setAdapter(m_apiAdapter);
    m_apiServer = new WebAPIServer(parser.getServerAddress(), parser.getServerPort(), m_requestMapper);
    m_apiServer->start();

    m_dspEngine->setMIMOSupport(parser.getMIMOSupport());

    qDebug() << "MainCore::MainCore: end";
}

MainCore::~MainCore()
{
    while (m_deviceSets.size() > 0) {
        removeLastDevice();
    }

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
        int direction = notif.getDirection();

        if (direction == 1) { // Single stream Tx
            addSinkDevice();
        } else if (direction == 0) { // Single stream Rx
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

        if (notif.getDeviceType() == 1) {
            changeSampleSink(notif.getDeviceSetIndex(), notif.getDeviceIndex());
        } else if (notif.getDeviceType() == 0) {
            changeSampleSource(notif.getDeviceSetIndex(), notif.getDeviceIndex());
        } else if (notif.getDeviceType() == 2) {
            changeSampleMIMO(notif.getDeviceSetIndex(), notif.getDeviceIndex());
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
    else if (MsgApplySettings::match(cmd))
    {
        applySettings();
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

void MainCore::applySettings()
{
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
    m_deviceSets.back()->m_deviceSourceEngine = nullptr;
    m_deviceSets.back()->m_deviceSinkEngine = dspDeviceSinkEngine;
    m_deviceSets.back()->m_deviceMIMOEngine = nullptr;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "T%d", deviceTabIndex);

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleTx, deviceTabIndex, nullptr, dspDeviceSinkEngine, nullptr);

    m_deviceSets.back()->m_deviceAPI = deviceAPI;
    QList<QString> channelNames;

    // create a file sink by default
    int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileSinkDeviceIndex();
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    m_deviceSets.back()->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    m_deviceSets.back()->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    m_deviceSets.back()->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    m_deviceSets.back()->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(fileSinkDeviceIndex));

    QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        m_deviceSets.back()->m_deviceAPI->setHardwareUserArguments(userArgs);
    }

    DeviceSampleSink *sink = m_deviceSets.back()->m_deviceAPI->getPluginInterface()->createSampleSinkPluginInstance(
            m_deviceSets.back()->m_deviceAPI->getSamplingDeviceId(), m_deviceSets.back()->m_deviceAPI);
    m_deviceSets.back()->m_deviceAPI->setSampleSink(sink);
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
    m_deviceSets.back()->m_deviceSinkEngine = nullptr;
    m_deviceSets.back()->m_deviceMIMOEngine = nullptr;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "R%d", deviceTabIndex);

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleRx, deviceTabIndex, dspDeviceSourceEngine, nullptr, nullptr);

    m_deviceSets.back()->m_deviceAPI = deviceAPI;

    // Create a file source instance by default
    int fileSourceDeviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(fileSourceDeviceIndex);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    m_deviceSets.back()->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    m_deviceSets.back()->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    m_deviceSets.back()->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    m_deviceSets.back()->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    m_deviceSets.back()->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(fileSourceDeviceIndex));

    QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        m_deviceSets.back()->m_deviceAPI->setHardwareUserArguments(userArgs);
    }

    DeviceSampleSource *source = m_deviceSets.back()->m_deviceAPI->getPluginInterface()->createSampleSourcePluginInstance(
            m_deviceSets.back()->m_deviceAPI->getSamplingDeviceId(), m_deviceSets.back()->m_deviceAPI);
    m_deviceSets.back()->m_deviceAPI->setSampleSource(source);
}

void MainCore::removeLastDevice()
{
    if (m_deviceSets.back()->m_deviceSourceEngine) // source set
    {
        DSPDeviceSourceEngine *lastDeviceEngine = m_deviceSets.back()->m_deviceSourceEngine;
        lastDeviceEngine->stopAcquistion();

        // deletes old UI and input object
        m_deviceSets.back()->freeChannels();      // destroys the channel instances
        m_deviceSets.back()->m_deviceAPI->resetSamplingDeviceId();
        m_deviceSets.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                m_deviceSets.back()->m_deviceAPI->getSampleSource());
        m_deviceSets.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        DeviceAPI *sourceAPI = m_deviceSets.back()->m_deviceAPI;
        delete m_deviceSets.back();

        lastDeviceEngine->stop();
        m_dspEngine->removeLastDeviceSourceEngine();

        delete sourceAPI;
    }
    else if (m_deviceSets.back()->m_deviceSinkEngine) // sink set
    {
        DSPDeviceSinkEngine *lastDeviceEngine = m_deviceSets.back()->m_deviceSinkEngine;
        lastDeviceEngine->stopGeneration();

        // deletes old UI and output object
        m_deviceSets.back()->freeChannels();
        m_deviceSets.back()->m_deviceAPI->resetSamplingDeviceId();
        m_deviceSets.back()->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                m_deviceSets.back()->m_deviceAPI->getSampleSink());
        m_deviceSets.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        DeviceAPI *sinkAPI = m_deviceSets.back()->m_deviceAPI;
        delete m_deviceSets.back();

        lastDeviceEngine->stop();
        m_dspEngine->removeLastDeviceSinkEngine();

        delete sinkAPI;
    }

    m_deviceSets.pop_back();
}

void MainCore::changeSampleSource(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainCore::changeSampleSource: deviceSet at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceSet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and input object
        deviceSet->m_deviceAPI->resetSamplingDeviceId();
        deviceSet->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                deviceSet->m_deviceAPI->getSampleSource());
        deviceSet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(selectedDeviceIndex);
        deviceSet->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceSet->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceSet->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceSet->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceSet->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceSet->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceSet->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceSet->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(selectedDeviceIndex));

        // add to buddies list
        std::vector<DeviceSet*>::iterator it = m_deviceSets.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceSets.end(); ++it)
        {
            if (*it != deviceSet) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceSet->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceSet->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSourceBuddy(deviceSet->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceSet->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceSet->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSourceBuddy(deviceSet->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceSet->m_deviceAPI->setBuddyLeader(true);
        }

        // constructs new GUI and input object
        DeviceSampleSource *source = deviceSet->m_deviceAPI->getPluginInterface()->createSampleSourcePluginInstance(
                deviceSet->m_deviceAPI->getSamplingDeviceId(), deviceSet->m_deviceAPI);
        deviceSet->m_deviceAPI->setSampleSource(source);

        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainCore::changeSampleSink(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainCore::changeSampleSink: device set at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceSet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceSet->m_deviceAPI->resetSamplingDeviceId();
        deviceSet->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                deviceSet->m_deviceAPI->getSampleSink());
        deviceSet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(selectedDeviceIndex);
        deviceSet->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceSet->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceSet->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceSet->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceSet->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceSet->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceSet->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceSet->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(selectedDeviceIndex));

        // add to buddies list
        std::vector<DeviceSet*>::iterator it = m_deviceSets.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceSets.end(); ++it)
        {
            if (*it != deviceSet) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceSet->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceSet->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSinkBuddy(deviceSet->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceSet->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceSet->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSinkBuddy(deviceSet->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceSet->m_deviceAPI->setBuddyLeader(true);
        }

        // constructs new GUI and output object
        DeviceSampleSink *sink = deviceSet->m_deviceAPI->getPluginInterface()->createSampleSinkPluginInstance(
                deviceSet->m_deviceAPI->getSamplingDeviceId(), deviceSet->m_deviceAPI);
        deviceSet->m_deviceAPI->setSampleSink(sink);

        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainCore::changeSampleMIMO(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainCore::changeSampleMIMO: device set at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceSet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceSet->m_deviceAPI->resetSamplingDeviceId();
        deviceSet->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceMIMO(
                deviceSet->m_deviceAPI->getSampleMIMO());

        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(selectedDeviceIndex);
        deviceSet->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceSet->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceSet->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceSet->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceSet->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceSet->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceSet->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceSet->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getMIMOPluginInterface(selectedDeviceIndex));

        QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

        if (userArgs.size() > 0) {
            deviceSet->m_deviceAPI->setHardwareUserArguments(userArgs);
        }

        // constructs new GUI and MIMO object
        DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getPluginInterface()->createSampleMIMOPluginInstance(
                deviceSet->m_deviceAPI->getSamplingDeviceId(), deviceSet->m_deviceAPI);
        deviceSet->m_deviceAPI->setSampleMIMO(mimo);

        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(m_settings.getWorkingPreset()); // load new API settings
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
        deviceSet->deleteChannel(channelIndex);
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
        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(preset);

        if (deviceSet->m_deviceSourceEngine) { // source device
        	deviceSet->loadRxChannelSettings(preset, m_pluginManager->getPluginAPI());
        } else if (deviceSet->m_deviceSinkEngine) { // sink device
        	deviceSet->loadTxChannelSettings(preset, m_pluginManager->getPluginAPI());
        } else if (deviceSet->m_deviceMIMOEngine) { // MIMO device
        	deviceSet->loadMIMOChannelSettings(preset, m_pluginManager->getPluginAPI());
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
        preset->setSourcePreset();
        deviceSet->saveRxChannelSettings(preset);
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(preset);
    }
    else if (deviceSet->m_deviceSinkEngine) // sink device
    {
        preset->clearChannels();
        preset->setSinkPreset();
        deviceSet->saveTxChannelSettings(preset);
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(preset);
    }
    else if (deviceSet->m_deviceMIMOEngine) // MIMO device
    {
        preset->clearChannels();
        preset->setMIMOPreset();
        deviceSet->saveMIMOChannelSettings(preset);
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(preset);
    }
}

