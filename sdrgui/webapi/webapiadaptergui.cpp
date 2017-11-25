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

#include <QApplication>
#include <QList>

#include "mainwindow.h"
#include "loggerwithfile.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "device/deviceenumerator.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/dspengine.h"
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "channel/channelsinkapi.h"
#include "channel/channelsourceapi.h"

#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGDeviceListItem.h"
#include "SWGAudioDevices.h"
#include "SWGAudioDevicesSelect.h"
#include "SWGLocationInformation.h"
#include "SWGDVSeralDevices.h"
#include "SWGDVSerialDevice.h"
#include "SWGPresets.h"
#include "SWGPresetGroup.h"
#include "SWGPresetItem.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGErrorResponse.h"

#include "webapiadaptergui.h"

WebAPIAdapterGUI::WebAPIAdapterGUI(MainWindow& mainWindow) :
    m_mainWindow(mainWindow)
{
}

WebAPIAdapterGUI::~WebAPIAdapterGUI()
{
}

int WebAPIAdapterGUI::instanceSummary(
            Swagger::SWGInstanceSummaryResponse& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{

    *response.getVersion() = qApp->applicationVersion();

    Swagger::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_mainWindow.m_logger->getUseFileLogger() ? 1 : 0);
    if (logging->getDumpToFile()) {
        m_mainWindow.m_logger->getLogFileName(*logging->getFileName());
        m_mainWindow.m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }
    m_mainWindow.m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    Swagger::SWGDeviceSetList *deviceSetList = response.getDevicesetlist();
    deviceSetList->init();
    deviceSetList->setDevicesetcount((int) m_mainWindow.m_deviceUIs.size());

    std::vector<DeviceUISet*>::const_iterator it = m_mainWindow.m_deviceUIs.begin();

    for (int i = 0; it != m_mainWindow.m_deviceUIs.end(); ++it, i++)
    {
        QList<Swagger::SWGDeviceSet*> *deviceSet = deviceSetList->getDeviceSets();
        deviceSet->append(new Swagger::SWGDeviceSet());
        Swagger::SWGSamplingDevice *samplingDevice = deviceSet->back()->getSamplingDevice();
        samplingDevice->init();
        samplingDevice->setIndex(i);
        samplingDevice->setTx((*it)->m_deviceSinkEngine != 0);

        if ((*it)->m_deviceSinkEngine) // Tx data
        {
            *samplingDevice->getHwType() = (*it)->m_deviceSinkAPI->getHardwareId();
            *samplingDevice->getSerial() = (*it)->m_deviceSinkAPI->getSampleSinkSerial();
            samplingDevice->setSequence((*it)->m_deviceSinkAPI->getSampleSinkSequence());
            samplingDevice->setNbStreams((*it)->m_deviceSinkAPI->getNbItems());
            samplingDevice->setStreamIndex((*it)->m_deviceSinkAPI->getItemIndex());
            (*it)->m_deviceSinkAPI->getDeviceEngineStateStr(*samplingDevice->getState());
            DeviceSampleSink *sampleSink = (*it)->m_deviceSinkEngine->getSink();

            if (sampleSink) {
                samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
                samplingDevice->setBandwidth(sampleSink->getSampleRate());
            }

            deviceSet->back()->setChannelcount((*it)->m_deviceSinkAPI->getNbChannels());
            QList<Swagger::SWGChannel*> *channels = deviceSet->back()->getChannels();

            for (int i = 0; i <  deviceSet->back()->getChannelcount(); i++)
            {
                channels->append(new Swagger::SWGChannel);
                ChannelSourceAPI *channel = (*it)->m_deviceSinkAPI->getChanelAPIAt(i);
                channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
                channels->back()->setIndex(channel->getIndexInDeviceSet());
                channels->back()->setUid(channel->getUID());
                channel->getIdentifier(*channels->back()->getId());
                channel->getTitle(*channels->back()->getTitle());
            }
        }

        if ((*it)->m_deviceSourceEngine) // Rx data
        {
            *samplingDevice->getHwType() = (*it)->m_deviceSourceAPI->getHardwareId();
            *samplingDevice->getSerial() = (*it)->m_deviceSourceAPI->getSampleSourceSerial();
            samplingDevice->setSequence((*it)->m_deviceSourceAPI->getSampleSourceSequence());
            samplingDevice->setNbStreams((*it)->m_deviceSourceAPI->getNbItems());
            samplingDevice->setStreamIndex((*it)->m_deviceSourceAPI->getItemIndex());
            (*it)->m_deviceSourceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
            DeviceSampleSource *sampleSource = (*it)->m_deviceSourceEngine->getSource();

            if (sampleSource) {
                samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
                samplingDevice->setBandwidth(sampleSource->getSampleRate());
            }

            deviceSet->back()->setChannelcount((*it)->m_deviceSourceAPI->getNbChannels());
            QList<Swagger::SWGChannel*> *channels = deviceSet->back()->getChannels();

            for (int i = 0; i <  deviceSet->back()->getChannelcount(); i++)
            {
                channels->append(new Swagger::SWGChannel);
                ChannelSinkAPI *channel = (*it)->m_deviceSourceAPI->getChanelAPIAt(i);
                channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
                channels->back()->setIndex(channel->getIndexInDeviceSet());
                channels->back()->setUid(channel->getUID());
                channel->getIdentifier(*channels->back()->getId());
                channel->getTitle(*channels->back()->getTitle());
            }
        }
    }

    return 200;
}

int WebAPIAdapterGUI::instanceDevices(
            bool tx,
            Swagger::SWGInstanceDevicesResponse& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    int nbSamplingDevices = tx ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();
    response.setDevicecount(nbSamplingDevices);
    QList<Swagger::SWGDeviceListItem*> *devices = response.getDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        PluginInterface::SamplingDevice samplingDevice = tx ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);
        devices->append(new Swagger::SWGDeviceListItem);
        *devices->back()->getDisplayedName() = samplingDevice.displayedName;
        *devices->back()->getHwType() = samplingDevice.hardwareId;
        *devices->back()->getSerial() = samplingDevice.serial;
        devices->back()->setSequence(samplingDevice.sequence);
        devices->back()->setTx(!samplingDevice.rxElseTx);
        devices->back()->setNbStreams(samplingDevice.deviceNbItems);
        devices->back()->setDeviceSetIndex(samplingDevice.claimed);
        devices->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterGUI::instanceChannels(
            bool tx,
            Swagger::SWGInstanceChannelsResponse& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    PluginAPI::ChannelRegistrations *channelRegistrations = tx ? m_mainWindow.m_pluginManager->getTxChannelRegistrations() : m_mainWindow.m_pluginManager->getRxChannelRegistrations();
    int nbChannelDevices = channelRegistrations->size();
    response.setChannelcount(nbChannelDevices);
    QList<Swagger::SWGChannelListItem*> *channels = response.getChannels();

    for (int i = 0; i < nbChannelDevices; i++)
    {
        channels->append(new Swagger::SWGChannelListItem);
        PluginInterface *channelInterface = channelRegistrations->at(i).m_plugin;
        const PluginDescriptor& pluginDescriptor = channelInterface->getPluginDescriptor();
        *channels->back()->getVersion() = pluginDescriptor.version;
        *channels->back()->getName() = pluginDescriptor.displayedName;
        channels->back()->setTx(tx);
        *channels->back()->getIdUri() = channelRegistrations->at(i).m_channelIdURI;
        *channels->back()->getId() = channelRegistrations->at(i).m_channelId;
        channels->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterGUI::instanceLoggingGet(
            Swagger::SWGLoggingInfo& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    response.setDumpToFile(m_mainWindow.m_logger->getUseFileLogger() ? 1 : 0);

    if (response.getDumpToFile()) {
        m_mainWindow.m_logger->getLogFileName(*response.getFileName());
        m_mainWindow.m_logger->getFileMinMessageLevelStr(*response.getFileLevel());
    }

    m_mainWindow.m_logger->getConsoleMinMessageLevelStr(*response.getConsoleLevel());

    return 200;
}

int WebAPIAdapterGUI::instanceLoggingPut(
            Swagger::SWGLoggingInfo& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    bool dumpToFile = (response.getDumpToFile() != 0);
    QString* consoleLevel = response.getConsoleLevel();
    QString* fileLevel = response.getFileLevel();
    QString* fileName = response.getFileName();

    // perform actions
    if (consoleLevel) {
        m_mainWindow.m_settings.setConsoleMinLogLevel(getMsgTypeFromString(*consoleLevel));
    }

    if (fileLevel) {
        m_mainWindow.m_settings.setFileMinLogLevel(getMsgTypeFromString(*fileLevel));
    }

    m_mainWindow.m_settings.setUseLogFile(dumpToFile);

    if (fileName) {
        m_mainWindow.m_settings.setLogFileName(*fileName);
    }

    m_mainWindow.setLoggingOpions();

    // build response
    response.init();
    getMsgTypeString(m_mainWindow.m_settings.getConsoleMinLogLevel(), *response.getConsoleLevel());
    response.setDumpToFile(m_mainWindow.m_settings.getUseLogFile() ? 1 : 0);
    getMsgTypeString(m_mainWindow.m_settings.getFileMinLogLevel(), *response.getFileLevel());
    *response.getFileName() = m_mainWindow.m_settings.getLogFileName();

    return 200;
}

int WebAPIAdapterGUI::instanceAudioGet(
            Swagger::SWGAudioDevices& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainWindow.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainWindow.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    response.init();
    response.setNbInputDevices(nbInputDevices);
    response.setInputDeviceSelectedIndex(m_mainWindow.m_audioDeviceInfo.getInputDeviceIndex());
    response.setNbOutputDevices(nbOutputDevices);
    response.setOutputDeviceSelectedIndex(m_mainWindow.m_audioDeviceInfo.getOutputDeviceIndex());
    response.setInputVolume(m_mainWindow.m_audioDeviceInfo.getInputVolume());
    QList<Swagger::SWGAudioDevice*> *inputDevices = response.getInputDevices();
    QList<Swagger::SWGAudioDevice*> *outputDevices = response.getOutputDevices();

    for (int i = 0; i < nbInputDevices; i++)
    {
        inputDevices->append(new Swagger::SWGAudioDevice);
        *inputDevices->back()->getName() = audioInputDevices.at(i).deviceName();
    }

    for (int i = 0; i < nbOutputDevices; i++)
    {
        outputDevices->append(new Swagger::SWGAudioDevice);
        *outputDevices->back()->getName() = audioOutputDevices.at(i).deviceName();
    }

    return 200;
}

int WebAPIAdapterGUI::instanceAudioPatch(
            Swagger::SWGAudioDevicesSelect& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    float inputVolume = response.getInputVolume();
    int inputIndex = response.getInputIndex();
    int outputIndex = response.getOutputIndex();

    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainWindow.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainWindow.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    inputVolume = inputVolume < 0.0 ? 0.0 : inputVolume > 1.0 ? 1.0 : inputVolume;
    inputIndex = inputIndex < -1 ? -1 : inputIndex > nbInputDevices ? nbInputDevices-1 : inputIndex;
    outputIndex = outputIndex < -1 ? -1 : outputIndex > nbOutputDevices ? nbOutputDevices-1 : outputIndex;

    m_mainWindow.m_audioDeviceInfo.setInputVolume(inputVolume);
    m_mainWindow.m_audioDeviceInfo.setInputDeviceIndex(inputIndex);
    m_mainWindow.m_audioDeviceInfo.setOutputDeviceIndex(outputIndex);

    m_mainWindow.m_dspEngine->setAudioInputVolume(inputVolume);
    m_mainWindow.m_dspEngine->setAudioInputDeviceIndex(inputIndex);
    m_mainWindow.m_dspEngine->setAudioOutputDeviceIndex(outputIndex);

    response.setInputVolume(m_mainWindow.m_audioDeviceInfo.getInputVolume());
    response.setInputIndex(m_mainWindow.m_audioDeviceInfo.getInputDeviceIndex());
    response.setOutputIndex(m_mainWindow.m_audioDeviceInfo.getOutputDeviceIndex());

    return 200;
}

int WebAPIAdapterGUI::instanceLocationGet(
            Swagger::SWGLocationInformation& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    response.setLatitude(m_mainWindow.m_settings.getLatitude());
    response.setLongitude(m_mainWindow.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterGUI::instanceLocationPut(
            Swagger::SWGLocationInformation& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    float latitude = response.getLatitude();
    float longitude = response.getLongitude();

    latitude = latitude < -90.0 ? -90.0 : latitude > 90.0 ? 90.0 : latitude;
    longitude = longitude < -180.0 ? -180.0 : longitude > 180.0 ? 180.0 : longitude;

    m_mainWindow.m_settings.setLatitude(latitude);
    m_mainWindow.m_settings.setLongitude(longitude);

    response.setLatitude(m_mainWindow.m_settings.getLatitude());
    response.setLongitude(m_mainWindow.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterGUI::instanceDVSerialPatch(
            bool dvserial,
            Swagger::SWGDVSeralDevices& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    m_mainWindow.m_dspEngine->setDVSerialSupport(dvserial);
    response.init();

    if (dvserial)
    {
        std::vector<std::string> deviceNames;
        m_mainWindow.m_dspEngine->getDVSerialNames(deviceNames);
        response.setNbDevices((int) deviceNames.size());
        QList<Swagger::SWGDVSerialDevice*> *deviceNamesList = response.getDvSerialDevices();

        std::vector<std::string>::iterator it = deviceNames.begin();
        std::string deviceNamesStr = "DV Serial devices found: ";

        while (it != deviceNames.end())
        {
            deviceNamesList->append(new Swagger::SWGDVSerialDevice);
            *deviceNamesList->back()->getDeviceName() = QString::fromStdString(*it);
            ++it;
        }
    }
    else
    {
        response.setNbDevices(0);
    }

    return 200;
}

int WebAPIAdapterGUI::instancePresetGet(
            Swagger::SWGPresets& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    int nbPresets = m_mainWindow.m_settings.getPresetCount();
    int nbGroups = 0;
    int nbPresetsThisGroup = 0;
    QString groupName;
    response.init();
    QList<Swagger::SWGPresetGroup*> *groups = response.getGroups();
    QList<Swagger::SWGPresetItem*> *swgPresets = 0;
    int i = 0;

    // Presets are sorted by group first

    for (; i < nbPresets; i++)
    {
        const Preset *preset = m_mainWindow.m_settings.getPreset(i);

        if ((i == 0) || (groupName != preset->getGroup())) // new group
        {
            if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
            groups->append(new Swagger::SWGPresetGroup);
            groups->back()->init();
            groupName = preset->getGroup();
            *groups->back()->getGroupName() = groupName;
            swgPresets = groups->back()->getPresets();
            nbGroups++;
            nbPresetsThisGroup = 0;
        }

        swgPresets->append(new Swagger::SWGPresetItem);
        swgPresets->back()->setCenterFrequency(preset->getCenterFrequency());
        *swgPresets->back()->getType() = preset->isSourcePreset() ? "R" : "T";
        *swgPresets->back()->getName() = preset->getDescription();
        nbPresetsThisGroup++;
    }

    if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
    response.setNbGroups(nbGroups);

    return 200;
}

int WebAPIAdapterGUI::instancePresetPatch(
        Swagger::SWGPresetTransfer& query,
        Swagger::SWGPresetIdentifier& response,
        Swagger::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    Swagger::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();

    if (deviceSetIndex > nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0)
    {
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName());
        return 404;
    }

    DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];

    if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
    {
        *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
        return 404;
    }

    if (deviceUI->m_deviceSinkEngine && selectedPreset->isSourcePreset())
    {
        *error.getMessage() = QString("Preset type (R) and device set type (Tx) mismatch");
        return 404;
    }

    MainWindow::MsgLoadPreset *msg = MainWindow::MsgLoadPreset::create(selectedPreset, deviceSetIndex);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

int WebAPIAdapterGUI::instancePresetPut(
        Swagger::SWGPresetTransfer& query,
        Swagger::SWGPresetIdentifier& response,
        Swagger::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    Swagger::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();
    bool newPreset;

    if (deviceSetIndex > nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0) // save on a new preset
    {
        selectedPreset = m_mainWindow.m_settings.newPreset(*presetIdentifier->getGroupName(), *presetIdentifier->getName());
        newPreset = true;
    }
    else // update existing preset
    {
        DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];
        newPreset = false;

        if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
        {
            *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
            return 404;
        }

        if (deviceUI->m_deviceSinkEngine && selectedPreset->isSourcePreset())
        {
            *error.getMessage() = QString("Preset type (R) and device set type (Tx) mismatch");
            return 404;
        }
    }

    MainWindow::MsgSavePreset *msg = MainWindow::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, newPreset);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

QtMsgType WebAPIAdapterGUI::getMsgTypeFromString(const QString& msgTypeString)
{
    if (msgTypeString == "debug") {
        return QtDebugMsg;
    } else if (msgTypeString == "info") {
        return QtInfoMsg;
    } else if (msgTypeString == "warning") {
        return QtWarningMsg;
    } else if (msgTypeString == "error") {
        return QtCriticalMsg;
    } else {
        return QtDebugMsg;
    }
}

void WebAPIAdapterGUI::getMsgTypeString(const QtMsgType& msgType, QString& levelStr)
{
    switch (msgType)
    {
    case QtDebugMsg:
        levelStr = "debug";
        break;
    case QtInfoMsg:
        levelStr = "info";
        break;
    case QtWarningMsg:
        levelStr = "warning";
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        levelStr = "error";
        break;
    default:
        levelStr = "debug";
        break;
    }
}
