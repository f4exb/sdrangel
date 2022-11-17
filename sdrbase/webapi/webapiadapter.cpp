///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2020 Edouard Griffiths, F4EXB.                             //
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

#include <QCoreApplication>
#include <QList>
#include <QSysInfo>
#include <QTextStream>

#include "maincore.h"
#include "loggerwithfile.h"
#include "audio/audiodeviceinfo.h"
#include "device/deviceapi.h"
#include "device/deviceset.h"
#include "device/deviceenumerator.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "channel/channelapi.h"
#include "webapi/webapiadapterbase.h"
#include "util/serialutil.h"

#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceConfigResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGInstanceFeaturesResponse.h"
#include "SWGDeviceListItem.h"
#include "SWGAudioDevices.h"
#include "SWGLocationInformation.h"
#include "SWGPresets.h"
#include "SWGPresetGroup.h"
#include "SWGPresetItem.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGPresetExport.h"
#include "SWGConfigurations.h"
#include "SWGConfigurationIdentifier.h"
#include "SWGConfigurationImportExport.h"
#include "SWGBase64Blob.h"
#include "SWGFilePath.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelsDetail.h"
#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGDeviceState.h"
#include "SWGLimeRFEDevices.h"
#include "SWGLimeRFESettings.h"
#include "SWGFeaturePresets.h"
#include "SWGFeaturePresetGroup.h"
#include "SWGFeaturePresetItem.h"
#include "SWGFeaturePresetIdentifier.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"

#include "webapiadapter.h"

WebAPIAdapter::WebAPIAdapter()
{
    m_mainCore = MainCore::instance();
}

WebAPIAdapter::~WebAPIAdapter()
{
}

int WebAPIAdapter::instanceSummary(
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    *response.getArchitecture() = QString(QSysInfo::currentCpuArchitecture());
    *response.getOs() = QString(QSysInfo::prettyProductName());
#endif

    SWGSDRangel::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_mainCore->m_logger->getUseFileLogger() ? 1 : 0);

    if (logging->getDumpToFile()) {
        m_mainCore->m_logger->getLogFileName(*logging->getFileName());
        m_mainCore->m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }

    m_mainCore->m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    SWGSDRangel::SWGDeviceSetList *deviceSetList = response.getDevicesetlist();
    getDeviceSetList(deviceSetList);

    SWGSDRangel::SWGFeatureSet *featureSet = response.getFeatureset();
    getFeatureSet(featureSet, m_mainCore->m_featureSets.back());

    return 200;
}

int WebAPIAdapter::instanceDelete(
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) response;
    *error.getMessage() = QString("Not supported in GUI instance");
    return 400;
}

int WebAPIAdapter::instanceConfigGet(
        SWGSDRangel::SWGInstanceConfigResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();
    WebAPIAdapterBase webAPIAdapterBase;
    webAPIAdapterBase.setPluginManager(m_mainCore->getPluginManager());
    SWGSDRangel::SWGPreferences *preferences = response.getPreferences();
    WebAPIAdapterBase::webapiFormatPreferences(preferences, m_mainCore->m_settings.getPreferences());
    SWGSDRangel::SWGPreset *workingPreset = response.getWorkingPreset();
    webAPIAdapterBase.webapiFormatPreset(workingPreset, m_mainCore->m_settings.getWorkingPresetConst());
    SWGSDRangel::SWGFeatureSetPreset *workingFeatureSetPreset = response.getWorkingFeatureSetPreset();
    webAPIAdapterBase.webapiFormatFeatureSetPreset(workingFeatureSetPreset, m_mainCore->m_settings.getWorkingFeatureSetPresetConst());

    int nbPresets = m_mainCore->m_settings.getPresetCount();
    QList<SWGSDRangel::SWGPreset*> *swgPresets = response.getPresets();

    for (int i = 0; i < nbPresets; i++)
    {
        const Preset *preset = m_mainCore->m_settings.getPreset(i);
        swgPresets->append(new SWGSDRangel::SWGPreset);
        webAPIAdapterBase.webapiFormatPreset(swgPresets->back(), *preset);
    }

    int nbCommands = m_mainCore->m_settings.getCommandCount();
    QList<SWGSDRangel::SWGCommand*> *swgCommands = response.getCommands();

    for (int i = 0; i < nbCommands; i++)
    {
        const Command *command = m_mainCore->m_settings.getCommand(i);
        swgCommands->append(new SWGSDRangel::SWGCommand);
        WebAPIAdapterBase::webapiFormatCommand(swgCommands->back(), *command);
    }

    int nbFeatureSetPresets = m_mainCore->m_settings.getFeatureSetPresetCount();
    QList<SWGSDRangel::SWGFeatureSetPreset*> *swgFeatureSetPresets = response.getFeaturesetpresets();

    for (int i = 0; i < nbFeatureSetPresets; i++)
    {
        const FeatureSetPreset *preset = m_mainCore->m_settings.getFeatureSetPreset(i);
        swgFeatureSetPresets->append(new SWGSDRangel::SWGFeatureSetPreset);
        webAPIAdapterBase.webapiFormatFeatureSetPreset(swgFeatureSetPresets->back(), *preset);
    }

    return 200;
}

int WebAPIAdapter::instanceConfigPutPatch(
        bool force, // PUT else PATCH
        SWGSDRangel::SWGInstanceConfigResponse& query,
        const ConfigKeys& configKeys,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) response;
    (void) error;
    WebAPIAdapterBase webAPIAdapterBase;
    webAPIAdapterBase.setPluginManager(m_mainCore->getPluginManager());

    if (force) {
        webAPIAdapterBase.webapiInitConfig(m_mainCore->m_settings);
    }

    Preferences newPreferences = m_mainCore->m_settings.getPreferences();
    webAPIAdapterBase.webapiUpdatePreferences(query.getPreferences(), configKeys.m_preferencesKeys, newPreferences);
    m_mainCore->m_settings.setPreferences(newPreferences);

    Preset *workingPreset = m_mainCore->m_settings.getWorkingPreset();
    webAPIAdapterBase.webapiUpdatePreset(force, query.getWorkingPreset(), configKeys.m_workingPresetKeys, workingPreset);

    FeatureSetPreset *workingFeatureSetPreset = m_mainCore->m_settings.getWorkingFeatureSetPreset();
    webAPIAdapterBase.webapiUpdateFeatureSetPreset(force, query.getWorkingFeatureSetPreset(), configKeys.m_workingFeatureSetPresetKeys, workingFeatureSetPreset);

    QList<PresetKeys>::const_iterator presetKeysIt = configKeys.m_presetKeys.begin();
    int i = 0;
    for (; presetKeysIt != configKeys.m_presetKeys.end(); ++presetKeysIt, i++)
    {
        Preset *newPreset = new Preset(); // created with default values
        SWGSDRangel::SWGPreset *swgPreset = query.getPresets()->at(i);
        webAPIAdapterBase.webapiUpdatePreset(force, swgPreset, *presetKeysIt, newPreset);
        m_mainCore->m_settings.addPreset(newPreset);
    }

    QList<CommandKeys>::const_iterator commandKeysIt = configKeys.m_commandKeys.begin();
    i = 0;
    for (; commandKeysIt != configKeys.m_commandKeys.end(); ++commandKeysIt, i++)
    {
        Command *newCommand = new Command(); // created with default values
        SWGSDRangel::SWGCommand *swgCommand = query.getCommands()->at(i);
        webAPIAdapterBase.webapiUpdateCommand(swgCommand, *commandKeysIt, *newCommand);
        m_mainCore->m_settings.addCommand(newCommand);
    }

    QList<FeatureSetPresetKeys>::const_iterator featureSetPresetKeysIt = configKeys.m_featureSetPresetKeys.begin();
    i = 0;
    for (; featureSetPresetKeysIt != configKeys.m_featureSetPresetKeys.end(); ++featureSetPresetKeysIt, i++)
    {
        FeatureSetPreset *newPreset = new FeatureSetPreset(); // created with default values
        SWGSDRangel::SWGFeatureSetPreset *swgPreset = query.getFeaturesetpresets()->at(i);
        webAPIAdapterBase.webapiUpdateFeatureSetPreset(force, swgPreset, *featureSetPresetKeysIt, newPreset);
        m_mainCore->m_settings.addFeatureSetPreset(newPreset);
    }

    MainCore::MsgApplySettings *msg = MainCore::MsgApplySettings::create();
    m_mainCore->m_mainMessageQueue->push(msg);

    return 200;
}

int WebAPIAdapter::instanceDevices(
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

int WebAPIAdapter::instanceChannels(
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
        channelRegistrations = m_mainCore->m_pluginManager->getRxChannelRegistrations();
        nbChannelDevices = channelRegistrations->size();
    }
    else if (direction == 1) // Single source (Tx) channel
    {
        channelRegistrations = m_mainCore->m_pluginManager->getTxChannelRegistrations();
        nbChannelDevices = channelRegistrations->size();
    }
    else if (direction == 2) // MIMO channel
    {
        channelRegistrations = m_mainCore->m_pluginManager->getMIMOChannelRegistrations();
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

int WebAPIAdapter::instanceFeatures(
            SWGSDRangel::SWGInstanceFeaturesResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();
    PluginAPI::FeatureRegistrations *featureRegistrations;
    int nbFeatureDevices;

    featureRegistrations = m_mainCore->m_pluginManager->getFeatureRegistrations();
    nbFeatureDevices = featureRegistrations->size();

    response.setFeaturecount(nbFeatureDevices);
    QList<SWGSDRangel::SWGFeatureListItem*> *features = response.getFeatures();

    for (int i = 0; i < nbFeatureDevices; i++)
    {
        features->append(new SWGSDRangel::SWGFeatureListItem);
        features->back()->init();
        PluginInterface *featureInterface = featureRegistrations->at(i).m_plugin;
        const PluginDescriptor& pluginDescriptor = featureInterface->getPluginDescriptor();
        *features->back()->getVersion() = pluginDescriptor.version;
        *features->back()->getName() = pluginDescriptor.displayedName;
        *features->back()->getIdUri() = featureRegistrations->at(i).m_featureIdURI;
        *features->back()->getId() = featureRegistrations->at(i).m_featureId;
        features->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapter::instanceLoggingGet(
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();
    response.setDumpToFile(m_mainCore->m_logger->getUseFileLogger() ? 1 : 0);

    if (response.getDumpToFile()) {
        m_mainCore->m_logger->getLogFileName(*response.getFileName());
        m_mainCore->m_logger->getFileMinMessageLevelStr(*response.getFileLevel());
    }

    m_mainCore->m_logger->getConsoleMinMessageLevelStr(*response.getConsoleLevel());

    return 200;
}

int WebAPIAdapter::instanceLoggingPut(
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
        m_mainCore->m_settings.setConsoleMinLogLevel(getMsgTypeFromString(*consoleLevel));
    }

    if (fileLevel) {
        m_mainCore->m_settings.setFileMinLogLevel(getMsgTypeFromString(*fileLevel));
    }

    m_mainCore->m_settings.setUseLogFile(dumpToFile);

    if (fileName) {
        m_mainCore->m_settings.setLogFileName(*fileName);
    }

    m_mainCore->setLoggingOptions();

    // build response
    response.init();
    getMsgTypeString(m_mainCore->m_settings.getConsoleMinLogLevel(), *response.getConsoleLevel());
    response.setDumpToFile(m_mainCore->m_settings.getUseLogFile() ? 1 : 0);
    getMsgTypeString(m_mainCore->m_settings.getFileMinLogLevel(), *response.getFileLevel());
    *response.getFileName() = m_mainCore->m_settings.getLogFileName();

    return 200;
}

int WebAPIAdapter::instanceAudioGet(
        SWGSDRangel::SWGAudioDevices& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    DSPEngine *dspEngine = DSPEngine::instance();
    const QList<AudioDeviceInfo>& audioInputDevices = dspEngine->getAudioDeviceManager()->getInputDevices();
    const QList<AudioDeviceInfo>& audioOutputDevices = dspEngine->getAudioDeviceManager()->getOutputDevices();
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
    bool found = dspEngine->getAudioDeviceManager()->getInputDeviceInfo(AudioDeviceManager::m_defaultDeviceName, inputDeviceInfo);
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
        found = dspEngine->getAudioDeviceManager()->getInputDeviceInfo(audioInputDevices.at(i).deviceName(), inputDeviceInfo);
        *inputDevices->back()->getName() = audioInputDevices.at(i).deviceName();
        inputDevices->back()->setIndex(i);
        inputDevices->back()->setSampleRate(inputDeviceInfo.sampleRate);
        inputDevices->back()->setIsSystemDefault(audioInputDevices.at(i).deviceName() == AudioDeviceInfo::defaultInputDevice().deviceName() ? 1 : 0);
        inputDevices->back()->setDefaultUnregistered(found ? 0 : 1);
        inputDevices->back()->setVolume(inputDeviceInfo.volume);
    }

    // system default output device
    outputDevices->append(new SWGSDRangel::SWGAudioOutputDevice);
    outputDevices->back()->init();
    found = dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(AudioDeviceManager::m_defaultDeviceName, outputDeviceInfo);
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
    *outputDevices->back()->getFileRecordName() = outputDeviceInfo.fileRecordName;
    outputDevices->back()->setRecordToFile(outputDeviceInfo.recordToFile ? 1 : 0);
    outputDevices->back()->setRecordSilenceTime(outputDeviceInfo.recordSilenceTime);

    // real output devices
    for (int i = 0; i < nbOutputDevices; i++)
    {
        outputDevices->append(new SWGSDRangel::SWGAudioOutputDevice);
        outputDevices->back()->init();
        outputDeviceInfo.resetToDefaults();
        found = dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(audioOutputDevices.at(i).deviceName(), outputDeviceInfo);
        *outputDevices->back()->getName() = audioOutputDevices.at(i).deviceName();
        outputDevices->back()->setIndex(i);
        outputDevices->back()->setSampleRate(outputDeviceInfo.sampleRate);
        outputDevices->back()->setIsSystemDefault(audioOutputDevices.at(i).deviceName() == AudioDeviceInfo::defaultOutputDevice().deviceName() ? 1 : 0);
        outputDevices->back()->setDefaultUnregistered(found ? 0 : 1);
        outputDevices->back()->setCopyToUdp(outputDeviceInfo.copyToUDP ? 1 : 0);
        outputDevices->back()->setUdpUsesRtp(outputDeviceInfo.udpUseRTP ? 1 : 0);
        outputDevices->back()->setUdpChannelMode((int) outputDeviceInfo.udpChannelMode);
        outputDevices->back()->setUdpChannelCodec((int) outputDeviceInfo.udpChannelCodec);
        outputDevices->back()->setUdpDecimationFactor((int) outputDeviceInfo.udpDecimationFactor);
        *outputDevices->back()->getUdpAddress() = outputDeviceInfo.udpAddress;
        outputDevices->back()->setUdpPort(outputDeviceInfo.udpPort);
        *outputDevices->back()->getFileRecordName() = outputDeviceInfo.fileRecordName;
        outputDevices->back()->setRecordToFile(outputDeviceInfo.recordToFile ? 1 : 0);
        outputDevices->back()->setRecordSilenceTime(outputDeviceInfo.recordSilenceTime);
    }

    return 200;
}

int WebAPIAdapter::instanceAudioInputPatch(
        SWGSDRangel::SWGAudioInputDevice& response,
        const QStringList& audioInputKeys,
        SWGSDRangel::SWGErrorResponse& error)
{
    DSPEngine *dspEngine = DSPEngine::instance();
    AudioDeviceManager::InputDeviceInfo inputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!dspEngine->getAudioDeviceManager()->getInputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio input device at index %1").arg(deviceIndex);
        return 404;
    }

    dspEngine->getAudioDeviceManager()->getInputDeviceInfo(deviceName, inputDeviceInfo);

    if (audioInputKeys.contains("sampleRate")) {
        inputDeviceInfo.sampleRate = response.getSampleRate();
    }
    if (audioInputKeys.contains("volume")) {
        inputDeviceInfo.volume = response.getVolume();
    }

    dspEngine->getAudioDeviceManager()->setInputDeviceInfo(deviceIndex, inputDeviceInfo);
    dspEngine->getAudioDeviceManager()->getInputDeviceInfo(deviceName, inputDeviceInfo);

    response.setSampleRate(inputDeviceInfo.sampleRate);
    response.setVolume(inputDeviceInfo.volume);

    return 200;
}

int WebAPIAdapter::instanceAudioOutputPatch(
        SWGSDRangel::SWGAudioOutputDevice& response,
        const QStringList& audioOutputKeys,
        SWGSDRangel::SWGErrorResponse& error)
{
    DSPEngine *dspEngine = DSPEngine::instance();
    AudioDeviceManager::OutputDeviceInfo outputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!dspEngine->getAudioDeviceManager()->getOutputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio output device at index %1").arg(deviceIndex);
        return 404;
    }

    dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(deviceName, outputDeviceInfo);

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
        outputDeviceInfo.udpChannelMode = static_cast<AudioOutputDevice::UDPChannelMode>(response.getUdpChannelMode());
    }
    if (audioOutputKeys.contains("udpChannelCodec")) {
        outputDeviceInfo.udpChannelCodec = static_cast<AudioOutputDevice::UDPChannelCodec>(response.getUdpChannelCodec());
    }
    if (audioOutputKeys.contains("udpDecimationFactor")) {
        outputDeviceInfo.udpDecimationFactor = response.getUdpDecimationFactor();
    }
    if (audioOutputKeys.contains("udpAddress")) {
        outputDeviceInfo.udpAddress = *response.getUdpAddress();
    }
    if (audioOutputKeys.contains("udpPort")) {
        outputDeviceInfo.udpPort = response.getUdpPort() % (1<<16);
    }
    if (audioOutputKeys.contains("fileRecordName")) {
        outputDeviceInfo.fileRecordName = *response.getFileRecordName();
    }
    if (audioOutputKeys.contains("recordToFile")) {
        outputDeviceInfo.recordToFile = response.getRecordToFile() == 0 ? 0 : 1;
    }
    if (audioOutputKeys.contains("recordSilenceTime")) {
        outputDeviceInfo.recordSilenceTime = response.getRecordSilenceTime();
    }

    dspEngine->getAudioDeviceManager()->setOutputDeviceInfo(deviceIndex, outputDeviceInfo);
    dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(deviceName, outputDeviceInfo);

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

    if (response.getFileRecordName()) {
        *response.getFileRecordName() = outputDeviceInfo.fileRecordName;
    } else {
        response.setFileRecordName(new QString(outputDeviceInfo.fileRecordName));
    }

    response.setRecordToFile(outputDeviceInfo.recordToFile == 0 ? 0 : 1);
    response.setRecordSilenceTime(outputDeviceInfo.recordSilenceTime);

    return 200;
}

int WebAPIAdapter::instanceAudioInputDelete(
            SWGSDRangel::SWGAudioInputDevice& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    DSPEngine *dspEngine = DSPEngine::instance();
    AudioDeviceManager::InputDeviceInfo inputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!dspEngine->getAudioDeviceManager()->getInputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio input device at index %1").arg(deviceIndex);
        return 404;
    }

    dspEngine->getAudioDeviceManager()->unsetInputDeviceInfo(deviceIndex);
    dspEngine->getAudioDeviceManager()->getInputDeviceInfo(deviceName, inputDeviceInfo);

    response.setSampleRate(inputDeviceInfo.sampleRate);
    response.setVolume(inputDeviceInfo.volume);

    return 200;
}

int WebAPIAdapter::instanceAudioOutputDelete(
            SWGSDRangel::SWGAudioOutputDevice& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    DSPEngine *dspEngine = DSPEngine::instance();
    AudioDeviceManager::OutputDeviceInfo outputDeviceInfo;
    QString deviceName;
    int deviceIndex = response.getIndex();

    if (!dspEngine->getAudioDeviceManager()->getOutputDeviceName(deviceIndex, deviceName))
    {
        error.init();
        *error.getMessage() = QString("There is no audio output device at index %1").arg(deviceIndex);
        return 404;
    }

    dspEngine->getAudioDeviceManager()->unsetInputDeviceInfo(deviceIndex);
    dspEngine->getAudioDeviceManager()->getOutputDeviceInfo(deviceName, outputDeviceInfo);

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

int WebAPIAdapter::instanceAudioInputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    DSPEngine *dspEngine = DSPEngine::instance();
    dspEngine->getAudioDeviceManager()->inputInfosCleanup();

    response.init();
    *response.getMessage() = QString("Unregistered parameters for devices not in list of available input devices for this instance");

    return 200;
}

int WebAPIAdapter::instanceAudioOutputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    DSPEngine *dspEngine = DSPEngine::instance();
    dspEngine->getAudioDeviceManager()->outputInfosCleanup();

    response.init();
    *response.getMessage() = QString("Unregistered parameters for devices not in list of available output devices for this instance");

    return 200;
}

int WebAPIAdapter::instanceLocationGet(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    response.init();
    response.setLatitude(m_mainCore->m_settings.getLatitude());
    response.setLongitude(m_mainCore->m_settings.getLongitude());

    return 200;
}

int WebAPIAdapter::instanceLocationPut(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    float latitude = response.getLatitude();
    float longitude = response.getLongitude();

    latitude = latitude < -90.0 ? -90.0 : latitude > 90.0 ? 90.0 : latitude;
    longitude = longitude < -180.0 ? -180.0 : longitude > 180.0 ? 180.0 : longitude;

    m_mainCore->m_settings.setLatitude(latitude);
    m_mainCore->m_settings.setLongitude(longitude);

    response.setLatitude(m_mainCore->m_settings.getLatitude());
    response.setLongitude(m_mainCore->m_settings.getLongitude());

    return 200;
}

int WebAPIAdapter::instancePresetsGet(
        SWGSDRangel::SWGPresets& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    int nbPresets = m_mainCore->m_settings.getPresetCount();
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
        const Preset *preset = m_mainCore->m_settings.getPreset(i);

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
        *swgPresets->back()->getType() = Preset::getPresetTypeChar(preset->getPresetType());
        *swgPresets->back()->getName() = preset->getDescription();
        nbPresetsThisGroup++;
    }

    if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
    response.setNbGroups(nbGroups);

    return 200;
}

int WebAPIAdapter::instancePresetPatch(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainCore->m_deviceSets.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        error.init();
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainCore->m_settings.getPreset(*presetIdentifier->getGroupName(),
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

    DeviceSet *deviceUI = m_mainCore->m_deviceSets[deviceSetIndex];

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

    MainCore::MsgLoadPreset *msg = MainCore::MsgLoadPreset::create(selectedPreset, deviceSetIndex);
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = Preset::getPresetTypeChar(selectedPreset->getPresetType());
    *response.getName() = selectedPreset->getDescription();

    return 202;
}

int WebAPIAdapter::instancePresetPut(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainCore->m_deviceSets.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        error.init();
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainCore->m_settings.getPreset(*presetIdentifier->getGroupName(),
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
        DeviceSet *deviceUI = m_mainCore->m_deviceSets[deviceSetIndex];

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

    MainCore::MsgSavePreset *msg = MainCore::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, false);
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = Preset::getPresetTypeChar(selectedPreset->getPresetType());
    *response.getName() = selectedPreset->getDescription();

    return 202;
}

int WebAPIAdapter::instancePresetPost(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainCore->m_deviceSets.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        error.init();
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
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

    const Preset *selectedPreset = m_mainCore->m_settings.getPreset(*presetIdentifier->getGroupName(),
            deviceCenterFrequency,
            *presetIdentifier->getName(),
            *presetIdentifier->getType());

    if (selectedPreset == 0) // save on a new preset
    {
        selectedPreset = m_mainCore->m_settings.newPreset(*presetIdentifier->getGroupName(), *presetIdentifier->getName());
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

    MainCore::MsgSavePreset *msg = MainCore::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, true);
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = Preset::getPresetTypeChar(selectedPreset->getPresetType());
    *response.getName() = selectedPreset->getDescription();

    return 202;
}

int WebAPIAdapter::instancePresetDelete(
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const Preset *selectedPreset = m_mainCore->m_settings.getPreset(*response.getGroupName(),
            response.getCenterFrequency(),
            *response.getName(),
            *response.getType());

    if (selectedPreset == 0)
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2, %3 %4]")
                .arg(*response.getGroupName())
                .arg(response.getCenterFrequency())
                .arg(*response.getName())
                .arg(*response.getType());
        return 404;
    }

    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = Preset::getPresetTypeChar(selectedPreset->getPresetType());
    *response.getName() = selectedPreset->getDescription();

    MainCore::MsgDeletePreset *msg = MainCore::MsgDeletePreset::create(const_cast<Preset*>(selectedPreset));
    m_mainCore->m_mainMessageQueue->push(msg);

    return 202;
}

int WebAPIAdapter::instancePresetFilePut(
        SWGSDRangel::SWGFilePath& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    QString filePath = *query.getFilePath();

    if (QFileInfo::exists(filePath))
    {
        QFile file(filePath);

        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QByteArray base64Str;
            QTextStream instream(&file);
            instream >> base64Str;
            file.close();
            Preset *newPreset = m_mainCore->m_settings.newPreset("TBD", "TBD");

            if (newPreset->deserialize(QByteArray::fromBase64(base64Str)))
            {
                response.init();
                *response.getGroupName() = newPreset->getGroup();
                response.setCenterFrequency(newPreset->getCenterFrequency());
                *response.getName() = newPreset->getDescription();
                *response.getType() = Preset::getPresetTypeChar(newPreset->getPresetType());
                return 202;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("Cannot deserialize preset from file %1").arg(filePath);
                return 400;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("Cannot read file %1").arg(filePath);
            return 500;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("File %1 is not found").arg(filePath);
        return 404;
    }
}

int WebAPIAdapter::instancePresetFilePost(
        SWGSDRangel::SWGPresetExport& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    QString filePath = *query.getFilePath();

    if (QFileInfo(filePath).absoluteDir().exists())
    {
        SWGSDRangel::SWGPresetIdentifier *presetId = query.getPreset();
        const Preset *selectedPreset = m_mainCore->m_settings.getPreset(
            *presetId->getGroupName(),
            presetId->getCenterFrequency(),
            *presetId->getName(),
            *presetId->getType());

        if (selectedPreset)
        {
            QString base64Str = selectedPreset->serialize().toBase64();
            QFileInfo fileInfo(filePath);

            if (fileInfo.suffix() != "prex") {
                filePath += ".prex";
            }

            QFile file(filePath);

            if (file.open(QIODevice::ReadWrite | QIODevice::Text))
            {
                QTextStream outstream(&file);
                outstream << base64Str;
                file.close();
                response.init();
                *response.getGroupName() = selectedPreset->getGroup();
                response.setCenterFrequency(selectedPreset->getCenterFrequency());
                *response.getName() = selectedPreset->getDescription();
                *response.getType() = Preset::getPresetTypeChar(selectedPreset->getPresetType());
                return 200;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("Cannot open %1 for writing").arg(filePath);
                return 500;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no preset [%1, %2, %3, %4]")
                .arg(*presetId->getGroupName())
                .arg(presetId->getCenterFrequency())
                .arg(*presetId->getName())
                .arg(*presetId->getType());
            return 404;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("File %1 directory does not exist").arg(filePath);
        return 404;
    }
}

int WebAPIAdapter::instancePresetBlobPut(
        SWGSDRangel::SWGBase64Blob& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    QString *base64Str = query.getBlob();

    if (base64Str)
    {
        Preset *newPreset = m_mainCore->m_settings.newPreset("TBD", "TBD");

        if (newPreset)
        {
            QByteArray blob = QByteArray::fromBase64(base64Str->toUtf8());

            if (newPreset->deserialize(blob))
            {
                response.init();
                *response.getGroupName() = newPreset->getGroup();
                response.setCenterFrequency(newPreset->getCenterFrequency());
                *response.getName() = newPreset->getDescription();
                *response.getType() = Preset::getPresetTypeChar(newPreset->getPresetType());
                return 202;
            }
            else
            {
                m_mainCore->m_settings.deletePreset(newPreset);
                error.init();
                *error.getMessage() = QString("Could not deserialize blob to preset");
                return 400;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("Cannot create new preset");
            return 500;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("Blob not specified");
        return 400;
    }
}

int WebAPIAdapter::instancePresetBlobPost(
        SWGSDRangel::SWGPresetIdentifier& query,
        SWGSDRangel::SWGBase64Blob& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const Preset *selectedPreset = m_mainCore->m_settings.getPreset(
        *query.getGroupName(),
        query.getCenterFrequency(),
        *query.getName(),
        *query.getType());

    if (selectedPreset)
    {
        QString base64Str = selectedPreset->serialize().toBase64();
        response.init();
        *response.getBlob() = base64Str;
        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2, %3, %4]")
            .arg(*query.getGroupName())
            .arg(query.getCenterFrequency())
            .arg(*query.getName())
            .arg(*query.getType());
        return 404;
    }
}

int WebAPIAdapter::instanceConfigurationsGet(
        SWGSDRangel::SWGConfigurations& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    int nbConfigurations = m_mainCore->m_settings.getConfigurationCount();
    int nbGroups = 0;
    int nbConfigurationsThisGroup = 0;
    QString groupName;
    response.init();
    QList<SWGSDRangel::SWGConfigurationGroup*> *groups = response.getGroups();
    QList<SWGSDRangel::SWGConfigurationItem*> *swgConfigurations = nullptr;
    int i = 0;

    // Configurations are sorted by group first

    for (; i < nbConfigurations; i++)
    {
        const Configuration *configuration = m_mainCore->m_settings.getConfiguration(i);

        if ((i == 0) || (groupName != configuration->getGroup())) // new group
        {
            if (i > 0) {
                groups->back()->setNbConfigurations(nbConfigurationsThisGroup);
            }

            groups->append(new SWGSDRangel::SWGConfigurationGroup);
            groups->back()->init();
            groupName = configuration->getGroup();
            *groups->back()->getGroupName() = groupName;
            swgConfigurations = groups->back()->getConfigurations();
            nbGroups++;
            nbConfigurationsThisGroup = 0;
        }

        swgConfigurations->append(new SWGSDRangel::SWGConfigurationItem);
        swgConfigurations->back()->init();
        *swgConfigurations->back()->getName() = configuration->getDescription();
        nbConfigurationsThisGroup++;
    }

    if (i > 0) {
        groups->back()->setNbConfigurations(nbConfigurationsThisGroup);
    }

    response.setNbGroups(nbGroups);

    return 200;
}

int WebAPIAdapter::instanceConfigurationPatch(
        SWGSDRangel::SWGConfigurationIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    SWGSDRangel::SWGConfigurationIdentifier *configurationIdentifier = &response;

    const Configuration *selectedConfiguration = m_mainCore->m_settings.getConfiguration(
        *configurationIdentifier->getGroupName(),
        *configurationIdentifier->getName()
    );

    if (selectedConfiguration == nullptr)
    {
        error.init();
        *error.getMessage() = QString("There is no configuration [%1, %2]")
                .arg(*configurationIdentifier->getGroupName())
                .arg(*configurationIdentifier->getName());
        return 404;
    }

    MainCore::MsgLoadConfiguration *msg = MainCore::MsgLoadConfiguration::create(selectedConfiguration);
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    *response.getGroupName() = selectedConfiguration->getGroup();
    *response.getName() = selectedConfiguration->getDescription();

    return 202;
}

int WebAPIAdapter::instanceConfigurationPut(
        SWGSDRangel::SWGConfigurationIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    SWGSDRangel::SWGConfigurationIdentifier *configurationIdentifier = &response;

    const Configuration *selectedConfiguration = m_mainCore->m_settings.getConfiguration(
        *configurationIdentifier->getGroupName(),
        *configurationIdentifier->getName()
    );

    if (selectedConfiguration == nullptr)
    {
        error.init();
        *error.getMessage() = QString("There is no configuration [%1, %2]")
                .arg(*configurationIdentifier->getGroupName())
                .arg(*configurationIdentifier->getName());
        return 404;
    }

    MainCore::MsgSaveConfiguration *msg = MainCore::MsgSaveConfiguration::create(const_cast<Configuration*>(selectedConfiguration), false);
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    *response.getGroupName() = selectedConfiguration->getGroup();
    *response.getName() = selectedConfiguration->getDescription();

    return 202;
}

int WebAPIAdapter::instanceConfigurationPost(
        SWGSDRangel::SWGConfigurationIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    SWGSDRangel::SWGConfigurationIdentifier *configurationIdentifier = &response;

    const Configuration *selectedConfiguration = m_mainCore->m_settings.getConfiguration(
        *configurationIdentifier->getGroupName(),
        *configurationIdentifier->getName()
    );

    if (selectedConfiguration == nullptr) // save on a new preset
    {
        selectedConfiguration = m_mainCore->m_settings.newConfiguration(
            *configurationIdentifier->getGroupName(),
            *configurationIdentifier->getName()
        );
    }
    else
    {
        error.init();
        *error.getMessage() = QString("Preset already exists [%1, %2]")
            .arg(*configurationIdentifier->getGroupName())
            .arg(*configurationIdentifier->getName());
        return 409;
    }

    MainCore::MsgSaveConfiguration *msg = MainCore::MsgSaveConfiguration::create(const_cast<Configuration*>(selectedConfiguration), true);
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    *response.getGroupName() = selectedConfiguration->getGroup();
    *response.getName() = selectedConfiguration->getDescription();

    return 202;
}

int WebAPIAdapter::instanceConfigurationDelete(
        SWGSDRangel::SWGConfigurationIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const Configuration *selectedConfiguration = m_mainCore->m_settings.getConfiguration(
        *response.getGroupName(),
        *response.getName());

    if (selectedConfiguration == nullptr)
    {
        error.init();
        *error.getMessage() = QString("There is no configuration [%1, %2]")
            .arg(*response.getGroupName())
            .arg(*response.getName());
        return 404;
    }

    *response.getGroupName() = selectedConfiguration->getGroup();
    *response.getName() = selectedConfiguration->getDescription();

    MainCore::MsgDeleteConfiguration *msg = MainCore::MsgDeleteConfiguration::create(const_cast<Configuration*>(selectedConfiguration));
    m_mainCore->m_mainMessageQueue->push(msg);

    return 202;
}

int WebAPIAdapter::instanceConfigurationFilePut(
        SWGSDRangel::SWGFilePath& query,
        SWGSDRangel::SWGConfigurationIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    QString filePath = *query.getFilePath();

    if (QFileInfo::exists(filePath))
    {
        QFile file(filePath);

        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QByteArray base64Str;
            QTextStream instream(&file);
            instream >> base64Str;
            file.close();
            Configuration *newConfiguration = m_mainCore->m_settings.newConfiguration("TBD", "TBD");

            if (newConfiguration->deserialize(QByteArray::fromBase64(base64Str)))
            {
                response.init();
                *response.getGroupName() = newConfiguration->getGroup();
                *response.getName() = newConfiguration->getDescription();
                return 202;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("Cannot deserialize configuration from file %1").arg(filePath);
                return 400;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("Cannot read file %1").arg(filePath);
            return 500;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("File %1 is not found").arg(filePath);
        return 404;
    }
}

int WebAPIAdapter::instanceConfigurationFilePost(
        SWGSDRangel::SWGConfigurationImportExport& query,
        SWGSDRangel::SWGConfigurationIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    QString filePath = *query.getFilePath();

    if (QFileInfo(filePath).absoluteDir().exists())
    {
        SWGSDRangel::SWGConfigurationIdentifier *configurationId = query.getConfiguration();
        const Configuration *selectedConfiguration = m_mainCore->m_settings.getConfiguration(
            *configurationId->getGroupName(),
            *configurationId->getName());

        if (selectedConfiguration)
        {
            QString base64Str = selectedConfiguration->serialize().toBase64();
            QFileInfo fileInfo(filePath);

            if (fileInfo.suffix() != "cfgx") {
                filePath += ".cfgx";
            }

            QFile file(filePath);

            if (file.open(QIODevice::ReadWrite | QIODevice::Text))
            {
                QTextStream outstream(&file);
                outstream << base64Str;
                file.close();
                response.init();
                *response.getGroupName() = selectedConfiguration->getGroup();
                *response.getName() = selectedConfiguration->getDescription();
                return 200;
            }
            else
            {
                error.init();
                *error.getMessage() = QString("Cannot open file %1 for writing").arg(filePath);
                return 500;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no configuration [%1, %2]")
                .arg(*configurationId->getGroupName())
                .arg(*configurationId->getName());
            return 404;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("File %1 directory does not exist").arg(filePath);
        return 404;
    }
}

int WebAPIAdapter::instanceConfigurationBlobPut(
        SWGSDRangel::SWGBase64Blob& query,
        SWGSDRangel::SWGConfigurationIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    QString *base64Str = query.getBlob();

    if (base64Str)
    {
        Configuration *newConfiguration = m_mainCore->m_settings.newConfiguration("TBD", "TBD");

        if (newConfiguration)
        {
            QByteArray blob = QByteArray::fromBase64(base64Str->toUtf8());

            if (newConfiguration->deserialize(blob))
            {
                response.init();
                *response.getGroupName() = newConfiguration->getGroup();
                *response.getName() = newConfiguration->getDescription();
                return 202;
            }
            else
            {
                m_mainCore->m_settings.deleteConfiguration(newConfiguration);
                error.init();
                *error.getMessage() = QString("Could not deserialize blob");
                return 400;
            }
        }
        else
        {
            error.init();
            *error.getMessage() = QString("Cannot create new configuration");
            return 500;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("Blob not specified");
        return 400;
    }
}

int WebAPIAdapter::instanceConfigurationBlobPost(
        SWGSDRangel::SWGConfigurationIdentifier& query,
        SWGSDRangel::SWGBase64Blob& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const Configuration *selectedConfiguration = m_mainCore->m_settings.getConfiguration(
        *query.getGroupName(),
        *query.getName());

    if (selectedConfiguration)
    {
        QString base64Str = selectedConfiguration->serialize().toBase64();
        response.init();
        *response.getBlob() = base64Str;
        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no configuration [%1, %2]")
            .arg(*query.getGroupName())
            .arg(*query.getName());
        return 404;
    }
}

int WebAPIAdapter::instanceFeaturePresetsGet(
        SWGSDRangel::SWGFeaturePresets& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    int nbPresets = m_mainCore->m_settings.getFeatureSetPresetCount();
    int nbGroups = 0;
    int nbPresetsThisGroup = 0;
    QString groupName;
    response.init();
    QList<SWGSDRangel::SWGFeaturePresetGroup*> *groups = response.getGroups();
    QList<SWGSDRangel::SWGFeaturePresetItem*> *swgPresets = 0;
    int i = 0;

    // Presets are sorted by group first

    for (; i < nbPresets; i++)
    {
        const FeatureSetPreset *preset = m_mainCore->m_settings.getFeatureSetPreset(i);

        if ((i == 0) || (groupName != preset->getGroup())) // new group
        {
            if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
            groups->append(new SWGSDRangel::SWGFeaturePresetGroup);
            groups->back()->init();
            groupName = preset->getGroup();
            *groups->back()->getGroupName() = groupName;
            swgPresets = groups->back()->getPresets();
            nbGroups++;
            nbPresetsThisGroup = 0;
        }

        swgPresets->append(new SWGSDRangel::SWGFeaturePresetItem);
        swgPresets->back()->init();
        *swgPresets->back()->getDescription() = preset->getDescription();
        nbPresetsThisGroup++;
    }

    if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
    response.setNbGroups(nbGroups);

    return 200;
}

int WebAPIAdapter::instanceFeaturePresetDelete(
        SWGSDRangel::SWGFeaturePresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const FeatureSetPreset *selectedPreset = m_mainCore->m_settings.getFeatureSetPreset(*response.getGroupName(),
            *response.getDescription());

    if (selectedPreset == nullptr)
    {
        error.init();
        *error.getMessage() = QString("There is no feature preset [%1, %2]")
            .arg(*response.getGroupName())
            .arg(*response.getDescription());
        return 404;
    }

    *response.getGroupName() = selectedPreset->getGroup();
    *response.getDescription() = selectedPreset->getDescription();

    MainCore::MsgDeleteFeatureSetPreset *msg = MainCore::MsgDeleteFeatureSetPreset::create(const_cast<FeatureSetPreset*>(selectedPreset));
    m_mainCore->m_mainMessageQueue->push(msg);

    return 202;
}

int WebAPIAdapter::instanceDeviceSetsGet(
        SWGSDRangel::SWGDeviceSetList& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    getDeviceSetList(&response);
    return 200;
}

int WebAPIAdapter::instanceDeviceSetPost(
        int direction,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    MainCore::MsgAddDeviceSet *msg = MainCore::MsgAddDeviceSet::create(direction);
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    *response.getMessage() = QString("Message to add a new device set (MsgAddDeviceSet) was submitted successfully");

    return 202;
}

int WebAPIAdapter::instanceDeviceSetDelete(
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if (m_mainCore->m_deviceSets.size() > 0)
    {
        MainCore::MsgRemoveLastDeviceSet *msg = MainCore::MsgRemoveLastDeviceSet::create();
        m_mainCore->m_mainMessageQueue->push(msg);

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

int WebAPIAdapter::instanceWorkspacePost(
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    MainCore::MsgAddWorkspace *msg = MainCore::MsgAddWorkspace::create();
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    *response.getMessage() = QString("Message to add a new workspace (MsgAddWorkspace) was submitted successfully");

    return 202;
}

int WebAPIAdapter::instanceWorkspaceDelete(
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    (void) error;
    MainCore::MsgDeleteEmptyWorkspaces *msg = MainCore::MsgDeleteEmptyWorkspaces::create();
    m_mainCore->m_mainMessageQueue->push(msg);

    response.init();
    *response.getMessage() = QString("Message to delete empty workspaces (MsgDeleteEmptyWorkspaces) was submitted successfully");

    return 202;
}

int WebAPIAdapter::devicesetGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceSet& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        const DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
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

int WebAPIAdapter::devicesetSpectrumSettingsGet(
        int deviceSetIndex,
        SWGSDRangel::SWGGLSpectrum& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        const DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        return deviceSet->webapiSpectrumSettingsGet(response, *error.getMessage());
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapter::devicesetSpectrumSettingsPutPatch(
        int deviceSetIndex,
        bool force, //!< true to force settings = put else patch
        const QStringList& spectrumSettingsKeys,
        SWGSDRangel::SWGGLSpectrum& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        return deviceSet->webapiSpectrumSettingsPutPatch(force, spectrumSettingsKeys, response, *error.getMessage());
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapter::devicesetSpectrumServerGet(
        int deviceSetIndex,
        SWGSDRangel::SWGSpectrumServer& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        const DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        deviceSet->webapiSpectrumServerGet(response, *error.getMessage());

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapter::devicesetSpectrumServerPost(
        int deviceSetIndex,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        deviceSet->webapiSpectrumServerPost(response, *error.getMessage());

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapter::devicesetSpectrumServerDelete(
        int deviceSetIndex,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        deviceSet->webapiSpectrumServerDelete(response, *error.getMessage());

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapter::devicesetSpectrumWorkspaceGet(
        int deviceSetIndex,
        SWGSDRangel::SWGWorkspaceInfo& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        const DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
        response.setIndex(deviceSet->m_spectrumVis->getWorkspaceIndex());
        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapter::devicesetSpectrumWorkspacePut(
        int deviceSetIndex,
        SWGSDRangel::SWGWorkspaceInfo& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        int workspaceIndex = query.getIndex();
        MainCore::MsgMoveMainSpectrumUIToWorkspace *msg = MainCore::MsgMoveMainSpectrumUIToWorkspace::create(deviceSetIndex, workspaceIndex);
        m_mainCore->m_mainMessageQueue->push(msg);
        response.init();
        *response.getMessage() = QString("Message to move a main spectrum to workspace (MsgMoveMainSpectrumUIToWorkspace) was submitted successfully");
        return 202;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapter::devicesetDevicePut(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceListItem& query,
        SWGSDRangel::SWGDeviceListItem& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

            MainCore::MsgSetDevice *msg = MainCore::MsgSetDevice::create(deviceSetIndex, i, query.getDirection());
            m_mainCore->m_mainMessageQueue->push(msg);

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

int WebAPIAdapter::devicesetDeviceSettingsGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceActionsPost(
        int deviceSetIndex,
        const QStringList& deviceActionsKeys,
        SWGSDRangel::SWGDeviceActions& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceWorkspaceGet(
        int deviceSetIndex,
        SWGSDRangel::SWGWorkspaceInfo& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        response.setIndex(m_mainCore->m_deviceSets[deviceSetIndex]->m_deviceAPI->getWorkspaceIndex());
        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapter::devicesetDeviceWorkspacePut(
        int deviceSetIndex,
        SWGSDRangel::SWGWorkspaceInfo& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        int workspaceIndex = query.getIndex();
        MainCore::MsgMoveDeviceUIToWorkspace *msg = MainCore::MsgMoveDeviceUIToWorkspace::create(deviceSetIndex, workspaceIndex);
        m_mainCore->m_mainMessageQueue->push(msg);
        response.init();
        *response.getMessage() = QString("Message to move a device UI to workspace (MsgMoveDeviceUIToWorkspace) was submitted successfully");
        return 202;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapter::devicesetDeviceSettingsPutPatch(
        int deviceSetIndex,
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceRunGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceSubsystemRunGet(
        int deviceSetIndex,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceRunPost(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceSubsystemRunPost(
        int deviceSetIndex,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceRunDelete(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceSubsystemRunDelete(
        int deviceSetIndex,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetDeviceReportGet(
        int deviceSetIndex,
        SWGSDRangel::SWGDeviceReport& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetChannelsReportGet(
        int deviceSetIndex,
        SWGSDRangel::SWGChannelsDetail& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        const DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];
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

int WebAPIAdapter::devicesetChannelPost(
            int deviceSetIndex,
            SWGSDRangel::SWGChannelSettings& query,
			SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

        if (query.getDirection() == 0) // Single Rx
        {
            if (!deviceSet->m_deviceSourceEngine && !deviceSet->m_deviceMIMOEngine)
            {
                error.init();
                *error.getMessage() = QString("Device set at %1 is not a receive capable device set").arg(deviceSetIndex);
                return 400;
            }

            PluginAPI::ChannelRegistrations *channelRegistrations = m_mainCore->m_pluginManager->getRxChannelRegistrations();
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
                MainCore::MsgAddChannel *msg = MainCore::MsgAddChannel::create(deviceSetIndex, index, 0);
                m_mainCore->m_mainMessageQueue->push(msg);

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

            PluginAPI::ChannelRegistrations *channelRegistrations = m_mainCore->m_pluginManager->getTxChannelRegistrations();
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
                MainCore::MsgAddChannel *msg = MainCore::MsgAddChannel::create(deviceSetIndex, index, 1);
                m_mainCore->m_mainMessageQueue->push(msg);

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

            PluginAPI::ChannelRegistrations *channelRegistrations = m_mainCore->m_pluginManager->getMIMOChannelRegistrations();
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
            	MainCore::MsgAddChannel *msg = MainCore::MsgAddChannel::create(deviceSetIndex, index, 2);
                m_mainCore->m_mainMessageQueue->push(msg);

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

int WebAPIAdapter::devicesetChannelDelete(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

        if (channelIndex < deviceSet->getNumberOfChannels())
        {
            MainCore::MsgDeleteChannel *msg = MainCore::MsgDeleteChannel::create(deviceSetIndex, channelIndex);
            m_mainCore->m_mainMessageQueue->push(msg);

            response.init();
            *response.getMessage() = QString("Message to delete a channel (MsgDeleteChannel) was submitted successfully");

            return 202;
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no channel at index %1. %2 channel(s) left")
                    .arg(channelIndex)
                    .arg(deviceSet->getNumberOfChannels());
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

int WebAPIAdapter::devicesetChannelSettingsGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetChannelReportGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelReport& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetChannelActionsPost(
        int deviceSetIndex,
        int channelIndex,
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::devicesetChannelWorkspaceGet(
        int deviceSetIndex,
        int channelIndex,
        SWGSDRangel::SWGWorkspaceInfo& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // Single Rx
        {
            ChannelAPI *channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);

            if (channelAPI == nullptr)
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }
            else
            {
                return channelAPI->webapiWorkspaceGet(response, *error.getMessage());
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
                return channelAPI->webapiWorkspaceGet(response, *error.getMessage());
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
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getChanelSourceAPIAt(channelIndex - nbSinkChannels);
            }
            else if (channelIndex < nbSinkChannels + nbSourceChannels + nbMIMOChannels)
            {
                channelAPI = deviceSet->m_deviceAPI->getMIMOChannelAPIAt(channelIndex - nbSinkChannels - nbSourceChannels);
            }
            else
            {
                *error.getMessage() = QString("There is no channel with index %1").arg(channelIndex);
                return 404;
            }

            if (channelAPI)
            {
                return channelAPI->webapiWorkspaceGet(response, *error.getMessage());
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

int WebAPIAdapter::devicesetChannelWorkspacePut(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

        if ((channelIndex >= 0) && (channelIndex < deviceSet->getNumberOfChannels()))
        {
            int workspaceIndex = query.getIndex();
            MainCore::MsgMoveChannelUIToWorkspace *msg = MainCore::MsgMoveChannelUIToWorkspace::create(deviceSetIndex, channelIndex, workspaceIndex);
            m_mainCore->m_mainMessageQueue->push(msg);
            response.init();
            *response.getMessage() = QString("Message to move a channel UI to workspace (MsgMoveChannelUIToWorkspace) was submitted successfully");
            return 202;
        }
        else
        {
            *error.getMessage() = QString("There is no channel with index %1 in device set %2").arg(channelIndex).arg(deviceSetIndex);
            return 404;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);
        return 404;
    }
}

int WebAPIAdapter::devicesetChannelSettingsPutPatch(
        int deviceSetIndex,
        int channelIndex,
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainCore->m_deviceSets.size()))
    {
        DeviceSet *deviceSet = m_mainCore->m_deviceSets[deviceSetIndex];

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

int WebAPIAdapter::featuresetFeaturePost(
            int featureSetIndex,
            SWGSDRangel::SWGFeatureSettings& query,
			SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        PluginAPI::FeatureRegistrations *featureRegistrations = m_mainCore->m_pluginManager->getFeatureRegistrations();
        int nbRegistrations = featureRegistrations->size();
        int index = 0;

        for (; index < nbRegistrations; index++)
        {
            if (featureRegistrations->at(index).m_featureId == *query.getFeatureType()) {
                break;
            }
        }

        if (index < nbRegistrations)
        {
            MainCore::MsgAddFeature *msg = MainCore::MsgAddFeature::create(featureSetIndex, index);
            m_mainCore->m_mainMessageQueue->push(msg);

            response.init();
            *response.getMessage() = QString("Message to add a feature (MsgAddFeature) was submitted successfully");

            return 202;
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no feature with id %1").arg(*query.getFeatureType());
            return 404;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureDelete(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];

        if (featureIndex < featureSet->getNumberOfFeatures())
        {
            MainCore::MsgDeleteFeature *msg = MainCore::MsgDeleteFeature::create(featureSetIndex, featureIndex);
            m_mainCore->m_mainMessageQueue->push(msg);

            response.init();
            *response.getMessage() = QString("Message to delete a feature (MsgDeleteFeature) was submitted successfully");

            return 202;
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no feature at index %1. %2 feature(s) left")
                    .arg(featureIndex)
                    .arg(featureSet->getNumberOfFeatures());
            return 400;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureRunGet(
    int featureSetIndex,
    int featureIndex,
    SWGSDRangel::SWGDeviceState& response,
    SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];

        if (featureIndex < featureSet->getNumberOfFeatures())
        {
            response.init();
            const Feature *feature = featureSet->getFeatureAt(featureIndex);
            return feature->webapiRunGet(response, *error.getMessage());
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no feature at index %1. %2 feature(s) left")
                    .arg(featureIndex)
                    .arg(featureSet->getNumberOfFeatures());
            return 400;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureRunPost(
    int featureSetIndex,
    int featureIndex,
    SWGSDRangel::SWGDeviceState& response,
    SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];

        if (featureIndex < featureSet->getNumberOfFeatures())
        {
            response.init();
            Feature *feature = featureSet->getFeatureAt(featureIndex);
            return feature->webapiRun(true, response, *error.getMessage());
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no feature at index %1. %2 feature(s) left")
                    .arg(featureIndex)
                    .arg(featureSet->getNumberOfFeatures());
            return 400;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureRunDelete(
    int featureSetIndex,
    int featureIndex,
    SWGSDRangel::SWGDeviceState& response,
    SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];

        if (featureIndex < featureSet->getNumberOfFeatures())
        {
            response.init();
            Feature *feature = featureSet->getFeatureAt(featureIndex);
            return feature->webapiRun(false, response, *error.getMessage());
        }
        else
        {
            error.init();
            *error.getMessage() = QString("There is no feature at index %1. %2 feature(s) left")
                    .arg(featureIndex)
                    .arg(featureSet->getNumberOfFeatures());
            return 400;
        }
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetPresetPatch(
        int featureSetIndex,
        SWGSDRangel::SWGFeaturePresetIdentifier& query,
        SWGSDRangel::SWGErrorResponse& error)
{
    int nbFeatureSets = m_mainCore->m_featureSets.size();

    if (featureSetIndex >= nbFeatureSets)
    {
        error.init();
        *error.getMessage() = QString("There is no feature set at index %1. Number of device sets is %2").arg(featureSetIndex).arg(nbFeatureSets);
        return 404;
    }

    const FeatureSetPreset *selectedPreset = m_mainCore->m_settings.getFeatureSetPreset(
            *query.getGroupName(),
            *query.getDescription());

    if (selectedPreset == 0)
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2]")
                .arg(*query.getGroupName())
                .arg(*query.getDescription());
        return 404;
    }

    MainCore::MsgLoadFeatureSetPreset *msg = MainCore::MsgLoadFeatureSetPreset::create(selectedPreset, featureSetIndex);
    m_mainCore->m_mainMessageQueue->push(msg);

    return 202;
}

int WebAPIAdapter::featuresetPresetPut(
        int featureSetIndex,
        SWGSDRangel::SWGFeaturePresetIdentifier& query,
        SWGSDRangel::SWGErrorResponse& error)
{
    int nbFeatureSets = m_mainCore->m_featureSets.size();

    if (featureSetIndex >= nbFeatureSets)
    {
        error.init();
        *error.getMessage() = QString("There is no feature set at index %1. Number of feature sets is %2").arg(featureSetIndex).arg(nbFeatureSets);
        return 404;
    }

    const FeatureSetPreset *selectedPreset = m_mainCore->m_settings.getFeatureSetPreset(
            *query.getGroupName(),
            *query.getDescription());

    if (selectedPreset == 0)
    {
        error.init();
        *error.getMessage() = QString("There is no preset [%1, %2]")
                .arg(*query.getGroupName())
                .arg(*query.getDescription());
        return 404;
    }

    MainCore::MsgSaveFeatureSetPreset *msg = MainCore::MsgSaveFeatureSetPreset::create(const_cast<FeatureSetPreset*>(selectedPreset), featureSetIndex, false);
    m_mainCore->m_mainMessageQueue->push(msg);

    return 202;
}

int WebAPIAdapter::featuresetPresetPost(
        int featureSetIndex,
        SWGSDRangel::SWGFeaturePresetIdentifier& query,
        SWGSDRangel::SWGErrorResponse& error)
{
    int nbFeatureSets = m_mainCore->m_featureSets.size();

    if (featureSetIndex >= nbFeatureSets)
    {
        error.init();
        *error.getMessage() = QString("There is no feature set at index %1. Number of feature sets is %2").arg(featureSetIndex).arg(nbFeatureSets);
        return 404;
    }

    const FeatureSetPreset *selectedPreset = m_mainCore->m_settings.getFeatureSetPreset(
            *query.getGroupName(),
            *query.getDescription());

    if (selectedPreset == 0) // save on a new preset
    {
        selectedPreset = m_mainCore->m_settings.newFeatureSetPreset(*query.getGroupName(), *query.getDescription());
    }
    else
    {
        error.init();
        *error.getMessage() = QString("Preset already exists [%1, %2]")
                .arg(*query.getGroupName())
                .arg(*query.getDescription());
        return 409;
    }

    MainCore::MsgSaveFeatureSetPreset *msg = MainCore::MsgSaveFeatureSetPreset::create(const_cast<FeatureSetPreset*>(selectedPreset), featureSetIndex, true);
    m_mainCore->m_mainMessageQueue->push(msg);

    return 202;
}

int WebAPIAdapter::featuresetFeatureSettingsGet(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGFeatureSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        Feature *feature = featureSet->getFeatureAt(featureIndex);

        if (feature)
        {
            response.setFeatureType(new QString());
            feature->getIdentifier(*response.getFeatureType());
            return feature->webapiSettingsGet(response, *error.getMessage());
        }
        else
        {
            *error.getMessage() = QString("There is no feature with index %1").arg(featureIndex);
            return 404;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureSettingsPutPatch(
        int featureSetIndex,
        int featureIndex,
        bool force,
        const QStringList& featureSettingsKeys,
        SWGSDRangel::SWGFeatureSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        Feature *feature = featureSet->getFeatureAt(featureIndex);

        if (feature)
        {
            QString featureType;
            feature->getIdentifier(featureType);

            if (featureType == *response.getFeatureType())
            {
                return feature->webapiSettingsPutPatch(force, featureSettingsKeys, response, *error.getMessage());
            }
            else
            {
                *error.getMessage() = QString("There is no feature type %1 at index %2. Found %3.")
                        .arg(*response.getFeatureType())
                        .arg(featureIndex)
                        .arg(featureType);
                return 404;
            }
        }
        else
        {
            *error.getMessage() = QString("There is no feature with index %1").arg(featureIndex);
            return 404;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureReportGet(
    int featureSetIndex,
    int featureIndex,
    SWGSDRangel::SWGFeatureReport& response,
    SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        Feature *feature = featureSet->getFeatureAt(featureIndex);

        if (feature)
        {
            response.setFeatureType(new QString());
            feature->getIdentifier(*response.getFeatureType());
            return feature->webapiReportGet(response, *error.getMessage());
        }
        else
        {
            *error.getMessage() = QString("There is no feature with index %1").arg(featureIndex);
            return 404;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureActionsPost(
        int featureSetIndex,
        int featureIndex,
        const QStringList& featureActionsKeys,
        SWGSDRangel::SWGFeatureActions& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        Feature *feature = featureSet->getFeatureAt(featureIndex);

        if (feature)
        {
            QString featureType;
            feature->getIdentifier(featureType);

            if (featureType == *query.getFeatureType())
            {
                int res = feature->webapiActionsPost(featureActionsKeys, query, *error.getMessage());

                if (res/100 == 2)
                {
                    response.init();
                    *response.getMessage() = QString("Message to post action was submitted successfully");
                }

                return res;
            }
            else
            {
                *error.getMessage() = QString("There is no feature type %1 at index %2. Found %3.")
                        .arg(*query.getFeatureType())
                        .arg(featureIndex)
                        .arg(featureType);
                return 404;
            }
        }
        else
        {
            *error.getMessage() = QString("There is no feature with index %1").arg(featureIndex);
            return 404;
        }
    }
    else
    {
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureWorkspaceGet(
        int featureIndex,
        SWGSDRangel::SWGWorkspaceInfo& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureIndex >= 0) && (featureIndex < (int) m_mainCore->m_featureSets.size()))
    {
        FeatureSet *featureSet = m_mainCore->m_featureSets[0];
        Feature *feature = featureSet->getFeatureAt(featureIndex);
        response.setIndex(feature->getWorkspaceIndex());
        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature with index %1").arg(featureIndex);
        return 404;
    }
}

int WebAPIAdapter::featuresetFeatureWorkspacePut(
        int featureIndex,
        SWGSDRangel::SWGWorkspaceInfo& query,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureIndex >= 0) && (featureIndex < (int) m_mainCore->m_featureSets.size()))
    {
        int workspaceIndex = query.getIndex();
        MainCore::MsgMoveFeatureUIToWorkspace *msg = MainCore::MsgMoveFeatureUIToWorkspace::create(featureIndex, workspaceIndex);
        m_mainCore->m_mainMessageQueue->push(msg);
        response.init();
        *response.getMessage() = QString("Message to move a feature UI to workspace (MsgMoveFeatureUIToWorkspace) was submitted successfully");
        return 202;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature with index %1").arg(featureIndex);
        return 404;
    }
}

void WebAPIAdapter::getDeviceSetList(SWGSDRangel::SWGDeviceSetList* deviceSetList)
{
    deviceSetList->init();
    deviceSetList->setDevicesetcount((int) m_mainCore->m_deviceSets.size());

    if (m_mainCore->m_deviceSets.size() > 0) {
        deviceSetList->setDevicesetfocus(m_mainCore->m_masterTabIndex);
    }

    std::vector<DeviceSet*>::const_iterator it = m_mainCore->m_deviceSets.begin();

    for (int i = 0; it != m_mainCore->m_deviceSets.end(); ++it, i++)
    {
        QList<SWGSDRangel::SWGDeviceSet*> *deviceSets = deviceSetList->getDeviceSets();
        deviceSets->append(new SWGSDRangel::SWGDeviceSet());

        getDeviceSet(deviceSets->back(), *it, i);
    }
}

void WebAPIAdapter::getDeviceSet(SWGSDRangel::SWGDeviceSet *swgDeviceSet, const DeviceSet* deviceSet, int deviceSetIndex)
{
    swgDeviceSet->init();
    SWGSDRangel::SWGSamplingDevice *samplingDevice = swgDeviceSet->getSamplingDevice();
    samplingDevice->init();
    samplingDevice->setIndex(deviceSetIndex);

    if (deviceSet->m_deviceSinkEngine) // Single Tx data
    {
        samplingDevice->setDirection(1);
        *samplingDevice->getHwType() = deviceSet->m_deviceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceSet->m_deviceAPI->getSamplingDeviceSerial();
        samplingDevice->setSequence(deviceSet->m_deviceAPI->getSamplingDeviceSequence());
        samplingDevice->setDeviceNbStreams(deviceSet->m_deviceAPI->getDeviceNbItems());
        samplingDevice->setDeviceStreamIndex(deviceSet->m_deviceAPI->getDeviceItemIndex());
        deviceSet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSink *sampleSink = deviceSet->m_deviceSinkEngine->getSink();

        if (sampleSink) {
            samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSink->getSampleRate());
        }

        swgDeviceSet->setChannelcount(deviceSet->m_deviceAPI->getNbSourceChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = swgDeviceSet->getChannels();

        for (int i = 0; i <  swgDeviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSourceAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(1);
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceSet->m_deviceSourceEngine) // Rx data
    {
        samplingDevice->setDirection(0);
        *samplingDevice->getHwType() = deviceSet->m_deviceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceSet->m_deviceAPI->getSamplingDeviceSerial();
        samplingDevice->setSequence(deviceSet->m_deviceAPI->getSamplingDeviceSequence());
        samplingDevice->setDeviceNbStreams(deviceSet->m_deviceAPI->getDeviceNbItems());
        samplingDevice->setDeviceStreamIndex(deviceSet->m_deviceAPI->getDeviceItemIndex());
        deviceSet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSource *sampleSource = deviceSet->m_deviceSourceEngine->getSource();

        if (sampleSource) {
            samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSource->getSampleRate());
        }

        swgDeviceSet->setChannelcount(deviceSet->m_deviceAPI->getNbSinkChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = swgDeviceSet->getChannels();

        for (int i = 0; i <  swgDeviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSinkAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(0);
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceSet->m_deviceMIMOEngine) // MIMO data
    {
        samplingDevice->setDirection(2);
        *samplingDevice->getHwType() = deviceSet->m_deviceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceSet->m_deviceAPI->getSamplingDeviceSerial();
        samplingDevice->setSequence(deviceSet->m_deviceAPI->getSamplingDeviceSequence());
        samplingDevice->setDeviceNbStreams(deviceSet->m_deviceAPI->getDeviceNbItems());
        samplingDevice->setDeviceStreamIndex(deviceSet->m_deviceAPI->getDeviceItemIndex());
        samplingDevice->setState(new QString("notStarted"));
        deviceSet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getStateRx(), 0);
        deviceSet->m_deviceAPI->getDeviceEngineStateStr(*samplingDevice->getStateTx(), 1);
        DeviceSampleMIMO *sampleMIMO = deviceSet->m_deviceMIMOEngine->getMIMO();

        if (sampleMIMO)
        {
            samplingDevice->setCenterFrequency(sampleMIMO->getMIMOCenterFrequency());
            samplingDevice->setBandwidth(sampleMIMO->getMIMOSampleRate());
        }

        int nbSinkChannels = deviceSet->m_deviceAPI->getNbSinkChannels();
        int nbSourceChannels = deviceSet->m_deviceAPI->getNbSourceChannels();
        int nbMIMOChannels = deviceSet->m_deviceAPI->getNbMIMOChannels();
        swgDeviceSet->setChannelcount(nbSinkChannels + nbSourceChannels + nbMIMOChannels);
        QList<SWGSDRangel::SWGChannel*> *channels = swgDeviceSet->getChannels();

        for (int i = 0; i < nbSinkChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSinkAPIAt(i);
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
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSourceAPIAt(i);
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
            ChannelAPI *channel = deviceSet->m_deviceAPI->getMIMOChannelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getCenterFrequency());
            channels->back()->setDirection(2);
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }
}

void WebAPIAdapter::getChannelsDetail(SWGSDRangel::SWGChannelsDetail *channelsDetail, const DeviceSet* deviceSet)
{
    channelsDetail->init();
    SWGSDRangel::SWGChannelReport *channelReport;
    QString channelReportError;

    if (deviceSet->m_deviceSinkEngine) // Tx data
    {
        channelsDetail->setChannelcount(deviceSet->m_deviceAPI->getNbSourceChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();

        for (int i = 0; i <  channelsDetail->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSourceAPIAt(i);
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

    if (deviceSet->m_deviceSourceEngine) // Rx data
    {
        channelsDetail->setChannelcount(deviceSet->m_deviceAPI->getNbSinkChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();

        for (int i = 0; i <  channelsDetail->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSinkAPIAt(i);
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

    if (deviceSet->m_deviceMIMOEngine) // MIMO data
    {
        int nbSinkChannels = deviceSet->m_deviceAPI->getNbSinkChannels();
        int nbSourceChannels = deviceSet->m_deviceAPI->getNbSourceChannels();
        int nbMIMOChannels = deviceSet->m_deviceAPI->getNbMIMOChannels();
        QList<SWGSDRangel::SWGChannel*> *channels = channelsDetail->getChannels();
        channelsDetail->setChannelcount(nbSinkChannels + nbSourceChannels + nbMIMOChannels);

        for (int i = 0; i < nbSinkChannels; i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            channels->back()->init();
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSinkAPIAt(i);
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
            ChannelAPI *channel = deviceSet->m_deviceAPI->getChanelSourceAPIAt(i);
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
            ChannelAPI *channel = deviceSet->m_deviceAPI->getMIMOChannelAPIAt(i);
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

int WebAPIAdapter::featuresetGet(
        int featureSetIndex,
        SWGSDRangel::SWGFeatureSet& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_mainCore->m_featureSets.size()))
    {
        const FeatureSet *featureSet = m_mainCore->m_featureSets[featureSetIndex];
        getFeatureSet(&response, featureSet);

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no feature set with index %1").arg(featureSetIndex);

        return 404;
    }
}

void WebAPIAdapter::getFeatureSet(SWGSDRangel::SWGFeatureSet *swgFeatureSet, const FeatureSet* featureSet)
{
    swgFeatureSet->init();
    swgFeatureSet->setFeaturecount(featureSet->getNumberOfFeatures());
    QList<SWGSDRangel::SWGFeature*> *features = swgFeatureSet->getFeatures();

    for (int i = 0; i < featureSet->getNumberOfFeatures(); i++)
    {
        const Feature *feature = featureSet->getFeatureAt(i);
        features->append(new SWGSDRangel::SWGFeature);
        features->back()->setIndex(i);
        QString s;
        feature->getTitle(s);
        features->back()->setTitle(new QString(s));
        feature->getIdentifier(s);
        features->back()->setId(new QString(s));
        features->back()->setUid(feature->getUID());
    }
}

QtMsgType WebAPIAdapter::getMsgTypeFromString(const QString& msgTypeString)
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

void WebAPIAdapter::getMsgTypeString(const QtMsgType& msgType, QString& levelStr)
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
