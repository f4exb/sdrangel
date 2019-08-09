///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "plugin/pluginmanager.h"
#include "channel/channelwebapiadapter.h"
#include "channel/channelutils.h"
#include "device/devicewebapiadapter.h"
#include "device/deviceutils.h"
#include "dsp/glspectrumsettings.h"
#include "webapiadapterbase.h"

WebAPIAdapterBase::WebAPIAdapterBase()
{}

WebAPIAdapterBase::~WebAPIAdapterBase()
{
    m_webAPIChannelAdapters.flush();
}

void WebAPIAdapterBase::webapiFormatPreferences(
    SWGSDRangel::SWGPreferences *apiPreferences,
    const Preferences& preferences
)
{
    apiPreferences->init();
    apiPreferences->setSourceDevice(new QString(preferences.getSourceDevice()));
    apiPreferences->setSourceIndex(preferences.getSourceIndex());
    apiPreferences->setAudioType(new QString(preferences.getAudioType()));
    apiPreferences->setAudioDevice(new QString(preferences.getAudioDevice()));
    apiPreferences->setLatitude(preferences.getLatitude());
    apiPreferences->setLongitude(preferences.getLongitude());
    apiPreferences->setConsoleMinLogLevel((int) preferences.getConsoleMinLogLevel());
    apiPreferences->setUseLogFile(preferences.getUseLogFile() ? 1 : 0);
    apiPreferences->setLogFileName(new QString(preferences.getLogFileName()));
    apiPreferences->setFileMinLogLevel((int) preferences.getFileMinLogLevel());
}

void WebAPIAdapterBase::webapiInitConfig(
    MainSettings& mainSettings
)
{
    mainSettings.initialize();
}

void WebAPIAdapterBase::webapiUpdatePreferences(
    SWGSDRangel::SWGPreferences *apiPreferences,
    const QStringList& preferenceKeys,
    Preferences& preferences
)
{
    if (preferenceKeys.contains("consoleMinLogLevel")) {
        preferences.setConsoleMinLogLevel((QtMsgType) apiPreferences->getConsoleMinLogLevel());
    }
    if (preferenceKeys.contains("fileMinLogLevel")) {
        preferences.setFileMinLogLevel((QtMsgType) apiPreferences->getFileMinLogLevel());
    }
    if (preferenceKeys.contains("latitude")) {
        preferences.setLatitude(apiPreferences->getLatitude());
    }
    if (preferenceKeys.contains("logFileName")) {
        preferences.setLogFileName(*apiPreferences->getLogFileName());
    }
    if (preferenceKeys.contains("longitude")) {
        preferences.setLongitude(apiPreferences->getLongitude());
    }
    if (preferenceKeys.contains("sourceDevice")) {
        preferences.setSourceDevice(*apiPreferences->getSourceDevice());
    }
    if (preferenceKeys.contains("sourceIndex")) {
        preferences.setSourceIndex(apiPreferences->getSourceIndex());
    }
    if (preferenceKeys.contains("useLogFile")) {
        preferences.setUseLogFile(apiPreferences->getUseLogFile() != 0);
    }

    if (preferenceKeys.contains("consoleMinLogLevel"))


    if (apiPreferences->getSourceDevice()) {
        preferences.setSourceDevice(*apiPreferences->getSourceDevice());
    }
    preferences.setSourceIndex(apiPreferences->getSourceIndex());
    if (apiPreferences->getAudioType()) {
        preferences.setAudioType(*apiPreferences->getAudioType());
    }
    if (apiPreferences->getAudioDevice()) {
        preferences.setAudioDevice(*apiPreferences->getAudioDevice());
    }
    preferences.setLatitude(apiPreferences->getLatitude());
    preferences.setLongitude(apiPreferences->getLongitude());
    preferences.setConsoleMinLogLevel((QtMsgType) apiPreferences->getConsoleMinLogLevel());
    preferences.setUseLogFile(apiPreferences->getUseLogFile() != 0);
    if (apiPreferences->getLogFileName()) {
        preferences.setLogFileName(*apiPreferences->getLogFileName());
    }
    preferences.setFileMinLogLevel((QtMsgType) apiPreferences->getFileMinLogLevel());
}

void WebAPIAdapterBase::webapiFormatPreset(
        SWGSDRangel::SWGPreset *apiPreset,
        const Preset& preset
)
{
    apiPreset->init();
    apiPreset->setSourcePreset(preset.isSourcePreset() ? 1 : 0);
    apiPreset->setGroup(new QString(preset.getGroup()));
    apiPreset->setDescription(new QString(preset.getDescription()));
    apiPreset->setCenterFrequency(preset.getCenterFrequency());
    apiPreset->setDcOffsetCorrection(preset.hasDCOffsetCorrection() ? 1 : 0);
    apiPreset->setIqImbalanceCorrection(preset.hasIQImbalanceCorrection() ? 1 : 0);
    const QByteArray& spectrumConfig = preset.getSpectrumConfig();
    GLSpectrumSettings m_spectrumSettings;

    if (m_spectrumSettings.deserialize(spectrumConfig))
    {
        SWGSDRangel::SWGGLSpectrum *swgSpectrumConfig = apiPreset->getSpectrumConfig();
        swgSpectrumConfig->init();
        swgSpectrumConfig->setFftSize(m_spectrumSettings.m_fftSize);
        swgSpectrumConfig->setFftOverlap(m_spectrumSettings.m_fftOverlap);
        swgSpectrumConfig->setFftWindow((int) m_spectrumSettings.m_fftWindow);
        swgSpectrumConfig->setRefLevel(m_spectrumSettings.m_refLevel);
        swgSpectrumConfig->setPowerRange(m_spectrumSettings.m_powerRange);
        swgSpectrumConfig->setDisplayWaterfall(m_spectrumSettings.m_displayWaterfall ? 0 : 1);
        swgSpectrumConfig->setInvertedWaterfall(m_spectrumSettings.m_invertedWaterfall ? 0 : 1);
        swgSpectrumConfig->setDisplayMaxHold(m_spectrumSettings.m_displayMaxHold ? 0 : 1);
        swgSpectrumConfig->setDisplayHistogram(m_spectrumSettings.m_displayHistogram ? 0 : 1);
        swgSpectrumConfig->setDecay(m_spectrumSettings.m_decay);
        swgSpectrumConfig->setDisplayGrid(m_spectrumSettings.m_displayGrid ? 1 : 0);
        swgSpectrumConfig->setInvert(m_spectrumSettings.m_invert ? 1 : 0);
        swgSpectrumConfig->setDisplayGridIntensity(m_spectrumSettings.m_displayGridIntensity);
        swgSpectrumConfig->setDecayDivisor(m_spectrumSettings.m_decayDivisor);
        swgSpectrumConfig->setHistogramStroke(m_spectrumSettings.m_histogramStroke);
        swgSpectrumConfig->setDisplayCurrent(m_spectrumSettings.m_displayCurrent ? 1 : 0);
        swgSpectrumConfig->setDisplayTraceIntensity(m_spectrumSettings.m_displayTraceIntensity);
        swgSpectrumConfig->setWaterfallShare(m_spectrumSettings.m_waterfallShare);
        swgSpectrumConfig->setAveragingMode((int) m_spectrumSettings.m_averagingMode);
        swgSpectrumConfig->setAveragingValue(GLSpectrumSettings::getAveragingValue(m_spectrumSettings.m_averagingIndex, m_spectrumSettings.m_averagingMode));
        swgSpectrumConfig->setLinear(m_spectrumSettings.m_linear ? 1 : 0);
    }

    int nbChannels = preset.getChannelCount();
    for (int i = 0; i < nbChannels; i++)
    {
        const Preset::ChannelConfig& channelConfig = preset.getChannelConfig(i);
        QList<SWGSDRangel::SWGChannelConfig *> *swgChannelConfigs = apiPreset->getChannelConfigs();
        swgChannelConfigs->append(new SWGSDRangel::SWGChannelConfig);
        swgChannelConfigs->back()->init();
        swgChannelConfigs->back()->setChannelIdUri(new QString(channelConfig.m_channelIdURI));
        const QByteArray& channelSettings = channelConfig.m_config;
        SWGSDRangel::SWGChannelSettings *swgChannelSettings = swgChannelConfigs->back()->getConfig();
        swgChannelSettings->init();
        ChannelWebAPIAdapter *channelWebAPIAdapter = m_webAPIChannelAdapters.getChannelWebAPIAdapter(channelConfig.m_channelIdURI, m_pluginManager);

        if (channelWebAPIAdapter)
        {
            channelWebAPIAdapter->deserialize(channelSettings);
            QString errorMessage;
            channelWebAPIAdapter->webapiSettingsGet(*swgChannelSettings, errorMessage);
        }
    }

    int nbDevices = preset.getDeviceCount();
    for (int i = 0; i < nbDevices; i++)
    {
        const Preset::DeviceConfig& deviceConfig = preset.getDeviceConfig(i);
        QList<SWGSDRangel::SWGDeviceConfig *> *swgdeviceConfigs = apiPreset->getDeviceConfigs();
        swgdeviceConfigs->append(new SWGSDRangel::SWGDeviceConfig);
        swgdeviceConfigs->back()->init();
        swgdeviceConfigs->back()->setDeviceId(new QString(deviceConfig.m_deviceId));
        swgdeviceConfigs->back()->setDeviceSerial(new QString(deviceConfig.m_deviceSerial));
        swgdeviceConfigs->back()->setDeviceSequence(deviceConfig.m_deviceSequence);
        const QByteArray& deviceSettings = deviceConfig.m_config;
        SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = swgdeviceConfigs->back()->getConfig();
        swgDeviceSettings->init();
        DeviceWebAPIAdapter *deviceWebAPIAdapter = m_webAPIDeviceAdapters.getDeviceWebAPIAdapter(deviceConfig.m_deviceId, m_pluginManager);

        if (deviceWebAPIAdapter)
        {
            deviceWebAPIAdapter->deserialize(deviceSettings);
            QString errorMessage;
            deviceWebAPIAdapter->webapiSettingsGet(*swgDeviceSettings, errorMessage);
        }
    }
}

void WebAPIAdapterBase::webapiUpdatePreset(
        SWGSDRangel::SWGPreset *apiPreset,
        const WebAPIAdapterInterface::PresetKeys& presetKeys,
        Preset& preset
)
{
}

void WebAPIAdapterBase::webapiFormatCommand(
        SWGSDRangel::SWGCommand *apiCommand,
        const Command& command
)
{
    apiCommand->init();
    apiCommand->setGroup(new QString(command.getGroup()));
    apiCommand->setDescription(new QString(command.getDescription()));
    apiCommand->setCommand(new QString(command.getCommand()));
    apiCommand->setArgString(new QString(command.getArgString()));
    apiCommand->setKey((int) command.getKey());
    apiCommand->setKeyModifiers((int) command.getKeyModifiers());
    apiCommand->setAssociateKey(command.getAssociateKey() ? 1 : 0);
    apiCommand->setRelease(command.getRelease() ? 1 : 0);
}

void WebAPIAdapterBase::webapiUpdateCommand(
        SWGSDRangel::SWGCommand *apiCommand,
        const WebAPIAdapterInterface::CommandKeys& commandKeys,
        Command& command
)
{
    if (commandKeys.m_keys.contains("argString")) {
        command.setArgString(*apiCommand->getArgString());
    }
    if (commandKeys.m_keys.contains("associateKey")) {
        command.setAssociateKey(apiCommand->getAssociateKey());
    }
    if (commandKeys.m_keys.contains("command")) {
        command.setCommand(*apiCommand->getCommand());
    }
    if (commandKeys.m_keys.contains("description")) {
        command.setDescription(*apiCommand->getDescription());
    }
    if (commandKeys.m_keys.contains("group")) {
        command.setGroup(*apiCommand->getGroup());
    }
    if (commandKeys.m_keys.contains("key")) {
        command.setKey((Qt::Key) apiCommand->getKey());
    }
    if (commandKeys.m_keys.contains("keyModifiers")) {
        command.setKeyModifiers((Qt::KeyboardModifiers) apiCommand->getKeyModifiers());
    }
    if (commandKeys.m_keys.contains("release")) {
        command.setRelease(apiCommand->getRelease() != 0);
    }
}

ChannelWebAPIAdapter *WebAPIAdapterBase::WebAPIChannelAdapters::getChannelWebAPIAdapter(const QString& channelURI, const PluginManager *pluginManager)
{
    QString registeredChannelURI = ChannelUtils::getRegisteredChannelURI(channelURI);
    QMap<QString, ChannelWebAPIAdapter*>::iterator it = m_webAPIChannelAdapters.find(registeredChannelURI);

    if (it == m_webAPIChannelAdapters.end())
    {
        const PluginInterface *pluginInterface = pluginManager->getChannelPluginInterface(registeredChannelURI);

        if (pluginInterface)
        {
            ChannelWebAPIAdapter *channelAPI = pluginInterface->createChannelWebAPIAdapter();
            m_webAPIChannelAdapters.insert(registeredChannelURI, channelAPI);
            return channelAPI;
        }
        else
        {
            m_webAPIChannelAdapters.insert(registeredChannelURI, nullptr);
            return nullptr;
        }
    }
    else
    {
        return *it;
    }
}

void WebAPIAdapterBase::WebAPIChannelAdapters::flush()
{
    foreach(ChannelWebAPIAdapter *channelAPI, m_webAPIChannelAdapters) {
        delete channelAPI;
    }

    m_webAPIChannelAdapters.clear();
}

DeviceWebAPIAdapter *WebAPIAdapterBase::WebAPIDeviceAdapters::getDeviceWebAPIAdapter(const QString& deviceId, const PluginManager *pluginManager)
{
    QString registeredDeviceId = DeviceUtils::getRegisteredDeviceURI(deviceId);
    QMap<QString, DeviceWebAPIAdapter*>::iterator it = m_webAPIDeviceAdapters.find(registeredDeviceId);

    if (it == m_webAPIDeviceAdapters.end())
    {
        const PluginInterface *pluginInterface = pluginManager->getDevicePluginInterface(registeredDeviceId);

        if (pluginInterface)
        {
            DeviceWebAPIAdapter *deviceWebAPIAdapter = pluginInterface->createDeviceWebAPIAdapter();
            m_webAPIDeviceAdapters.insert(registeredDeviceId, deviceWebAPIAdapter);
            return deviceWebAPIAdapter;
        }
        else
        {
            m_webAPIDeviceAdapters.insert(registeredDeviceId, nullptr);
            return nullptr;
        }
    }
    else
    {
        return *it;
    }
}

void WebAPIAdapterBase::WebAPIDeviceAdapters::flush()
{
    foreach(DeviceWebAPIAdapter *deviceWebAPIAdapter, m_webAPIDeviceAdapters) {
        delete deviceWebAPIAdapter;
    }

    m_webAPIDeviceAdapters.clear();
}
