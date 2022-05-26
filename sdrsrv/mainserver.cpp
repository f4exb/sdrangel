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
#include "dsp/spectrumvis.h"
#include "device/deviceapi.h"
#include "device/deviceset.h"
#include "device/deviceenumerator.h"
#include "feature/featureset.h"
#include "plugin/pluginmanager.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"
#include "webapi/webapiadapter.h"

#include "mainparser.h"
#include "mainserver.h"

MainServer *MainServer::m_instance = 0;

MainServer::MainServer(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent) :
    QObject(parent),
    m_mainCore(MainCore::instance()),
    m_dspEngine(DSPEngine::instance())
{
    qDebug() << "MainServer::MainServer: start";

    m_instance = this;
    m_mainCore->m_logger = logger;
    m_mainCore->m_mainMessageQueue = &m_inputMessageQueue;
    m_mainCore->m_settings.setAudioDeviceManager(m_dspEngine->getAudioDeviceManager());
    m_mainCore->m_masterTabIndex = -1;

    qDebug() << "MainServer::MainServer: create FFT factory...";
    m_dspEngine->createFFTFactory(parser.getFFTWFWisdomFileName());

    qDebug() << "MainServer::MainServer: load plugins...";
    m_mainCore->m_pluginManager = new PluginManager(this);
    m_mainCore->m_pluginManager->setEnableSoapy(parser.getSoapy());
    m_mainCore->m_pluginManager->loadPlugins(QString("pluginssrv"));
    addFeatureSet(); // Create the uniuefeature set

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);
    m_mainCore->m_masterTimer.start(50);

    qDebug() << "MainServer::MainServer: load setings...";
	loadSettings();

    qDebug() << "MainServer::MainServer: finishing...";
    QString applicationDirPath = QCoreApplication::instance()->applicationDirPath();

    m_apiAdapter = new WebAPIAdapter();
    m_requestMapper = new WebAPIRequestMapper(this);
    m_requestMapper->setAdapter(m_apiAdapter);
    m_apiServer = new WebAPIServer(parser.getServerAddress(), parser.getServerPort(), m_requestMapper);
    m_apiServer->start();

    m_dspEngine->setMIMOSupport(true);

    qDebug() << "MainServer::MainServer: end";
}

MainServer::~MainServer()
{
    while (m_mainCore->m_deviceSets.size() > 0) {
        removeLastDevice();
    }

	m_apiServer->stop();
	m_mainCore->m_settings.save();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_mainCore->m_pluginManager;

    qDebug() << "MainServer::~MainServer: end";
}

bool MainServer::handleMessage(const Message& cmd)
{
    if (MainCore::MsgDeleteInstance::match(cmd))
    {
        while (m_mainCore->m_deviceSets.size() > 0)
        {
            removeLastDevice();
        }

        emit finished();
        return true;
    }
    else if (MainCore::MsgLoadPreset::match(cmd))
    {
        MainCore::MsgLoadPreset& notif = (MainCore::MsgLoadPreset&) cmd;
        loadPresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        return true;
    }
    else if (MainCore::MsgSavePreset::match(cmd))
    {
        MainCore::MsgSavePreset& notif = (MainCore::MsgSavePreset&) cmd;
        savePresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        m_mainCore->m_settings.sortPresets();
        m_mainCore->m_settings.save();
        return true;
    }
    else if (MainCore::MsgDeletePreset::match(cmd))
    {
        MainCore::MsgDeletePreset& notif = (MainCore::MsgDeletePreset&) cmd;
        const Preset *presetToDelete = notif.getPreset();
        // remove preset from settings
        m_mainCore->m_settings.deletePreset(presetToDelete);
        return true;
    }
    else if (MainCore::MsgDeleteConfiguration::match(cmd))
    {
        MainCore::MsgDeleteConfiguration& notif = (MainCore::MsgDeleteConfiguration&) cmd;
        const Configuration *configuationToDelete = notif.getConfiguration();
        // remove configuration from settings
        m_mainCore->m_settings.deleteConfiguration(configuationToDelete);
        return true;
    }
    else if (MainCore::MsgLoadFeatureSetPreset::match(cmd))
    {
        MainCore::MsgLoadFeatureSetPreset& notif = (MainCore::MsgLoadFeatureSetPreset&) cmd;
        loadFeatureSetPresetSettings(notif.getPreset(), notif.getFeatureSetIndex());
        return true;
    }
    else if (MainCore::MsgSaveFeatureSetPreset::match(cmd))
    {
        MainCore::MsgSaveFeatureSetPreset& notif = (MainCore::MsgSaveFeatureSetPreset&) cmd;
        saveFeatureSetPresetSettings(notif.getPreset(), notif.getFeatureSetIndex());
        m_mainCore->m_settings.sortPresets();
        m_mainCore->m_settings.save();
        return true;
    }
    else if (MainCore::MsgDeleteFeatureSetPreset::match(cmd))
    {
        MainCore::MsgDeleteFeatureSetPreset& notif = (MainCore::MsgDeleteFeatureSetPreset&) cmd;
        const FeatureSetPreset *presetToDelete = notif.getPreset();
        // remove preset from settings
        m_mainCore->m_settings.deleteFeatureSetPreset(presetToDelete);
        return true;
    }
    else if (MainCore::MsgAddDeviceSet::match(cmd))
    {
        MainCore::MsgAddDeviceSet& notif = (MainCore::MsgAddDeviceSet&) cmd;
        int direction = notif.getDirection();

        if (direction == 1) { // Single stream Tx
            addSinkDevice();
        } else if (direction == 0) { // Single stream Rx
            addSourceDevice();
        } else if (direction == 2) { // MIMO
            addMIMODevice();
        }

        return true;
    }
    else if (MainCore::MsgRemoveLastDeviceSet::match(cmd))
    {
        if (m_mainCore->m_deviceSets.size() > 0) {
            removeLastDevice();
        }

        return true;
    }
    else if (MainCore::MsgSetDevice::match(cmd))
    {
        MainCore::MsgSetDevice& notif = (MainCore::MsgSetDevice&) cmd;

        if (notif.getDeviceType() == 1) {
            changeSampleSink(notif.getDeviceSetIndex(), notif.getDeviceIndex());
        } else if (notif.getDeviceType() == 0) {
            changeSampleSource(notif.getDeviceSetIndex(), notif.getDeviceIndex());
        } else if (notif.getDeviceType() == 2) {
            changeSampleMIMO(notif.getDeviceSetIndex(), notif.getDeviceIndex());
        }
        return true;
    }
    else if (MainCore::MsgAddChannel::match(cmd))
    {
        MainCore::MsgAddChannel& notif = (MainCore::MsgAddChannel&) cmd;
        addChannel(notif.getDeviceSetIndex(), notif.getChannelRegistrationIndex());
        return true;
    }
    else if (MainCore::MsgDeleteChannel::match(cmd))
    {
        MainCore::MsgDeleteChannel& notif = (MainCore::MsgDeleteChannel&) cmd;
        deleteChannel(notif.getDeviceSetIndex(), notif.getChannelIndex());
        return true;
    }
    else if (MainCore::MsgAddFeature::match(cmd))
    {
        MainCore::MsgAddFeature& notif = (MainCore::MsgAddFeature&) cmd;
        addFeature(0, notif.getFeatureRegistrationIndex());

        return true;
    }
    else if (MainCore::MsgDeleteFeature::match(cmd))
    {
        MainCore::MsgDeleteFeature& notif = (MainCore::MsgDeleteFeature&) cmd;
        deleteFeature(0, notif.getFeatureIndex());
        return true;
    }
    else if (MainCore::MsgApplySettings::match(cmd))
    {
        applySettings();
        return true;
    }
    else
    {
        return false;
    }
}

void MainServer::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("MainServer::handleMessages: message: %s", message->getIdentifier());
        handleMessage(*message);
        delete message;
    }
}

void MainServer::loadSettings()
{
	qDebug() << "MainServer::loadSettings";

    m_mainCore->m_settings.load();
    m_mainCore->m_settings.sortPresets();
    m_mainCore->setLoggingOptions();
}

void MainServer::applySettings()
{
    m_mainCore->m_settings.sortPresets();
    m_mainCore->setLoggingOptions();
}

void MainServer::addSinkDevice()
{
    DSPDeviceSinkEngine *dspDeviceSinkEngine = m_dspEngine->addDeviceSinkEngine();
    dspDeviceSinkEngine->start();

    uint dspDeviceSinkEngineUID =  dspDeviceSinkEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSinkEngineUID);

    int deviceTabIndex = m_mainCore->m_deviceSets.size();
    m_mainCore->m_deviceSets.push_back(new DeviceSet(deviceTabIndex, 1));
    m_mainCore->m_deviceSets.back()->m_deviceSourceEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceSinkEngine = dspDeviceSinkEngine;
    m_mainCore->m_deviceSets.back()->m_deviceMIMOEngine = nullptr;
    dspDeviceSinkEngine->addSpectrumSink(m_mainCore->m_deviceSets.back()->m_spectrumVis);

    char tabNameCStr[16];
    sprintf(tabNameCStr, "T%d", deviceTabIndex);

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleTx, deviceTabIndex, nullptr, dspDeviceSinkEngine, nullptr);

    m_mainCore->m_deviceSets.back()->m_deviceAPI = deviceAPI;
    QList<QString> channelNames;

    // create a file sink by default
    int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileOutputDeviceIndex();
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
    deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    deviceAPI->setHardwareId(samplingDevice->hardwareId);
    deviceAPI->setSamplingDeviceId(samplingDevice->id);
    deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(fileSinkDeviceIndex));

    QString userArgs = m_mainCore->m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        deviceAPI->setHardwareUserArguments(userArgs);
    }

    DeviceSampleSink *sink = deviceAPI->getPluginInterface()->createSampleSinkPluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleSink(sink);
    emit m_mainCore->deviceSetAdded(deviceTabIndex, deviceAPI);
}

void MainServer::addSourceDevice()
{
    DSPDeviceSourceEngine *dspDeviceSourceEngine = m_dspEngine->addDeviceSourceEngine();
    dspDeviceSourceEngine->start();

    uint dspDeviceSourceEngineUID =  dspDeviceSourceEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSourceEngineUID);

    int deviceTabIndex = m_mainCore->m_deviceSets.size();
    m_mainCore->m_deviceSets.push_back(new DeviceSet(deviceTabIndex, 0));
    m_mainCore->m_deviceSets.back()->m_deviceSourceEngine = dspDeviceSourceEngine;
    m_mainCore->m_deviceSets.back()->m_deviceSinkEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceMIMOEngine = nullptr;
    dspDeviceSourceEngine->addSink(m_mainCore->m_deviceSets.back()->m_spectrumVis);

    char tabNameCStr[16];
    sprintf(tabNameCStr, "R%d", deviceTabIndex);

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleRx, deviceTabIndex, dspDeviceSourceEngine, nullptr, nullptr);

    m_mainCore->m_deviceSets.back()->m_deviceAPI = deviceAPI;

    // Create a file source instance by default
    int fileSourceDeviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(fileSourceDeviceIndex);
    deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    deviceAPI->setHardwareId(samplingDevice->hardwareId);
    deviceAPI->setSamplingDeviceId(samplingDevice->id);
    deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(fileSourceDeviceIndex));

    QString userArgs = m_mainCore->m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        deviceAPI->setHardwareUserArguments(userArgs);
    }

    DeviceSampleSource *source = deviceAPI->getPluginInterface()->createSampleSourcePluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleSource(source);
    emit m_mainCore->deviceSetAdded(deviceTabIndex, deviceAPI);
}

void MainServer::addMIMODevice()
{
    DSPDeviceMIMOEngine *dspDeviceMIMOEngine = m_dspEngine->addDeviceMIMOEngine();
    dspDeviceMIMOEngine->start();

    uint dspDeviceMIMOEngineUID =  dspDeviceMIMOEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceMIMOEngineUID);

    int deviceTabIndex = m_mainCore->m_deviceSets.size();
    m_mainCore->m_deviceSets.push_back(new DeviceSet(deviceTabIndex, 2));
    m_mainCore->m_deviceSets.back()->m_deviceSourceEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceSinkEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceMIMOEngine = dspDeviceMIMOEngine;
    dspDeviceMIMOEngine->addSpectrumSink(m_mainCore->m_deviceSets.back()->m_spectrumVis);

    char tabNameCStr[16];
    sprintf(tabNameCStr, "M%d", deviceTabIndex);

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamMIMO, deviceTabIndex, nullptr, nullptr, dspDeviceMIMOEngine);

    // create a test MIMO by default
    int testMIMODeviceIndex = DeviceEnumerator::instance()->getTestMIMODeviceIndex();
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(testMIMODeviceIndex);
    deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    deviceAPI->setHardwareId(samplingDevice->hardwareId);
    deviceAPI->setSamplingDeviceId(samplingDevice->id);
    deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getMIMOPluginInterface(testMIMODeviceIndex));

    QString userArgs = m_mainCore->m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        deviceAPI->setHardwareUserArguments(userArgs);
    }

    DeviceSampleMIMO *mimo = deviceAPI->getPluginInterface()->createSampleMIMOPluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    m_mainCore->m_deviceSets.back()->m_deviceAPI->setSampleMIMO(mimo);
    emit m_mainCore->deviceSetAdded(deviceTabIndex, deviceAPI);
}

void MainServer::removeLastDevice()
{
    int removedTabIndex = m_mainCore->m_deviceSets.size() - 1;

    if (m_mainCore->m_deviceSets.back()->m_deviceSourceEngine) // source set
    {
        DSPDeviceSourceEngine *lastDeviceEngine = m_mainCore->m_deviceSets.back()->m_deviceSourceEngine;
        lastDeviceEngine->stopAcquistion();

        // deletes old UI and input object
        m_mainCore->m_deviceSets.back()->freeChannels();      // destroys the channel instances
        m_mainCore->m_deviceSets.back()->m_deviceAPI->resetSamplingDeviceId();
        m_mainCore->m_deviceSets.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                m_mainCore->m_deviceSets.back()->m_deviceAPI->getSampleSource());
        m_mainCore->m_deviceSets.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        DeviceAPI *sourceAPI = m_mainCore->m_deviceSets.back()->m_deviceAPI;
        delete m_mainCore->m_deviceSets.back();

        lastDeviceEngine->stop();
        m_dspEngine->removeLastDeviceSourceEngine();

        delete sourceAPI;
    }
    else if (m_mainCore->m_deviceSets.back()->m_deviceSinkEngine) // sink set
    {
        DSPDeviceSinkEngine *lastDeviceEngine = m_mainCore->m_deviceSets.back()->m_deviceSinkEngine;
        lastDeviceEngine->stopGeneration();

        // deletes old UI and output object
        m_mainCore->m_deviceSets.back()->freeChannels();
        m_mainCore->m_deviceSets.back()->m_deviceAPI->resetSamplingDeviceId();
        m_mainCore->m_deviceSets.back()->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                m_mainCore->m_deviceSets.back()->m_deviceAPI->getSampleSink());
        m_mainCore->m_deviceSets.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        DeviceAPI *sinkAPI = m_mainCore->m_deviceSets.back()->m_deviceAPI;
        delete m_mainCore->m_deviceSets.back();

        lastDeviceEngine->stop();
        m_dspEngine->removeLastDeviceSinkEngine();

        delete sinkAPI;
    }

    m_mainCore->m_deviceSets.pop_back();
    emit m_mainCore->deviceSetRemoved(removedTabIndex);
}

void MainServer::changeSampleSource(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainServer::changeSampleSource: deviceSet at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
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

        if (deviceSet->m_deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
        {
            qDebug("MainServer::changeSampleSource: non existent device replaced by File Input");
            int  deviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
            samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex);
            deviceSet->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
            deviceSet->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
            deviceSet->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
            deviceSet->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
            deviceSet->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
            deviceSet->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
            deviceSet->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
            deviceSet->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(deviceIndex));
        }

        // add to buddies list
        std::vector<DeviceSet*>::iterator it = m_mainCore->m_deviceSets.begin();
        int nbOfBuddies = 0;

        for (; it != m_mainCore->m_deviceSets.end(); ++it)
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

        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // load new API settings

        // Notify
        emit m_mainCore->deviceChanged(deviceSetIndex);
    }
}

void MainServer::changeSampleSink(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainServer::changeSampleSink: device set at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
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

        if (deviceSet->m_deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
        {
            qDebug("MainServer::changeSampleSink: non existent device replaced by File Sink");
            int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileOutputDeviceIndex();
            const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
            deviceSet->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
            deviceSet->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
            deviceSet->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
            deviceSet->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
            deviceSet->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
            deviceSet->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
            deviceSet->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
            deviceSet->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(fileSinkDeviceIndex));
        }

        // add to buddies list
        std::vector<DeviceSet*>::iterator it = m_mainCore->m_deviceSets.begin();
        int nbOfBuddies = 0;

        for (; it != m_mainCore->m_deviceSets.end(); ++it)
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

        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainServer::changeSampleMIMO(int deviceSetIndex, int selectedDeviceIndex)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainServer::changeSampleMIMO: device set at %d", deviceSetIndex);
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        deviceSet->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
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

        if (deviceSet->m_deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
        {
            qDebug("MainServer::changeSampleMIMO: non existent device replaced by Test MIMO");
            int testMIMODeviceIndex = DeviceEnumerator::instance()->getTestMIMODeviceIndex();
            const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(testMIMODeviceIndex);
            deviceSet->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
            deviceSet->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
            deviceSet->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
            deviceSet->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
            deviceSet->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
            deviceSet->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
            deviceSet->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
            deviceSet->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getMIMOPluginInterface(testMIMODeviceIndex));
        }

        QString userArgs = m_mainCore->m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

        if (userArgs.size() > 0) {
            deviceSet->m_deviceAPI->setHardwareUserArguments(userArgs);
        }

        // constructs new GUI and MIMO object
        DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getPluginInterface()->createSampleMIMOPluginInstance(
                deviceSet->m_deviceAPI->getSamplingDeviceId(), deviceSet->m_deviceAPI);
        deviceSet->m_deviceAPI->setSampleMIMO(mimo);

        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainServer::addChannel(int deviceSetIndex, int selectedChannelIndex)
{
    if (deviceSetIndex >= 0)
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // source device => Rx channels
        {
            deviceSet->addRxChannel(selectedChannelIndex, m_mainCore->m_pluginManager->getPluginAPI());
        }
        else if (deviceSet->m_deviceSinkEngine) // sink device => Tx channels
        {
            deviceSet->addTxChannel(selectedChannelIndex, m_mainCore->m_pluginManager->getPluginAPI());
        }
    }
}

void MainServer::deleteChannel(int deviceSetIndex, int channelIndex)
{
    if (deviceSetIndex >= 0)
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        deviceSet->deleteChannel(channelIndex);
    }
}

void MainServer::addFeatureSet()
{
    m_mainCore->appendFeatureSet();
    emit m_mainCore->featureSetAdded(m_mainCore->getFeatureeSets().size() - 1);
}

void MainServer::removeFeatureSet(unsigned int featureSetIndex)
{
    if (featureSetIndex < m_mainCore->m_featureSets.size())
    {
        m_mainCore->removeFeatureSet(featureSetIndex);
        emit m_mainCore->featureSetRemoved(featureSetIndex);
    }
}

void MainServer::addFeature(int featureSetIndex, int selectedFeatureIndex)
{
    if (featureSetIndex >= 0)
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        featureSet->addFeature(selectedFeatureIndex, m_mainCore->m_pluginManager->getPluginAPI(), m_apiAdapter);
    }
}

void MainServer::deleteFeature(int featureSetIndex, int featureIndex)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        featureSet->deleteFeature(featureIndex);
    }
}

void MainServer::loadPresetSettings(const Preset* preset, int tabIndex)
{
	qDebug("MainServer::loadPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	if (tabIndex >= 0)
	{
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[tabIndex];
        deviceSet->m_deviceAPI->loadSamplingDeviceSettings(preset);

        if (deviceSet->m_deviceSourceEngine) { // source device
        	deviceSet->loadRxChannelSettings(preset, m_mainCore->m_pluginManager->getPluginAPI());
        } else if (deviceSet->m_deviceSinkEngine) { // sink device
        	deviceSet->loadTxChannelSettings(preset, m_mainCore->m_pluginManager->getPluginAPI());
        } else if (deviceSet->m_deviceMIMOEngine) { // MIMO device
        	deviceSet->loadMIMOChannelSettings(preset, m_mainCore->m_pluginManager->getPluginAPI());
        }
	}
}

void MainServer::savePresetSettings(Preset* preset, int tabIndex)
{
    qDebug("MainServer::savePresetSettings: preset [%s | %s]",
        qPrintable(preset->getGroup()),
        qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    DeviceSet *deviceSet = m_mainCore->m_deviceSets[tabIndex];

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

void MainServer::loadFeatureSetPresetSettings(const FeatureSetPreset* preset, int featureSetIndex)
{
	qDebug("MainServer::loadFeatureSetPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	if (featureSetIndex >= 0)
	{
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        featureSet->loadFeatureSetSettings(preset, m_mainCore->m_pluginManager->getPluginAPI(), m_apiAdapter);
	}
}

void MainServer::saveFeatureSetPresetSettings(FeatureSetPreset* preset, int featureSetIndex)
{
    qDebug("MainServer::saveFeatureSetPresetSettings: preset [%s | %s]",
        qPrintable(preset->getGroup()),
        qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];

    preset->clearFeatures();
    featureSet->saveFeatureSetSettings(preset);
}
