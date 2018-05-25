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
#include <QSysInfo>

#include <unistd.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
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
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGChannelsDetail.h"
#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGDeviceState.h"

#include "webapiadaptergui.h"

WebAPIAdapterGUI::WebAPIAdapterGUI(MainWindow& mainWindow) :
    m_mainWindow(mainWindow)
{
}

WebAPIAdapterGUI::~WebAPIAdapterGUI()
{
}

int WebAPIAdapterGUI::instanceSummary(
        SWGSDRangel::SWGInstanceSummaryResponse& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.init();
    *response.getAppname() = qApp->applicationName();
    *response.getVersion() = qApp->applicationVersion();
    *response.getQtVersion() = QString(QT_VERSION_STR);
    response.setDspRxBits(SDR_RX_SAMP_SZ);
    response.setDspTxBits(SDR_TX_SAMP_SZ);
    response.setPid(qApp->applicationPid());
#if QT_VERSION >= 0x050400
    *response.getArchitecture() = QString(QSysInfo::currentCpuArchitecture());
    *response.getOs() = QString(QSysInfo::prettyProductName());
#endif

    SWGSDRangel::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_mainWindow.m_logger->getUseFileLogger() ? 1 : 0);

    if (logging->getDumpToFile()) {
        m_mainWindow.m_logger->getLogFileName(*logging->getFileName());
        m_mainWindow.m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }

    m_mainWindow.m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    SWGSDRangel::SWGDeviceSetList *deviceSetList = response.getDevicesetlist();
    getDeviceSetList(deviceSetList);

    return 200;
}

int WebAPIAdapterGUI::instanceDelete(
        SWGSDRangel::SWGSuccessResponse& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    *error.getMessage() = QString("Not supported in GUI instance");
    return 400;
}

int WebAPIAdapterGUI::instanceDevices(
            bool tx,
            SWGSDRangel::SWGInstanceDevicesResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.init();
    int nbSamplingDevices = tx ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();
    response.setDevicecount(nbSamplingDevices);
    QList<SWGSDRangel::SWGDeviceListItem*> *devices = response.getDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        PluginInterface::SamplingDevice samplingDevice = tx ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);
        devices->append(new SWGSDRangel::SWGDeviceListItem);
        devices->back()->init();
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
            SWGSDRangel::SWGInstanceChannelsResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.init();
    PluginAPI::ChannelRegistrations *channelRegistrations = tx ? m_mainWindow.m_pluginManager->getTxChannelRegistrations() : m_mainWindow.m_pluginManager->getRxChannelRegistrations();
    int nbChannelDevices = channelRegistrations->size();
    response.setChannelcount(nbChannelDevices);
    QList<SWGSDRangel::SWGChannelListItem*> *channels = response.getChannels();

    for (int i = 0; i < nbChannelDevices; i++)
    {
        channels->append(new SWGSDRangel::SWGChannelListItem);
        channels->back()->init();
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
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.init();
    response.setDumpToFile(m_mainWindow.m_logger->getUseFileLogger() ? 1 : 0);

    if (response.getDumpToFile()) {
        m_mainWindow.m_logger->getLogFileName(*response.getFileName());
        m_mainWindow.m_logger->getFileMinMessageLevelStr(*response.getFileLevel());
    }

    m_mainWindow.m_logger->getConsoleMinMessageLevelStr(*response.getConsoleLevel());

    return 200;
}

int WebAPIAdapterGUI::instanceLoggingPut(
        SWGSDRangel::SWGLoggingInfo& query,
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    bool dumpToFile = (query.getDumpToFile() != 0);
    QString* consoleLevel = query.getConsoleLevel();
    QString* fileLevel = query.getFileLevel();
    QString* fileName = query.getFileName();

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

    m_mainWindow.setLoggingOptions();

    // build response
    response.init();
    getMsgTypeString(m_mainWindow.m_settings.getConsoleMinLogLevel(), *response.getConsoleLevel());
    response.setDumpToFile(m_mainWindow.m_settings.getUseLogFile() ? 1 : 0);
    getMsgTypeString(m_mainWindow.m_settings.getFileMinLogLevel(), *response.getFileLevel());
    *response.getFileName() = m_mainWindow.m_settings.getLogFileName();

    return 200;
}

int WebAPIAdapterGUI::instanceAudioGet(
        SWGSDRangel::SWGAudioDevices& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    response.init();
    response.setNbInputDevices(nbInputDevices);
    response.setNbOutputDevices(nbOutputDevices);
    QList<SWGSDRangel::SWGAudioInputDevice*> *inputDevices = response.getInputDevices();
    QList<SWGSDRangel::SWGAudioOutputDevice*> *outputDevices = response.getOutputDevices();
    AudioDeviceManager::InputDeviceInfo inputDeviceInfo;
    AudioDeviceManager::OutputDeviceInfo outputDeviceInfo;

    // system default input device
    inputDevices->append(new SWGSDRangel::SWGAudioInputDevice);
    inputDevices->back()->init();
    bool found = m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDeviceInfo(AudioDeviceManager::m_defaultDeviceName, inputDeviceInfo);
    *inputDevices->back()->getName() = AudioDeviceManager::m_defaultDeviceName;
    inputDevices->back()->setIndex(-1);
    inputDevices->back()->setSampleRate(inputDeviceInfo.sampleRate);
    inputDevices->back()->setIsSystemDefault(0);
    inputDevices->back()->setDefaultUnregistered(found ? 0 : 1);
    inputDevices->back()->setVolume(inputDeviceInfo.volume);

    // real input devices
    for (int i = 0; i < nbInputDevices; i++)
    {
        inputDevices->append(new SWGSDRangel::SWGAudioInputDevice);
        inputDevices->back()->init();
        inputDeviceInfo.resetToDefaults();
        found = m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDeviceInfo(audioInputDevices.at(i).deviceName(), inputDeviceInfo);
        *inputDevices->back()->getName() = audioInputDevices.at(i).deviceName();
        inputDevices->back()->setIndex(i);
        inputDevices->back()->setSampleRate(inputDeviceInfo.sampleRate);
        inputDevices->back()->setIsSystemDefault(audioInputDevices.at(i).deviceName() == QAudioDeviceInfo::defaultInputDevice().deviceName() ? 1 : 0);
        inputDevices->back()->setDefaultUnregistered(found ? 0 : 1);
        inputDevices->back()->setVolume(inputDeviceInfo.volume);
    }

    // system default output device
    outputDevices->append(new SWGSDRangel::SWGAudioOutputDevice);
    outputDevices->back()->init();
    found = m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(AudioDeviceManager::m_defaultDeviceName, outputDeviceInfo);
    *outputDevices->back()->getName() = AudioDeviceManager::m_defaultDeviceName;
    outputDevices->back()->setIndex(-1);
    outputDevices->back()->setSampleRate(outputDeviceInfo.sampleRate);
    inputDevices->back()->setIsSystemDefault(0);
    outputDevices->back()->setDefaultUnregistered(found ? 0 : 1);
    outputDevices->back()->setCopyToUdp(outputDeviceInfo.copyToUDP ? 1 : 0);
    outputDevices->back()->setUdpUsesRtp(outputDeviceInfo.udpUseRTP ? 1 : 0);
    outputDevices->back()->setUdpChannelMode((int) outputDeviceInfo.udpChannelMode);
    *outputDevices->back()->getUdpAddress() = outputDeviceInfo.udpAddress;
    outputDevices->back()->setUdpPort(outputDeviceInfo.udpPort);

    // real output devices
    for (int i = 0; i < nbOutputDevices; i++)
    {
        outputDevices->append(new SWGSDRangel::SWGAudioOutputDevice);
        outputDevices->back()->init();
        outputDeviceInfo.resetToDefaults();
        found = m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(audioOutputDevices.at(i).deviceName(), outputDeviceInfo);
        *outputDevices->back()->getName() = audioOutputDevices.at(i).deviceName();
        outputDevices->back()->setIndex(i);
        outputDevices->back()->setSampleRate(outputDeviceInfo.sampleRate);
        outputDevices->back()->setIsSystemDefault(audioOutputDevices.at(i).deviceName() == QAudioDeviceInfo::defaultOutputDevice().deviceName() ? 1 : 0);
        outputDevices->back()->setDefaultUnregistered(found ? 0 : 1);
        outputDevices->back()->setCopyToUdp(outputDeviceInfo.copyToUDP ? 1 : 0);
        outputDevices->back()->setUdpUsesRtp(outputDeviceInfo.udpUseRTP ? 1 : 0);
        outputDevices->back()->setUdpChannelMode((int) outputDeviceInfo.udpChannelMode);
        *outputDevices->back()->getUdpAddress() = outputDeviceInfo.udpAddress;
        outputDevices->back()->setUdpPort(outputDeviceInfo.udpPort);
    }

    return 200;
}

int WebAPIAdapterGUI::instanceAudioInputPatch(
        SWGSDRangel::SWGAudioInputDevice& response,
        const QStringList& audioInputKeys,
        SWGSDRangel::SWGErrorResponse& error)
{
    // TODO
    AudioDeviceManager::InputDeviceInfo inputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio input device at index %1").arg(deviceIndex);
        return 404;
    }

    m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDeviceInfo(deviceName, inputDeviceInfo);

    if (audioInputKeys.contains("sampleRate")) {
        inputDeviceInfo.sampleRate = response.getSampleRate();
    }
    if (audioInputKeys.contains("volume")) {
        inputDeviceInfo.volume = response.getVolume();
    }

    m_mainWindow.m_dspEngine->getAudioDeviceManager()->setInputDeviceInfo(deviceIndex, inputDeviceInfo);
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDeviceInfo(deviceName, inputDeviceInfo);

    response.setSampleRate(inputDeviceInfo.sampleRate);
    response.setVolume(inputDeviceInfo.volume);

    return 200;
}

int WebAPIAdapterGUI::instanceAudioOutputPatch(
        SWGSDRangel::SWGAudioOutputDevice& response,
        const QStringList& audioOutputKeys,
        SWGSDRangel::SWGErrorResponse& error)
{
    AudioDeviceManager::OutputDeviceInfo outputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio output device at index %1").arg(deviceIndex);
        return 404;
    }

    m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(deviceName, outputDeviceInfo);

    if (audioOutputKeys.contains("sampleRate")) {
        outputDeviceInfo.sampleRate = response.getSampleRate();
    }
    if (audioOutputKeys.contains("copyToUDP")) {
        outputDeviceInfo.copyToUDP = response.getCopyToUdp() == 0 ? 0 : 1;
    }
    if (audioOutputKeys.contains("udpUsesRTP")) {
        outputDeviceInfo.udpUseRTP = response.getUdpUsesRtp() == 0 ? 0 : 1;
    }
    if (audioOutputKeys.contains("udpChannelMode")) {
        outputDeviceInfo.udpChannelMode = static_cast<AudioOutput::UDPChannelMode>(response.getUdpChannelMode() % 4);
    }
    if (audioOutputKeys.contains("udpAddress")) {
        outputDeviceInfo.udpAddress = *response.getUdpAddress();
    }
    if (audioOutputKeys.contains("udpPort")) {
        outputDeviceInfo.udpPort = response.getUdpPort() % (1<<16);
    }

    m_mainWindow.m_dspEngine->getAudioDeviceManager()->setOutputDeviceInfo(deviceIndex, outputDeviceInfo);
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(deviceName, outputDeviceInfo);

    response.setSampleRate(outputDeviceInfo.sampleRate);
    response.setCopyToUdp(outputDeviceInfo.copyToUDP == 0 ? 0 : 1);
    response.setUdpUsesRtp(outputDeviceInfo.udpUseRTP == 0 ? 0 : 1);
    response.setUdpChannelMode(outputDeviceInfo.udpChannelMode % 4);

    if (response.getUdpAddress()) {
        *response.getUdpAddress() = outputDeviceInfo.udpAddress;
    } else {
        response.setUdpAddress(new QString(outputDeviceInfo.udpAddress));
    }

    response.setUdpPort(outputDeviceInfo.udpPort % (1<<16));

    return 200;
}

int WebAPIAdapterGUI::instanceAudioInputDelete(
            SWGSDRangel::SWGAudioInputDevice& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    AudioDeviceManager::InputDeviceInfo inputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio input device at index %1").arg(deviceIndex);
        return 404;
    }

    m_mainWindow.m_dspEngine->getAudioDeviceManager()->unsetInputDeviceInfo(deviceIndex);
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->getInputDeviceInfo(deviceName, inputDeviceInfo);

    response.setSampleRate(inputDeviceInfo.sampleRate);
    response.setVolume(inputDeviceInfo.volume);

    return 200;
}

int WebAPIAdapterGUI::instanceAudioOutputDelete(
            SWGSDRangel::SWGAudioOutputDevice& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    AudioDeviceManager::OutputDeviceInfo outputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio output device at index %1").arg(deviceIndex);
        return 404;
    }

    m_mainWindow.m_dspEngine->getAudioDeviceManager()->unsetInputDeviceInfo(deviceIndex);
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(deviceName, outputDeviceInfo);

    response.setSampleRate(outputDeviceInfo.sampleRate);
    response.setCopyToUdp(outputDeviceInfo.copyToUDP == 0 ? 0 : 1);
    response.setUdpUsesRtp(outputDeviceInfo.udpUseRTP == 0 ? 0 : 1);
    response.setUdpChannelMode(outputDeviceInfo.udpChannelMode % 4);

    if (response.getUdpAddress()) {
        *response.getUdpAddress() = outputDeviceInfo.udpAddress;
    } else {
        response.setUdpAddress(new QString(outputDeviceInfo.udpAddress));
    }

    response.setUdpPort(outputDeviceInfo.udpPort % (1<<16));

    return 200;
}

int WebAPIAdapterGUI::instanceAudioInputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->inputInfosCleanup();

    response.init();
    *response.getMessage() = QString("Unregistered parameters for devices not in list of available input devices for this instance");

    return 200;
}

int WebAPIAdapterGUI::instanceAudioOutputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->outputInfosCleanup();

    response.init();
    *response.getMessage() = QString("Unregistered parameters for devices not in list of available output devices for this instance");

    return 200;
}

int WebAPIAdapterGUI::instanceLocationGet(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.init();
    response.setLatitude(m_mainWindow.m_settings.getLatitude());
    response.setLongitude(m_mainWindow.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterGUI::instanceLocationPut(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
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
            SWGSDRangel::SWGDVSeralDevices& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    m_mainWindow.m_dspEngine->setDVSerialSupport(dvserial);
    m_mainWindow.ui->action_DV_Serial->setChecked(dvserial);
    response.init();

    if (dvserial)
    {
        std::vector<std::string> deviceNames;
        m_mainWindow.m_dspEngine->getDVSerialNames(deviceNames);
        response.setNbDevices((int) deviceNames.size());
        QList<SWGSDRangel::SWGDVSerialDevice*> *deviceNamesList = response.getDvSerialDevices();

        std::vector<std::string>::iterator it = deviceNames.begin();
        std::string deviceNamesStr = "DV Serial devices found: ";

        while (it != deviceNames.end())
        {
            deviceNamesList->append(new SWGSDRangel::SWGDVSerialDevice);
            deviceNamesList->back()->init();
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

int WebAPIAdapterGUI::instancePresetsGet(
        SWGSDRangel::SWGPresets& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    int nbPresets = m_mainWindow.m_settings.getPresetCount();
    int nbGroups = 0;
    int nbPresetsThisGroup = 0;
    QString groupName;
    response.init();
    QList<SWGSDRangel::SWGPresetGroup*> *groups = response.getGroups();
    QList<SWGSDRangel::SWGPresetItem*> *swgPresets = 0;
    int i = 0;

    // Presets are sorted by group first

    for (; i < nbPresets; i++)
    {
        const Preset *preset = m_mainWindow.m_settings.getPreset(i);

        if ((i == 0) || (groupName != preset->getGroup())) // new group
        {
            if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
            groups->append(new SWGSDRangel::SWGPresetGroup);
            groups->back()->init();
            groupName = preset->getGroup();
            *groups->back()->getGroupName() = groupName;
            swgPresets = groups->back()->getPresets();
            nbGroups++;
            nbPresetsThisGroup = 0;
        }

        swgPresets->append(new SWGSDRangel::SWGPresetItem);
        swgPresets->back()->init();
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
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        error.init();
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0)
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName());
        return 404;
    }

    DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];

    if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
    {
        error.init();
        *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
        return 404;
    }

    if (deviceUI->m_deviceSinkEngine && selectedPreset->isSourcePreset())
    {
        error.init();
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

    return 202;
}

int WebAPIAdapterGUI::instancePresetPut(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        error.init();
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0)
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName());
        return 404;
    }
    else // update existing preset
    {
        DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
        {
            error.init();
            *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
            return 404;
        }

        if (deviceUI->m_deviceSinkEngine && selectedPreset->isSourcePreset())
        {
            error.init();
            *error.getMessage() = QString("Preset type (R) and device set type (Tx) mismatch");
            return 404;
        }
    }

    MainWindow::MsgSavePreset *msg = MainWindow::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, false);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 202;
}

int WebAPIAdapterGUI::instancePresetPost(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        error.init();
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];
    int deviceCenterFrequency = 0;

    if (deviceSet->m_deviceSourceEngine) { // Rx
        deviceCenterFrequency = deviceSet->m_deviceSourceEngine->getSource()->getCenterFrequency();
    } else if (deviceSet->m_deviceSinkEngine) { // Tx
        deviceCenterFrequency = deviceSet->m_deviceSinkEngine->getSink()->getCenterFrequency();
    } else {
        error.init();
        *error.getMessage() = QString("Device set error");
        return 500;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
            deviceCenterFrequency,
            *presetIdentifier->getName());

    if (selectedPreset == 0) // save on a new preset
    {
        selectedPreset = m_mainWindow.m_settings.newPreset(*presetIdentifier->getGroupName(), *presetIdentifier->getName());
    }
    else
    {
        error.init();
        *error.getMessage() = QString("Preset already exists [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(deviceCenterFrequency)
                .arg(*presetIdentifier->getName());
        return 409;
    }

    MainWindow::MsgSavePreset *msg = MainWindow::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, true);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 202;
}

int WebAPIAdapterGUI::instancePresetDelete(
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*response.getGroupName(),
            response.getCenterFrequency(),
            *response.getName());

    if (selectedPreset == 0)
    {
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*response.getGroupName())
                .arg(response.getCenterFrequency())
                .arg(*response.getName());
        return 404;
    }

    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    MainWindow::MsgDeletePreset *msg = MainWindow::MsgDeletePreset::create(const_cast<Preset*>(selectedPreset));
    m_mainWindow.m_inputMessageQueue.push(msg);

    return 202;
}

int WebAPIAdapterGUI::instanceDeviceSetsGet(
        SWGSDRangel::SWGDeviceSetList& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    getDeviceSetList(&response);
    return 200;
}

int WebAPIAdapterGUI::instanceDeviceSetPost(
        bool tx,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    MainWindow::MsgAddDeviceSet *msg = MainWindow::MsgAddDeviceSet::create(tx);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    *response.getMessage() = QString("Message to add a new device set (MsgAddDeviceSet) was submitted successfully");

    return 202;
}

int WebAPIAdapterGUI::instanceDeviceSetDelete(
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if (m_mainWindow.m_deviceUIs.size() > 1)
    {
        MainWindow::MsgRemoveLastDeviceSet *msg = MainWindow::MsgRemoveLastDeviceSet::create();
        m_mainWindow.m_inputMessageQueue.push(msg);

        response.init();
        *response.getMessage() = QString("Message to remove last device set (MsgRemoveLastDeviceSet) was submitted successfully");

        return 202;
    }
    else
    {
        error.init();
        *error.getMessage() = "No more device sets to be removed";

        return 404;
    }
}

int WebAPIAdapterGUI::devicesetGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceSet& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        const DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];
        getDeviceSet(&response, deviceSet, deviceSetIndex);

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
 }

int WebAPIAdapterGUI::devicesetFocusPatch(
        int deviceSetIndex,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        MainWindow::MsgDeviceSetFocus *msg = MainWindow::MsgDeviceSetFocus::create(deviceSetIndex);
        m_mainWindow.m_inputMessageQueue.push(msg);

        response.init();
        *response.getMessage() = QString("Message to focus on device set (MsgDeviceSetFocus) was submitted successfully");

        return 202;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapterGUI::devicesetDevicePut(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceListItem& query,
        SWGSDRangel::SWGDeviceListItem& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if ((query.getTx() == 0) && (deviceSet->m_deviceSinkEngine))
        {
            error.init();
            *error.getMessage() = QString("Device type (Rx) and device set type (Tx) mismatch");
            return 404;
        }

        if ((query.getTx() != 0) && (deviceSet->m_deviceSourceEngine))
        {
            error.init();
            *error.getMessage() = QString("Device type (Tx) and device set type (Rx) mismatch");
            return 404;
        }

        int nbSamplingDevices = query.getTx() != 0 ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();
        int tx = query.getTx();

        for (int i = 0; i < nbSamplingDevices; i++)
        {
            PluginInterface::SamplingDevice samplingDevice = query.getTx() ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);

            if (query.getDisplayedName() && (*query.getDisplayedName() != samplingDevice.displayedName)) {
                continue;
            }

            if (query.getHwType() && (*query.getHwType() != samplingDevice.hardwareId)) {
                continue;
            }

            if ((query.getSequence() >= 0) && (query.getSequence() != samplingDevice.sequence)) {
                continue;
            }

            if (query.getSerial() && (*query.getSerial() != samplingDevice.serial)) {
                continue;
            }

            if ((query.getStreamIndex() >= 0) && (query.getStreamIndex() != samplingDevice.deviceItemIndex)) {
                continue;
            }

            MainWindow::MsgSetDevice *msg = MainWindow::MsgSetDevice::create(deviceSetIndex, i, query.getTx() != 0);
            m_mainWindow.m_inputMessageQueue.push(msg);

            response.init();
            *response.getDisplayedName() = samplingDevice.displayedName;
            *response.getHwType() = samplingDevice.hardwareId;
            *response.getSerial() = samplingDevice.serial;
            response.setSequence(samplingDevice.sequence);
            response.setTx(tx);
            response.setNbStreams(samplingDevice.deviceNbItems);
            response.setStreamIndex(samplingDevice.deviceItemIndex);
            response.setDeviceSetIndex(deviceSetIndex);
            response.setIndex(i);

            return 202;
        }

        error.init();
        *error.getMessage() = QString("Device not found");
        return 404;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetDeviceSettingsGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceSourceAPI->getHardwareId()));
            response.setTx(0);
            DeviceSampleSource *source = deviceSet->m_deviceSourceAPI->getSampleSource();
            return source->webapiSettingsGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceSinkAPI->getHardwareId()));
            response.setTx(1);
            DeviceSampleSink *sink = deviceSet->m_deviceSinkAPI->getSampleSink();
            return sink->webapiSettingsGet(response, *error.getMessage());
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetDeviceSettingsPutPatch(
        int deviceSetIndex,
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            if (response.getTx() != 0)
            {
                *error.getMessage() = QString("Rx device found but Tx device requested");
                return 400;
            }
            if (deviceSet->m_deviceSourceAPI->getHardwareId() != *response.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 input").arg(deviceSet->m_deviceSourceAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleSource *source = deviceSet->m_deviceSourceAPI->getSampleSource();
                return source->webapiSettingsPutPatch(force, deviceSettingsKeys, response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            if (response.getTx() == 0)
            {
                *error.getMessage() = QString("Tx device found but Rx device requested");
                return 400;
            }
            else if (deviceSet->m_deviceSinkAPI->getHardwareId() != *response.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 output").arg(deviceSet->m_deviceSinkAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleSink *sink = deviceSet->m_deviceSinkAPI->getSampleSink();
                return sink->webapiSettingsPutPatch(force, deviceSettingsKeys, response, *error.getMessage());
            }
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetDeviceRunGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            DeviceSampleSource *source = deviceSet->m_deviceSourceAPI->getSampleSource();
            response.init();
            return source->webapiRunGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            DeviceSampleSink *sink = deviceSet->m_deviceSinkAPI->getSampleSink();
            response.init();
            return sink->webapiRunGet(response, *error.getMessage());
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetDeviceRunPost(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            DeviceSampleSource *source = deviceSet->m_deviceSourceAPI->getSampleSource();
            response.init();
            return source->webapiRun(true, response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            DeviceSampleSink *sink = deviceSet->m_deviceSinkAPI->getSampleSink();
            response.init();
            return sink->webapiRun(true, response, *error.getMessage());
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetDeviceRunDelete(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            DeviceSampleSource *source = deviceSet->m_deviceSourceAPI->getSampleSource();
            response.init();
            return source->webapiRun(false, response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            DeviceSampleSink *sink = deviceSet->m_deviceSinkAPI->getSampleSink();
            response.init();
            return sink->webapiRun(false, response, *error.getMessage());
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}


int WebAPIAdapterGUI::devicesetDeviceReportGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceReport& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceSourceAPI->getHardwareId()));
            response.setTx(0);
            DeviceSampleSource *source = deviceSet->m_deviceSourceAPI->getSampleSource();
            return source->webapiReportGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceSinkAPI->getHardwareId()));
            response.setTx(1);
            DeviceSampleSink *sink = deviceSet->m_deviceSinkAPI->getSampleSink();
            return sink->webapiReportGet(response, *error.getMessage());
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetChannelsReportGet(
        int deviceSetIndex,
        SWGSDRangel::SWGChannelsDetail& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        const DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];
        getChannelsDetail(&response, deviceSet);

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapterGUI::devicesetChannelPost(
            int deviceSetIndex,
            SWGSDRangel::SWGChannelSettings& query,
			SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (query.getTx() == 0) // Rx
        {
            if (deviceSet->m_deviceSourceEngine == 0)
            {
                error.init();
                *error.getMessage() = QString("Device set at %1 is not a receive device set").arg(deviceSetIndex);
                return 400;
            }

            PluginAPI::ChannelRegistrations *channelRegistrations = m_mainWindow.m_pluginManager->getRxChannelRegistrations();
            int nbRegistrations = channelRegistrations->size();
            int index = 0;
            for (; index < nbRegistrations; index++)
            {
                if (channelRegistrations->at(index).m_channelId == *query.getChannelType()) {
                    break;
                }
            }

            if (index < nbRegistrations)
            {
                MainWindow::MsgAddChannel *msg = MainWindow::MsgAddChannel::create(deviceSetIndex, index, false);
                m_mainWindow.m_inputMessageQueue.push(msg);

                response.init();
                *response.getMessage() = QString("Message to add a channel (MsgAddChannel) was submitted successfully");

                return 202;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("There is no receive channel with id %1").arg(*query.getChannelType());
                return 404;
            }
        }
        else // Tx
        {
            if (deviceSet->m_deviceSinkEngine == 0)
            {
                error.init();
                *error.getMessage() = QString("Device set at %1 is not a transmit device set").arg(deviceSetIndex);
                return 400;
            }

            PluginAPI::ChannelRegistrations *channelRegistrations = m_mainWindow.m_pluginManager->getTxChannelRegistrations();
            int nbRegistrations = channelRegistrations->size();
            int index = 0;
            for (; index < nbRegistrations; index++)
            {
                if (channelRegistrations->at(index).m_channelId == *query.getChannelType()) {
                    break;
                }
            }

            if (index < nbRegistrations)
            {
                MainWindow::MsgAddChannel *msg = MainWindow::MsgAddChannel::create(deviceSetIndex, index, true);
                m_mainWindow.m_inputMessageQueue.push(msg);

                response.init();
                *response.getMessage() = QString("Message to add a channel (MsgAddChannel) was submitted successfully");

                return 202;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("There is no transmit channel with id %1").arg(*query.getChannelType());
                return 404;
            }
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetChannelDelete(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            if (channelIndex < deviceSet->getNumberOfRxChannels())
            {
                MainWindow::MsgDeleteChannel *msg = MainWindow::MsgDeleteChannel::create(deviceSetIndex, channelIndex, false);
                m_mainWindow.m_inputMessageQueue.push(msg);

                response.init();
                *response.getMessage() = QString("Message to delete a channel (MsgDeleteChannel) was submitted successfully");

                return 202;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("There is no channel at index %1. There are %2 Rx channels")
                        .arg(channelIndex)
                        .arg(channelIndex < deviceSet->getNumberOfRxChannels());
                return 400;
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            if (channelIndex < deviceSet->getNumberOfTxChannels())
            {
                MainWindow::MsgDeleteChannel *msg = MainWindow::MsgDeleteChannel::create(deviceSetIndex, channelIndex, true);
                m_mainWindow.m_inputMessageQueue.push(msg);

                response.init();
                *response.getMessage() = QString("Message to delete a channel (MsgDeleteChannel) was submitted successfully");

                return 202;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("There is no channel at index %1. There are %2 Tx channels")
                        .arg(channelIndex)
                        .arg(channelIndex < deviceSet->getNumberOfRxChannels());
                return 400;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetChannelSettingsGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            ChannelSinkAPI *channelAPI = deviceSet->m_deviceSourceAPI->getChanelAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setTx(0);
                return channelAPI->webapiSettingsGet(response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            ChannelSourceAPI *channelAPI = deviceSet->m_deviceSinkAPI->getChanelAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setTx(1);
                return channelAPI->webapiSettingsGet(response, *error.getMessage());
            }
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}


int WebAPIAdapterGUI::devicesetChannelReportGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelReport& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            ChannelSinkAPI *channelAPI = deviceSet->m_deviceSourceAPI->getChanelAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setTx(0);
                return channelAPI->webapiReportGet(response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            ChannelSourceAPI *channelAPI = deviceSet->m_deviceSinkAPI->getChanelAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setTx(1);
                return channelAPI->webapiReportGet(response, *error.getMessage());
            }
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapterGUI::devicesetChannelSettingsPutPatch(
        int deviceSetIndex,
        int channelIndex,
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Rx
        {
            ChannelSinkAPI *channelAPI = deviceSet->m_deviceSourceAPI->getChanelAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                QString channelType;
                channelAPI->getIdentifier(channelType);

                if (channelType == *response.getChannelType())
                {
                    return channelAPI->webapiSettingsPutPatch(force, channelSettingsKeys, response, *error.getMessage());
                }
                else
                {
                    *error.getMessage() = QString("There is no channel type %1 at index %2. Found %3.")
                            .arg(*response.getChannelType())
                            .arg(channelIndex)
                            .arg(channelType);
                    return 404;
                }
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            ChannelSourceAPI *channelAPI = deviceSet->m_deviceSinkAPI->getChanelAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                QString channelType;
                channelAPI->getIdentifier(channelType);

                if (channelType == *response.getChannelType())
                {
                    return channelAPI->webapiSettingsPutPatch(force, channelSettingsKeys, response, *error.getMessage());
                }
                else
                {
                    *error.getMessage() = QString("There is no channel type %1 at index %2. Found %3.")
                            .arg(*response.getChannelType())
                            .arg(channelIndex)
                            .arg(channelType);
                    return 404;
                }
            }
        }
        else
        {
            *error.getMessage() = QString("DeviceSet error");
            return 500;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }

}

void WebAPIAdapterGUI::getDeviceSetList(SWGSDRangel::SWGDeviceSetList* deviceSetList)
{
    deviceSetList->init();
    deviceSetList->setDevicesetcount((int) m_mainWindow.m_deviceUIs.size());

    if (m_mainWindow.m_deviceUIs.size() > 0) {
        deviceSetList->setDevicesetfocus(m_mainWindow.ui->tabInputsView->currentIndex());
    }

    std::vector<DeviceUISet*>::const_iterator it = m_mainWindow.m_deviceUIs.begin();

    for (int i = 0; it != m_mainWindow.m_deviceUIs.end(); ++it, i++)
    {
        QList<SWGSDRangel::SWGDeviceSet*> *deviceSets = deviceSetList->getDeviceSets();
        deviceSets->append(new SWGSDRangel::SWGDeviceSet());

        getDeviceSet(deviceSets->back(), *it, i);
    }
}

void WebAPIAdapterGUI::getDeviceSet(SWGSDRangel::SWGDeviceSet *deviceSet, const DeviceUISet* deviceUISet, int deviceUISetIndex)
{
    deviceSet->init();
    SWGSDRangel::SWGSamplingDevice *samplingDevice = deviceSet->getSamplingDevice();
    samplingDevice->init();
    samplingDevice->setIndex(deviceUISetIndex);
    samplingDevice->setTx(deviceUISet->m_deviceSinkEngine != 0);

    if (deviceUISet->m_deviceSinkEngine) // Tx data
    {
        *samplingDevice->getHwType() = deviceUISet->m_deviceSinkAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceUISet->m_deviceSinkAPI->getSampleSinkSerial();
        samplingDevice->setSequence(deviceUISet->m_deviceSinkAPI->getSampleSinkSequence());
        samplingDevice->setNbStreams(deviceUISet->m_deviceSinkAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceUISet->m_deviceSinkAPI->getItemIndex());
        deviceUISet->m_deviceSinkAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSink *sampleSink = deviceUISet->m_deviceSinkEngine->getSink();

        if (sampleSink) {
            samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSink->getSampleRate());
        }

        deviceSet->setChannelcount(deviceUISet->m_deviceSinkAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = deviceSet->getChannels();

        for (int i = 0; i <  deviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelSourceAPI *channel = deviceUISet->m_deviceSinkAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceUISet->m_deviceSourceEngine) // Rx data
    {
        *samplingDevice->getHwType() = deviceUISet->m_deviceSourceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceUISet->m_deviceSourceAPI->getSampleSourceSerial();
        samplingDevice->setSequence(deviceUISet->m_deviceSourceAPI->getSampleSourceSequence());
        samplingDevice->setNbStreams(deviceUISet->m_deviceSourceAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceUISet->m_deviceSourceAPI->getItemIndex());
        deviceUISet->m_deviceSourceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSource *sampleSource = deviceUISet->m_deviceSourceEngine->getSource();

        if (sampleSource) {
            samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSource->getSampleRate());
        }

        deviceSet->setChannelcount(deviceUISet->m_deviceSourceAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = deviceSet->getChannels();

        for (int i = 0; i <  deviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelSinkAPI *channel = deviceUISet->m_deviceSourceAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }
}

void WebAPIAdapterGUI::getChannelsDetail(SWGSDRangel::SWGChannelsDetail *channelsDetail, const DeviceUISet* deviceUISet)
{
    channelsDetail->init();
    SWGSDRangel::SWGChannelReport *channelReport;
    QString channelReportError;

    if (deviceUISet->m_deviceSinkEngine) // Tx data
    {
        channelsDetail->setChannelcount(deviceUISet->m_deviceSinkAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();

        for (int i = 0; i <  channelsDetail->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelSourceAPI *channel = deviceUISet->m_deviceSinkAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());

            channelReport = new SWGSDRangel::SWGChannelReport();

            if (channel->webapiReportGet(*channelReport, channelReportError) != 501) {
                channels->back()->setReport(channelReport);
            } else {
                delete channelReport;
            }
        }
    }

    if (deviceUISet->m_deviceSourceEngine) // Rx data
    {
        channelsDetail->setChannelcount(deviceUISet->m_deviceSourceAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();

        for (int i = 0; i <  channelsDetail->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelSinkAPI *channel = deviceUISet->m_deviceSourceAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());

            channelReport = new SWGSDRangel::SWGChannelReport();

            if (channel->webapiReportGet(*channelReport, channelReportError) != 501) {
                channels->back()->setReport(channelReport);
            } else {
                delete channelReport;
            }
        }
    }
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
