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

#include <QApplication>
#include <QList>
#include <QSysInfo>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "loggerwithfile.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "device/deviceenumerator.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/dspengine.h"
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "channel/channelapi.h"
#include "webapi/webapiadapterbase.h"
#include "util/serialutil.h"

#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceConfigResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGDeviceListItem.h"
#include "SWGAudioDevices.h"
#include "SWGLocationInformation.h"
#include "SWGDVSerialDevices.h"
#include "SWGDVSerialDevice.h"
#include "SWGAMBEDevices.h"
#include "SWGPresets.h"
#include "SWGPresetGroup.h"
#include "SWGPresetItem.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"
#include "SWGChannelsDetail.h"
#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGDeviceState.h"
#include "SWGLimeRFEDevices.h"
#include "SWGLimeRFESettings.h"
#include "SWGLimeRFEPower.h"

#ifdef HAS_LIMERFEUSB
#include "limerfe/limerfecontroller.h"
#endif

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
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
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
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) response;
    *error.getMessage() = QString("Not supported in GUI instance");
    return 400;
}

int WebAPIAdapterGUI::instanceConfigGet(
        SWGSDRangel::SWGInstanceConfigResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    response.init();
    WebAPIAdapterBase webAPIAdapterBase;
    webAPIAdapterBase.setPluginManager(m_mainWindow.getPluginManager());
    SWGSDRangel::SWGPreferences *preferences = response.getPreferences();
    WebAPIAdapterBase::webapiFormatPreferences(preferences, m_mainWindow.getMainSettings().getPreferences());
    SWGSDRangel::SWGPreset *workingPreset = response.getWorkingPreset();
    webAPIAdapterBase.webapiFormatPreset(workingPreset, m_mainWindow.getMainSettings().getWorkingPresetConst());

    int nbPresets = m_mainWindow.m_settings.getPresetCount();
    QList<SWGSDRangel::SWGPreset*> *swgPresets = response.getPresets();

    for (int i = 0; i < nbPresets; i++)
    {
        const Preset *preset = m_mainWindow.m_settings.getPreset(i);
        swgPresets->append(new SWGSDRangel::SWGPreset);
        webAPIAdapterBase.webapiFormatPreset(swgPresets->back(), *preset);
    }

    int nbCommands = m_mainWindow.m_settings.getCommandCount();
    QList<SWGSDRangel::SWGCommand*> *swgCommands = response.getCommands();

    for (int i = 0; i < nbCommands; i++)
    {
        const Command *command = m_mainWindow.m_settings.getCommand(i);
        swgCommands->append(new SWGSDRangel::SWGCommand);
        WebAPIAdapterBase::webapiFormatCommand(swgCommands->back(), *command);
    }

    return 200;
}

int WebAPIAdapterGUI::instanceConfigPutPatch(
        bool force, // PUT else PATCH
        SWGSDRangel::SWGInstanceConfigResponse& query,
        const ConfigKeys& configKeys,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    WebAPIAdapterBase webAPIAdapterBase;
    webAPIAdapterBase.setPluginManager(m_mainWindow.getPluginManager());

    if (force) {
        webAPIAdapterBase.webapiInitConfig(m_mainWindow.m_settings);
    }

    Preferences newPreferences = m_mainWindow.m_settings.getPreferences();
    webAPIAdapterBase.webapiUpdatePreferences(query.getPreferences(), configKeys.m_preferencesKeys, newPreferences);
    m_mainWindow.m_settings.setPreferences(newPreferences);

    Preset *workingPreset = m_mainWindow.m_settings.getWorkingPreset();
    webAPIAdapterBase.webapiUpdatePreset(force, query.getWorkingPreset(), configKeys.m_workingPresetKeys, workingPreset);

    QList<PresetKeys>::const_iterator presetKeysIt = configKeys.m_presetKeys.begin();
    int i = 0;
    for (; presetKeysIt != configKeys.m_presetKeys.end(); ++presetKeysIt, i++)
    {
        Preset *newPreset = new Preset(); // created with default values
        SWGSDRangel::SWGPreset *swgPreset = query.getPresets()->at(i);
        webAPIAdapterBase.webapiUpdatePreset(force, swgPreset, *presetKeysIt, newPreset);
        m_mainWindow.m_settings.addPreset(newPreset);
    }

    QList<CommandKeys>::const_iterator commandKeysIt = configKeys.m_commandKeys.begin();
    i = 0;
    for (; commandKeysIt != configKeys.m_commandKeys.end(); ++commandKeysIt, i++)
    {
        Command *newCommand = new Command(); // created with default values
        SWGSDRangel::SWGCommand *swgCommand = query.getCommands()->at(i);
        webAPIAdapterBase.webapiUpdateCommand(swgCommand, *commandKeysIt, *newCommand);
        m_mainWindow.m_settings.addCommand(newCommand);
    }

    MainWindow::MsgApplySettings *msg = MainWindow::MsgApplySettings::create();
    m_mainWindow.m_inputMessageQueue.push(msg);

    return 200;
}

int WebAPIAdapterGUI::instanceDevices(
            int direction,
            SWGSDRangel::SWGInstanceDevicesResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();

    int nbSamplingDevices;

    if (direction == 0) { // Single Rx stream device
        nbSamplingDevices = DeviceEnumerator::instance()->getNbRxSamplingDevices();
    } else if (direction == 1) { // Single Tx stream device
        nbSamplingDevices = DeviceEnumerator::instance()->getNbTxSamplingDevices();
    } else if (direction == 2) { // MIMO device
        nbSamplingDevices = DeviceEnumerator::instance()->getNbMIMOSamplingDevices();
    } else { // not supported
        nbSamplingDevices = 0;
    }

    response.setDevicecount(nbSamplingDevices);
    QList<SWGSDRangel::SWGDeviceListItem*> *devices = response.getDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        const PluginInterface::SamplingDevice *samplingDevice = nullptr;

        if (direction == 0) {
            samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(i);
        } else if (direction == 1) {
            samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(i);
        } else if (direction == 2) {
            samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(i);
        } else {
            continue;
        }

        devices->append(new SWGSDRangel::SWGDeviceListItem);
        devices->back()->init();
        *devices->back()->getDisplayedName() = samplingDevice->displayedName;
        *devices->back()->getHwType() = samplingDevice->hardwareId;
        *devices->back()->getSerial() = samplingDevice->serial;
        devices->back()->setSequence(samplingDevice->sequence);
        devices->back()->setDirection((int) samplingDevice->streamType);
        devices->back()->setDeviceNbStreams(samplingDevice->deviceNbItems);
        devices->back()->setDeviceSetIndex(samplingDevice->claimed);
        devices->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterGUI::instanceChannels(
            int direction,
            SWGSDRangel::SWGInstanceChannelsResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();
    PluginAPI::ChannelRegistrations *channelRegistrations;
    int nbChannelDevices;

    if (direction == 0) // Single sink (Rx) channel
    {
        channelRegistrations = m_mainWindow.m_pluginManager->getRxChannelRegistrations();
        nbChannelDevices = channelRegistrations->size();
    }
    else if (direction == 1) // Single source (Tx) channel
    {
        channelRegistrations = m_mainWindow.m_pluginManager->getTxChannelRegistrations();
        nbChannelDevices = channelRegistrations->size();
    }
    else if (direction == 2) // MIMO channel
    {
        channelRegistrations = m_mainWindow.m_pluginManager->getMIMOChannelRegistrations();
        nbChannelDevices = channelRegistrations->size();
    }
    else // not supported
    {
        channelRegistrations = nullptr;
        nbChannelDevices = 0;
    }

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
        channels->back()->setDirection(direction);
        *channels->back()->getIdUri() = channelRegistrations->at(i).m_channelIdURI;
        *channels->back()->getId() = channelRegistrations->at(i).m_channelId;
        channels->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterGUI::instanceLoggingGet(
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
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
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
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
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
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
    outputDevices->back()->setUdpChannelCodec((int) outputDeviceInfo.udpChannelCodec);
    outputDevices->back()->setUdpDecimationFactor((int) outputDeviceInfo.udpDecimationFactor);
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
        outputDevices->back()->setUdpChannelCodec((int) outputDeviceInfo.udpChannelCodec);
        outputDevices->back()->setUdpDecimationFactor((int) outputDeviceInfo.udpDecimationFactor);
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
        outputDeviceInfo.udpChannelMode = static_cast<AudioOutput::UDPChannelMode>(response.getUdpChannelMode());
    }
    if (audioOutputKeys.contains("udpChannelCodec")) {
        outputDeviceInfo.udpChannelCodec = static_cast<AudioOutput::UDPChannelCodec>(response.getUdpChannelCodec());
    }
    if (audioOutputKeys.contains("udpDecimatiobFactor")) {
        outputDeviceInfo.udpDecimationFactor = response.getUdpDecimationFactor();
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
    response.setUdpChannelMode(outputDeviceInfo.udpChannelMode);
    response.setUdpChannelCodec(outputDeviceInfo.udpChannelCodec);
    response.setUdpDecimationFactor(outputDeviceInfo.udpDecimationFactor);

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
    response.setUdpChannelMode(outputDeviceInfo.udpChannelMode);
    response.setUdpChannelCodec(outputDeviceInfo.udpChannelCodec);
    response.setUdpDecimationFactor(outputDeviceInfo.udpDecimationFactor);

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
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->inputInfosCleanup();

    response.init();
    *response.getMessage() = QString("Unregistered parameters for devices not in list of available input devices for this instance");

    return 200;
}

int WebAPIAdapterGUI::instanceAudioOutputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    m_mainWindow.m_dspEngine->getAudioDeviceManager()->outputInfosCleanup();

    response.init();
    *response.getMessage() = QString("Unregistered parameters for devices not in list of available output devices for this instance");

    return 200;
}

int WebAPIAdapterGUI::instanceLocationGet(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();
    response.setLatitude(m_mainWindow.m_settings.getLatitude());
    response.setLongitude(m_mainWindow.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterGUI::instanceLocationPut(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
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

int WebAPIAdapterGUI::instanceDVSerialGet(
            SWGSDRangel::SWGDVSerialDevices& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();

    std::vector<std::string> deviceNames;
    m_mainWindow.m_dspEngine->getDVSerialNames(deviceNames);
    response.setNbDevices((int) deviceNames.size());
    QList<SWGSDRangel::SWGDVSerialDevice*> *deviceNamesList = response.getDvSerialDevices();

    std::vector<std::string>::iterator it = deviceNames.begin();

    while (it != deviceNames.end())
    {
        deviceNamesList->append(new SWGSDRangel::SWGDVSerialDevice);
        deviceNamesList->back()->init();
        *deviceNamesList->back()->getDeviceName() = QString::fromStdString(*it);
        ++it;
    }

    return 200;
}

int WebAPIAdapterGUI::instanceDVSerialPatch(
            bool dvserial,
            SWGSDRangel::SWGDVSerialDevices& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
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

int WebAPIAdapterGUI::instanceAMBESerialGet(
        SWGSDRangel::SWGDVSerialDevices& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();

    std::vector<std::string> deviceNames;
    std::vector<QString> qDeviceNames;
    m_mainWindow.m_dspEngine->getAMBEEngine()->scan(qDeviceNames);

    for (std::vector<QString>::const_iterator it = qDeviceNames.begin(); it != qDeviceNames.end(); ++it) {
        deviceNames.push_back(it->toStdString());
    }

    response.setNbDevices((int) deviceNames.size());
    QList<SWGSDRangel::SWGDVSerialDevice*> *deviceNamesList = response.getDvSerialDevices();

    std::vector<std::string>::iterator it = deviceNames.begin();

    while (it != deviceNames.end())
    {
        deviceNamesList->append(new SWGSDRangel::SWGDVSerialDevice);
        deviceNamesList->back()->init();
        *deviceNamesList->back()->getDeviceName() = QString::fromStdString(*it);
        ++it;
    }

    return 200;
}

int WebAPIAdapterGUI::instanceAMBEDevicesGet(
        SWGSDRangel::SWGAMBEDevices& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();

    std::vector<std::string> deviceNames;
    m_mainWindow.m_dspEngine->getDVSerialNames(deviceNames);
    response.setNbDevices((int) deviceNames.size());
    QList<SWGSDRangel::SWGAMBEDevice*> *deviceNamesList = response.getAmbeDevices();

    std::vector<std::string>::iterator it = deviceNames.begin();

    while (it != deviceNames.end())
    {
        deviceNamesList->append(new SWGSDRangel::SWGAMBEDevice);
        deviceNamesList->back()->init();
        *deviceNamesList->back()->getDeviceRef() = QString::fromStdString(*it);
        deviceNamesList->back()->setDelete(0);
        ++it;
    }

    return 200;
}

int WebAPIAdapterGUI::instanceAMBEDevicesDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    m_mainWindow.m_dspEngine->getAMBEEngine()->releaseAll();

    response.init();
    *response.getMessage() = QString("All AMBE devices released");

    return 200;
}

int WebAPIAdapterGUI::instanceAMBEDevicesPut(
        SWGSDRangel::SWGAMBEDevices& query,
        SWGSDRangel::SWGAMBEDevices& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    m_mainWindow.m_dspEngine->getAMBEEngine()->releaseAll();

    QList<SWGSDRangel::SWGAMBEDevice *> *ambeList = query.getAmbeDevices();

    for (QList<SWGSDRangel::SWGAMBEDevice *>::const_iterator it = ambeList->begin(); it != ambeList->end(); ++it) {
        m_mainWindow.m_dspEngine->getAMBEEngine()->registerController((*it)->getDeviceRef()->toStdString());
    }

    instanceAMBEDevicesGet(response, error);
    return 200;
}

int WebAPIAdapterGUI::instanceAMBEDevicesPatch(
        SWGSDRangel::SWGAMBEDevices& query,
        SWGSDRangel::SWGAMBEDevices& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    QList<SWGSDRangel::SWGAMBEDevice *> *ambeList = query.getAmbeDevices();

    for (QList<SWGSDRangel::SWGAMBEDevice *>::const_iterator it = ambeList->begin(); it != ambeList->end(); ++it)
    {
        if ((*it)->getDelete()) {
            m_mainWindow.m_dspEngine->getAMBEEngine()->releaseController((*it)->getDeviceRef()->toStdString());
        } else {
            m_mainWindow.m_dspEngine->getAMBEEngine()->registerController((*it)->getDeviceRef()->toStdString());
        }
    }

    instanceAMBEDevicesGet(response, error);
    return 200;
}

#ifdef HAS_LIMERFEUSB
int WebAPIAdapterGUI::instanceLimeRFESerialGet(
        SWGSDRangel::SWGLimeRFEDevices& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();
    std::vector<std::string> comPorts;
    SerialUtil::getComPorts(comPorts, "ttyUSB[0-9]+"); // regex is for Linux only
    response.setNbDevices((int) comPorts.size());
    QList<SWGSDRangel::SWGLimeRFEDevice*> *deviceNamesList = response.getLimeRfeDevices();
    std::vector<std::string>::iterator it = comPorts.begin();

    while (it != comPorts.end())
    {
        deviceNamesList->append(new SWGSDRangel::SWGLimeRFEDevice);
        deviceNamesList->back()->init();
        *deviceNamesList->back()->getDeviceRef() = QString::fromStdString(*it);
        ++it;
    }

    return 200;
}

int WebAPIAdapterGUI::instanceLimeRFEConfigGet(
        const QString& serial,
        SWGSDRangel::SWGLimeRFESettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    LimeRFEController controller;
    int rc = controller.openDevice(serial.toStdString());

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error opening LimeRFE device %1: %2")
            .arg(serial).arg(controller.getError(rc).c_str());
        return 400;
    }

    rc = controller.getState();

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error getting config from LimeRFE device %1: %2")
            .arg(serial).arg(controller.getError(rc).c_str());
        return 500;
    }

    controller.closeDevice();

    LimeRFEController::LimeRFESettings settings;
    controller.stateToSettings(settings);
    response.init();
    response.setDevicePath(new QString(serial));
    response.setRxChannels((int) settings.m_rxChannels);
    response.setRxWidebandChannel((int) settings.m_rxWidebandChannel);
    response.setRxHamChannel((int) settings.m_rxHAMChannel);
    response.setRxCellularChannel((int) settings.m_rxCellularChannel);
    response.setRxPort((int) settings.m_rxPort);
    response.setRxOn(settings.m_rxOn ? 1 : 0);
    response.setAmfmNotch(settings.m_amfmNotch ? 1 : 0);
    response.setAttenuationFactor(settings.m_attenuationFactor);
    response.setTxChannels((int) settings.m_txChannels);
    response.setTxWidebandChannel((int) settings.m_txWidebandChannel);
    response.setTxHamChannel((int) settings.m_txHAMChannel);
    response.setTxCellularChannel((int) settings.m_txCellularChannel);
    response.setTxPort((int) settings.m_txPort);
    response.setTxOn(settings.m_txOn ? 1 : 0);
    response.setSwrEnable(settings.m_swrEnable ? 1 : 0);
    response.setSwrSource((int) settings.m_swrSource);

    return 200;
}

int WebAPIAdapterGUI::instanceLimeRFEConfigPut(
        SWGSDRangel::SWGLimeRFESettings& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    LimeRFEController controller;
    int rc = controller.openDevice(query.getDevicePath()->toStdString());

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error opening LimeRFE device %1: %2")
            .arg(*query.getDevicePath()).arg(controller.getError(rc).c_str());
        return 400;
    }

    LimeRFEController::LimeRFESettings settings;
    settings.m_rxChannels = (LimeRFEController::ChannelGroups) query.getRxChannels();
    settings.m_rxWidebandChannel = (LimeRFEController::WidebandChannel) query.getRxWidebandChannel();
    settings.m_rxHAMChannel = (LimeRFEController::HAMChannel) query.getRxHamChannel();
    settings.m_rxCellularChannel = (LimeRFEController::CellularChannel) query.getRxCellularChannel();
    settings.m_rxPort = (LimeRFEController::RxPort) query.getRxPort();
    settings.m_rxOn = query.getRxOn() != 0;
    settings.m_amfmNotch = query.getAmfmNotch() != 0;
    settings.m_attenuationFactor = query.getAttenuationFactor();
    settings.m_txChannels = (LimeRFEController::ChannelGroups) query.getTxChannels();
    settings.m_txWidebandChannel = (LimeRFEController::WidebandChannel) query.getTxWidebandChannel();
    settings.m_txHAMChannel = (LimeRFEController::HAMChannel) query.getTxHamChannel();
    settings.m_txCellularChannel = (LimeRFEController::CellularChannel) query.getTxCellularChannel();
    settings.m_txPort = (LimeRFEController::TxPort) query.getTxPort();
    settings.m_txOn = query.getTxOn() != 0;
    settings.m_swrEnable = query.getSwrEnable() != 0;
    settings.m_swrSource = (LimeRFEController::SWRSource) query.getSwrSource();

    controller.settingsToState(settings);

    rc = controller.configure();

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error configuring LimeRFE device %1: %2")
            .arg(*query.getDevicePath()).arg(controller.getError(rc).c_str());
        return 500;
    }

    response.init();
    *response.getMessage() = QString("LimeRFE device at %1 configuration updated successfully").arg(*query.getDevicePath());
    return 200;
}

int WebAPIAdapterGUI::instanceLimeRFERunPut(
        SWGSDRangel::SWGLimeRFESettings& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    LimeRFEController controller;
    int rc = controller.openDevice(query.getDevicePath()->toStdString());

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error opening LimeRFE device %1: %2")
            .arg(*query.getDevicePath()).arg(controller.getError(rc).c_str());
        return 400;
    }

    LimeRFEController::LimeRFESettings settings;
    settings.m_rxOn = query.getRxOn() != 0;
    settings.m_txOn = query.getTxOn() != 0;

    rc = controller.setRx(settings, settings.m_rxOn);

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error setting Rx/Tx LimeRFE device %1: %2")
            .arg(*query.getDevicePath()).arg(controller.getError(rc).c_str());
        return 400;
    }

    response.init();
    *response.getMessage() = QString("LimeRFE device at %1 mode updated successfully").arg(*query.getDevicePath());
    return 200;
}

int WebAPIAdapterGUI::instanceLimeRFEPowerGet(
        const QString& serial,
        SWGSDRangel::SWGLimeRFEPower& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    LimeRFEController controller;
    int rc = controller.openDevice(serial.toStdString());

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error opening LimeRFE device %1: %2")
            .arg(serial).arg(controller.getError(rc).c_str());
        return 400;
    }

    int fwdPower;
    rc = controller.getFwdPower(fwdPower);

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error getting forward power from LimeRFE device %1: %2")
            .arg(serial).arg(controller.getError(rc).c_str());
        return 500;
    }

    int refPower;
    rc = controller.getRefPower(refPower);

    if (rc != 0)
    {
        error.init();
        *error.getMessage() = QString("Error getting reflected power from LimeRFE device %1: %2")
            .arg(serial).arg(controller.getError(rc).c_str());
        return 500;
    }

    controller.closeDevice();

    response.init();
    response.setForward(fwdPower);
    response.setReflected(refPower);
    return 200;
}
#endif

int WebAPIAdapterGUI::instancePresetsGet(
        SWGSDRangel::SWGPresets& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
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
        *swgPresets->back()->getType() = preset->isSourcePreset() ? "R" : preset->isSinkPreset() ? "T" : preset->isMIMOPreset() ? "M" : "X";
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
            *presetIdentifier->getName(),
            *presetIdentifier->getType());

    if (selectedPreset == 0)
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2, %3 %4]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName())
                .arg(*presetIdentifier->getType());
        return 404;
    }

    DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];

    if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
    {
        error.init();
        *error.getMessage() = QString("Preset type and device set type (Rx) mismatch");
        return 404;
    }

    if (deviceUI->m_deviceSinkEngine && !selectedPreset->isSinkPreset())
    {
        error.init();
        *error.getMessage() = QString("Preset type and device set type (Tx) mismatch");
        return 404;
    }

    if (deviceUI->m_deviceMIMOEngine && !selectedPreset->isMIMOPreset())
    {
        error.init();
        *error.getMessage() = QString("Preset type and device set type (MIMO) mismatch");
        return 404;
    }

    MainWindow::MsgLoadPreset *msg = MainWindow::MsgLoadPreset::create(selectedPreset, deviceSetIndex);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : selectedPreset->isSinkPreset() ? "T" : selectedPreset->isMIMOPreset() ? "M" : "X";
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
            *presetIdentifier->getName(),
            *presetIdentifier->getType());

    if (selectedPreset == 0)
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2, %3 %4]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName())
                .arg(*presetIdentifier->getType());
        return 404;
    }
    else // update existing preset
    {
        DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
        {
            error.init();
            *error.getMessage() = QString("Preset type and device set type (Rx) mismatch");
            return 404;
        }

        if (deviceUI->m_deviceSinkEngine && !selectedPreset->isSinkPreset())
        {
            error.init();
            *error.getMessage() = QString("Preset type and device set type (Tx) mismatch");
            return 404;
        }

        if (deviceUI->m_deviceMIMOEngine && !selectedPreset->isMIMOPreset())
        {
            error.init();
            *error.getMessage() = QString("Preset type and device set type (MIMO) mismatch");
            return 404;
        }
    }

    MainWindow::MsgSavePreset *msg = MainWindow::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, false);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : selectedPreset->isSinkPreset() ? "T": selectedPreset->isMIMOPreset() ? "M" : "X";
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
    } else if (deviceSet->m_deviceMIMOEngine) { // MIMO
        deviceCenterFrequency = deviceSet->m_deviceMIMOEngine->getMIMO()->getMIMOCenterFrequency();
    } else {
        error.init();
        *error.getMessage() = QString("Device set error");
        return 500;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
            deviceCenterFrequency,
            *presetIdentifier->getName(),
            *presetIdentifier->getType());

    if (selectedPreset == 0) // save on a new preset
    {
        selectedPreset = m_mainWindow.m_settings.newPreset(*presetIdentifier->getGroupName(), *presetIdentifier->getName());
    }
    else
    {
        error.init();
        *error.getMessage() = QString("Preset already exists [%1, %2, %3 %4]")
                .arg(*presetIdentifier->getGroupName())
                .arg(deviceCenterFrequency)
                .arg(*presetIdentifier->getName())
                .arg(*presetIdentifier->getType());
        return 409;
    }

    MainWindow::MsgSavePreset *msg = MainWindow::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, true);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : selectedPreset->isSinkPreset() ? "T" : selectedPreset->isMIMOPreset() ? "M" : "X";
    *response.getName() = selectedPreset->getDescription();

    return 202;
}

int WebAPIAdapterGUI::instancePresetDelete(
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*response.getGroupName(),
            response.getCenterFrequency(),
            *response.getName(),
            *response.getType());

    if (selectedPreset == 0)
    {
        *error.getMessage() = QString("There is no preset [%1, %2, %3 %4]")
                .arg(*response.getGroupName())
                .arg(response.getCenterFrequency())
                .arg(*response.getName())
                .arg(*response.getType());
        return 404;
    }

    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : selectedPreset->isSinkPreset() ? "T" : selectedPreset->isMIMOPreset() ? "M" : "X";
    *response.getName() = selectedPreset->getDescription();

    MainWindow::MsgDeletePreset *msg = MainWindow::MsgDeletePreset::create(const_cast<Preset*>(selectedPreset));
    m_mainWindow.m_inputMessageQueue.push(msg);

    return 202;
}

int WebAPIAdapterGUI::instanceDeviceSetsGet(
        SWGSDRangel::SWGDeviceSetList& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    getDeviceSetList(&response);
    return 200;
}

int WebAPIAdapterGUI::instanceDeviceSetPost(
        int direction,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    MainWindow::MsgAddDeviceSet *msg = MainWindow::MsgAddDeviceSet::create(direction);
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

        if ((query.getDirection() != 1) && (deviceSet->m_deviceSinkEngine))
        {
            error.init();
            *error.getMessage() = QString("Device type and device set type (Tx) mismatch");
            return 404;
        }

        if ((query.getDirection() != 0) && (deviceSet->m_deviceSourceEngine))
        {
            error.init();
            *error.getMessage() = QString("Device type and device set type (Rx) mismatch");
            return 404;
        }

        if ((query.getDirection() != 2) && (deviceSet->m_deviceMIMOEngine))
        {
            error.init();
            *error.getMessage() = QString("Device type and device set type (MIMO) mismatch");
            return 404;
        }

        int nbSamplingDevices;

        if (query.getDirection() == 0) {
            nbSamplingDevices = DeviceEnumerator::instance()->getNbRxSamplingDevices();
        } else if (query.getDirection() == 1) {
            nbSamplingDevices = DeviceEnumerator::instance()->getNbTxSamplingDevices();
        } else if (query.getDirection() == 2) {
            nbSamplingDevices = DeviceEnumerator::instance()->getNbMIMOSamplingDevices();
        } else {
            nbSamplingDevices = 0; // TODO: not implemented yet
        }


        for (int i = 0; i < nbSamplingDevices; i++)
        {
            const PluginInterface::SamplingDevice *samplingDevice;

            if (query.getDirection() == 0) {
                samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(i);
            } else if (query.getDirection() == 1) {
                samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(i);
            } else if (query.getDirection() == 2) {
                samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(i);
            } else {
                continue; // device not supported
            }

            if (query.getDisplayedName() && (*query.getDisplayedName() != samplingDevice->displayedName)) {
                continue;
            }

            if (query.getHwType() && (*query.getHwType() != samplingDevice->hardwareId)) {
                continue;
            }

            if ((query.getSequence() >= 0) && (query.getSequence() != samplingDevice->sequence)) {
                continue;
            }

            if (query.getSerial() && (*query.getSerial() != samplingDevice->serial)) {
                continue;
            }

            if ((query.getDeviceStreamIndex() >= 0) && (query.getDeviceStreamIndex() != samplingDevice->deviceItemIndex)) {
                continue;
            }

            MainWindow::MsgSetDevice *msg = MainWindow::MsgSetDevice::create(deviceSetIndex, i, query.getDirection());
            m_mainWindow.m_inputMessageQueue.push(msg);

            response.init();
            *response.getDisplayedName() = samplingDevice->displayedName;
            *response.getHwType() = samplingDevice->hardwareId;
            *response.getSerial() = samplingDevice->serial;
            response.setSequence(samplingDevice->sequence);
            response.setDirection(query.getDirection());
            response.setDeviceNbStreams(samplingDevice->deviceNbItems);
            response.setDeviceStreamIndex(samplingDevice->deviceItemIndex);
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

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            response.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            return source->webapiSettingsGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            response.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            return sink->webapiSettingsGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            response.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            return mimo->webapiSettingsGet(response, *error.getMessage());
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

int WebAPIAdapterGUI::devicesetDeviceActionsPost(
        int deviceSetIndex,
        const QStringList& deviceActionsKeys,
        SWGSDRangel::SWGDeviceActions& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            if (query.getDirection() != 0)
            {
                *error.getMessage() = QString("Single Rx device found but other type of device requested");
                return 400;
            }
            if (deviceSet->m_deviceAPI->getHardwareId() != *query.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 input").arg(deviceSet->m_deviceAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
                int res = source->webapiActionsPost(deviceActionsKeys, query, *error.getMessage());

                if (res/100 == 2)
                {
                    response.init();
                    *response.getMessage() = QString("Message to post action was submitted successfully");
                }

                return res;
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            if (query.getDirection() != 1)
            {
                *error.getMessage() = QString("Single Tx device found but other type of device requested");
                return 400;
            }
            else if (deviceSet->m_deviceAPI->getHardwareId() != *query.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 output").arg(deviceSet->m_deviceAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
                int res = sink->webapiActionsPost(deviceActionsKeys, query, *error.getMessage());

                if (res/100 == 2)
                {
                    response.init();
                    *response.getMessage() = QString("Message to post action was submitted successfully");
                }

                return res;
            }
        }
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            if (query.getDirection() != 2)
            {
                *error.getMessage() = QString("MIMO device found but other type of device requested");
                return 400;
            }
            else if (deviceSet->m_deviceAPI->getHardwareId() != *query.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 output").arg(deviceSet->m_deviceAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
                int res = mimo->webapiActionsPost(deviceActionsKeys, query, *error.getMessage());

                if (res/100 == 2)
                {
                    response.init();
                    *response.getMessage() = QString("Message to post action was submitted successfully");
                }

                return res;
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

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            if (response.getDirection() != 0)
            {
                *error.getMessage() = QString("Single Rx device found but other type of device requested");
                return 400;
            }
            if (deviceSet->m_deviceAPI->getHardwareId() != *response.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 input").arg(deviceSet->m_deviceAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
                return source->webapiSettingsPutPatch(force, deviceSettingsKeys, response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            if (response.getDirection() != 1)
            {
                *error.getMessage() = QString("Single Tx device found but other type of device requested");
                return 400;
            }
            else if (deviceSet->m_deviceAPI->getHardwareId() != *response.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 output").arg(deviceSet->m_deviceAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
                return sink->webapiSettingsPutPatch(force, deviceSettingsKeys, response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            if (response.getDirection() != 2)
            {
                *error.getMessage() = QString("MIMO device found but other type of device requested");
                return 400;
            }
            else if (deviceSet->m_deviceAPI->getHardwareId() != *response.getDeviceHwType())
            {
                *error.getMessage() = QString("Device mismatch. Found %1 output").arg(deviceSet->m_deviceAPI->getHardwareId());
                return 400;
            }
            else
            {
                DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
                return mimo->webapiSettingsPutPatch(force, deviceSettingsKeys, response, *error.getMessage());
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
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            response.init();
            return source->webapiRunGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
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

int WebAPIAdapterGUI::devicesetDeviceSubsystemRunGet(
        int deviceSetIndex,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            response.init();
            return mimo->webapiRunGet(subsystemIndex, response, *error.getMessage());
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
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            response.init();
            return source->webapiRun(true, response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
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

int WebAPIAdapterGUI::devicesetDeviceSubsystemRunPost(
        int deviceSetIndex,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            response.init();
            return mimo->webapiRun(true, subsystemIndex, response, *error.getMessage());
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
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            response.init();
            return source->webapiRun(false, response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Tx
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
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

int WebAPIAdapterGUI::devicesetDeviceSubsystemRunDelete(
        int deviceSetIndex,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            response.init();
            return mimo->webapiRun(false, subsystemIndex, response, *error.getMessage());
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

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            response.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            return source->webapiReportGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            response.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            return sink->webapiReportGet(response, *error.getMessage());
        }
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            response.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            response.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            return mimo->webapiReportGet(response, *error.getMessage());
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

        if (query.getDirection() == 0) // Single Rx
        {
            if (!deviceSet->m_deviceSourceEngine && !deviceSet->m_deviceMIMOEngine)
            {
                error.init();
                *error.getMessage() = QString("Device set at %1 is not a receive capable device set").arg(deviceSetIndex);
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
        else if (query.getDirection() == 1) // single Tx
        {
            if (!deviceSet->m_deviceSinkEngine && !deviceSet->m_deviceMIMOEngine)
            {
                error.init();
                *error.getMessage() = QString("Device set at %1 is not a transmit capable device set").arg(deviceSetIndex);
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
        else if (query.getDirection() == 2) // MIMO
        {
            if (!deviceSet->m_deviceMIMOEngine)
            {
                error.init();
                *error.getMessage() = QString("Device set at %1 is not a MIMO capable device set").arg(deviceSetIndex);
                return 400;
            }

            PluginAPI::ChannelRegistrations *channelRegistrations = m_mainWindow.m_pluginManager->getMIMOChannelRegistrations();
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
                *error.getMessage() = QString("There is no MIMO channel with id %1").arg(*query.getChannelType());
                return 404;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("This type of device is not implemented yet");
            return 400;
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

        if (channelIndex < deviceSet->getNumberOfChannels())
        {
            MainWindow::MsgDeleteChannel *msg = MainWindow::MsgDeleteChannel::create(deviceSetIndex, channelIndex);
            m_mainWindow.m_inputMessageQueue.push(msg);

            response.init();
            *response.getMessage() = QString("Message to delete a channel (MsgDeleteChannel) was submitted successfully");

            return 202;
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no channel at index %1. There are %2 channels")
                    .arg(channelIndex)
                    .arg(channelIndex < deviceSet->getNumberOfChannels());
            return 400;
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

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setDirection(0);
                return channelAPI->webapiSettingsGet(response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setDirection(1);
                return channelAPI->webapiSettingsGet(response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            int nbSinkChannels = deviceSet->m_deviceAPI->getNbSinkChannels();
            int nbSourceChannels = deviceSet->m_deviceAPI->getNbSourceChannels();
            int nbMIMOChannels = deviceSet->m_deviceAPI->getNbMIMOChannels();
            ChannelAPI *channelAPI = nullptr;

            if (channelIndex < nbSinkChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);
                response.setDirection(0);
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex - nbSinkChannels);
                response.setDirection(1);
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels + nbMIMOChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getMIMOChannelAPIAt(channelIndex - nbSinkChannels - nbSourceChannels);
                response.setDirection(2);
            }
            else
            {
                *error.getMessage() = QString("Ther is no channel with index %1").arg(channelIndex);
                return 404;
            }

            if (channelAPI)
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                return channelAPI->webapiSettingsGet(response, *error.getMessage());
            }
            else
            {
                *error.getMessage() = QString("Ther is no channel with index %1").arg(channelIndex);
                return 404;
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

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setDirection(0);
                return channelAPI->webapiReportGet(response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                response.setDirection(1);
                return channelAPI->webapiReportGet(response, *error.getMessage());
            }
        }
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            int nbSinkChannels = deviceSet->m_deviceAPI->getNbSinkChannels();
            int nbSourceChannels = deviceSet->m_deviceAPI->getNbSourceChannels();
            int nbMIMOChannels = deviceSet->m_deviceAPI->getNbMIMOChannels();
            ChannelAPI *channelAPI = nullptr;

            if (channelIndex < nbSinkChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);
                response.setDirection(0);
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex - nbSinkChannels);
                response.setDirection(1);
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels + nbMIMOChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getMIMOChannelAPIAt(channelIndex - nbSinkChannels - nbSourceChannels);
                response.setDirection(2);
            }
            else
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }

            if (channelAPI)
            {
                response.setChannelType(new QString());
                channelAPI->getIdentifier(*response.getChannelType());
                return channelAPI->webapiReportGet(response, *error.getMessage());
            }
            else
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
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

int WebAPIAdapterGUI::devicesetChannelActionsPost(
        int deviceSetIndex,
        int channelIndex,
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                QString channelType;
                channelAPI->getIdentifier(channelType);

                if (channelType == *query.getChannelType())
                {
                    int res = channelAPI->webapiActionsPost(channelActionsKeys, query, *error.getMessage());

                    if (res/100 == 2)
                    {
                        response.init();
                        *response.getMessage() = QString("Message to post action was submitted successfully");
                    }

                    return res;
                }
                else
                {
                    *error.getMessage() = QString("There is no channel type %1 at index %2. Found %3.")
                            .arg(*query.getChannelType())
                            .arg(channelIndex)
                            .arg(channelType);
                    return 404;
                }
            }
        }
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex);

            if (channelAPI == 0)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                QString channelType;
                channelAPI->getIdentifier(channelType);

                if (channelType == *query.getChannelType())
                {
                    int res = channelAPI->webapiActionsPost(channelActionsKeys, query, *error.getMessage());

                    if (res/100 == 2)
                    {
                        response.init();
                        *response.getMessage() = QString("Message to post action was submitted successfully");
                    }

                    return res;
                }
                else
                {
                    *error.getMessage() = QString("There is no channel type %1 at index %2. Found %3.")
                            .arg(*query.getChannelType())
                            .arg(channelIndex)
                            .arg(channelType);
                    return 404;
                }
            }
        }
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            int nbSinkChannels = deviceSet->m_deviceAPI->getNbSinkChannels();
            int nbSourceChannels = deviceSet->m_deviceAPI->getNbSourceChannels();
            int nbMIMOChannels = deviceSet->m_deviceAPI->getNbMIMOChannels();
            ChannelAPI *channelAPI = nullptr;

            if ((query.getDirection() == 0) && (channelIndex < nbSinkChannels))
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);
            }
            else if ((query.getDirection() == 1) && (channelIndex < nbSinkChannels + nbSourceChannels))
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex - nbSinkChannels);
            }
            else if ((query.getDirection() == 2) && (channelIndex < nbSinkChannels + nbSourceChannels + nbMIMOChannels))
            {
                channelAPI = deviceSet->m_deviceAPI->getMIMOChannelAPIAt(channelIndex - nbSinkChannels - nbSourceChannels);
            }
            else
            {
                *error.getMessage() = QString("here is no channel with index %1").arg(channelIndex);
                return 404;
            }

            if (channelAPI)
            {
                QString channelType;
                channelAPI->getIdentifier(channelType);

                if (channelType == *query.getChannelType())
                {
                    int res = channelAPI->webapiActionsPost(channelActionsKeys, query, *error.getMessage());
                    if (res/100 == 2)
                    {
                        response.init();
                        *response.getMessage() = QString("Message to post action was submitted successfully");
                    }

                    return res;
                }
                else
                {
                    *error.getMessage() = QString("There is no channel type %1 at index %2. Found %3.")
                            .arg(*query.getChannelType())
                            .arg(channelIndex)
                            .arg(channelType);
                    return 404;
                }
            }
            else
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
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

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);

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
        else if (deviceSet->m_deviceSinkEngine) // Single Tx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex);

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
        else if (deviceSet->m_deviceMIMOEngine) // MIMO
        {
            int nbSinkChannels = deviceSet->m_deviceAPI->getNbSinkChannels();
            int nbSourceChannels = deviceSet->m_deviceAPI->getNbSourceChannels();
            int nbMIMOChannels = deviceSet->m_deviceAPI->getNbMIMOChannels();
            ChannelAPI *channelAPI = nullptr;

            if (channelIndex < nbSinkChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);
                response.setDirection(0);
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex - nbSinkChannels);
                response.setDirection(1);
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels + nbMIMOChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getMIMOChannelAPIAt(channelIndex - nbSinkChannels - nbSourceChannels);
                response.setDirection(2);
            }
            else
            {
                *error.getMessage() = QString("here is no channel with index %1").arg(channelIndex);
                return 404;
            }

            if (channelAPI)
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
            else
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
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

    if (deviceUISet->m_deviceSinkEngine) // Single Tx data
    {
        samplingDevice->setDirection(1);
        *samplingDevice->getHwType() = deviceUISet->m_deviceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceUISet->m_deviceAPI->getSamplingDeviceSerial();
        samplingDevice->setSequence(deviceUISet->m_deviceAPI->getSamplingDeviceSequence());
        samplingDevice->setDeviceNbStreams(deviceUISet->m_deviceAPI->getDeviceNbItems());
        samplingDevice->setDeviceStreamIndex(deviceUISet->m_deviceAPI->getDeviceItemIndex());
        deviceUISet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSink *sampleSink = deviceUISet->m_deviceSinkEngine->getSink();

        if (sampleSink) {
            samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSink->getSampleRate());
        }

        deviceSet->setChannelcount(deviceUISet->m_deviceAPI->getNbSourceChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = deviceSet->getChannels();

        for (int i = 0; i <  deviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSourceAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(1);
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceUISet->m_deviceSourceEngine) // Rx data
    {
        samplingDevice->setDirection(0);
        *samplingDevice->getHwType() = deviceUISet->m_deviceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceUISet->m_deviceAPI->getSamplingDeviceSerial();
        samplingDevice->setSequence(deviceUISet->m_deviceAPI->getSamplingDeviceSequence());
        samplingDevice->setDeviceNbStreams(deviceUISet->m_deviceAPI->getDeviceNbItems());
        samplingDevice->setDeviceStreamIndex(deviceUISet->m_deviceAPI->getDeviceItemIndex());
        deviceUISet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSource *sampleSource = deviceUISet->m_deviceSourceEngine->getSource();

        if (sampleSource) {
            samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSource->getSampleRate());
        }

        deviceSet->setChannelcount(deviceUISet->m_deviceAPI->getNbSinkChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = deviceSet->getChannels();

        for (int i = 0; i <  deviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSinkAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(0);
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceUISet->m_deviceMIMOEngine) // MIMO data
    {
        samplingDevice->setDirection(2);
        *samplingDevice->getHwType() = deviceUISet->m_deviceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceUISet->m_deviceAPI->getSamplingDeviceSerial();
        samplingDevice->setSequence(deviceUISet->m_deviceAPI->getSamplingDeviceSequence());
        samplingDevice->setDeviceNbStreams(deviceUISet->m_deviceAPI->getDeviceNbItems());
        samplingDevice->setDeviceStreamIndex(deviceUISet->m_deviceAPI->getDeviceItemIndex());
        samplingDevice->setState(new QString("notStarted"));
        deviceUISet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getStateRx(), 0);
        deviceUISet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getStateTx(), 1);
        DeviceSampleMIMO *sampleMIMO = deviceUISet->m_deviceMIMOEngine->getMIMO();

        if (sampleMIMO)
        {
            samplingDevice->setCenterFrequency(sampleMIMO->getMIMOCenterFrequency());
            samplingDevice->setBandwidth(sampleMIMO->getMIMOSampleRate());
        }

        int nbSinkChannels = deviceUISet->m_deviceAPI->getNbSinkChannels();
        int nbSourceChannels = deviceUISet->m_deviceAPI->getNbSourceChannels();
        int nbMIMOChannels = deviceUISet->m_deviceAPI->getNbMIMOChannels();
        deviceSet->setChannelcount(nbSinkChannels + nbSourceChannels + nbMIMOChannels);
        QList<SWGSDRangel::SWGChannel*> *channels = deviceSet->getChannels();

        for (int i = 0; i < nbSinkChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSinkAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(0);
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }

        for (int i = 0; i < nbSourceChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSourceAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(1);
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }

        for (int i = 0; i < nbMIMOChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getMIMOChannelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(2);
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
        channelsDetail->setChannelcount(deviceUISet->m_deviceAPI->getNbSourceChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();

        for (int i = 0; i <  channelsDetail->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSourceAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(1);
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
        channelsDetail->setChannelcount(deviceUISet->m_deviceAPI->getNbSinkChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();

        for (int i = 0; i <  channelsDetail->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSinkAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(0);
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

    if (deviceUISet->m_deviceMIMOEngine) // MIMO data
    {
        int nbSinkChannels = deviceUISet->m_deviceAPI->getNbSinkChannels();
        int nbSourceChannels = deviceUISet->m_deviceAPI->getNbSourceChannels();
        int nbMIMOChannels = deviceUISet->m_deviceAPI->getNbMIMOChannels();
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();
        channelsDetail->setChannelcount(nbSinkChannels + nbSourceChannels + nbMIMOChannels);

        for (int i = 0; i < nbSinkChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSinkAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(0);
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

        for (int i = 0; i <  nbSourceChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getChanelSourceAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(1);
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

        for (int i = 0; i <  nbMIMOChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceUISet->m_deviceAPI->getMIMOChannelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(2);
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
