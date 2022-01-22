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
#include "feature/featurewebapiadapter.h"
#include "feature/featureutils.h"
#include "dsp/spectrumsettings.h"
#include "webapiadapterbase.h"

WebAPIAdapterBase::WebAPIAdapterBase()
{}

WebAPIAdapterBase::~WebAPIAdapterBase()
{
    m_webAPIChannelAdapters.flush();
    m_webAPIFeatureAdapters.flush();
    m_webAPIDeviceAdapters.flush();
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
}

void WebAPIAdapterBase::webapiFormatFeatureSetPreset(
        SWGSDRangel::SWGFeatureSetPreset *apiPreset,
        const FeatureSetPreset& preset
)
{
    apiPreset->init();
    apiPreset->setGroup(new QString(preset.getGroup()));
    apiPreset->setDescription(new QString(preset.getDescription()));

    int nbFeatures = preset.getFeatureCount();
    for (int i = 0; i < nbFeatures; i++)
    {
        const FeatureSetPreset::FeatureConfig& featureConfig = preset.getFeatureConfig(i);
        QList<SWGSDRangel::SWGFeatureConfig *> *swgFeatureConfigs = apiPreset->getFeatureConfigs();
        swgFeatureConfigs->append(new SWGSDRangel::SWGFeatureConfig);
        swgFeatureConfigs->back()->init();
        swgFeatureConfigs->back()->setFeatureIdUri(new QString(featureConfig.m_featureIdURI));
        const QByteArray& featureSettings = featureConfig.m_config;
        SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = swgFeatureConfigs->back()->getConfig();
        swgFeatureSettings->init();
        FeatureWebAPIAdapter *featureWebAPIAdapter = m_webAPIFeatureAdapters.getFeatureWebAPIAdapter(featureConfig.m_featureIdURI, m_pluginManager);

        if (featureWebAPIAdapter)
        {
            featureWebAPIAdapter->deserialize(featureSettings);
            QString errorMessage;
            featureWebAPIAdapter->webapiSettingsGet(*swgFeatureSettings, errorMessage);
        }
    }
}

void WebAPIAdapterBase::webapiFormatPreset(
        SWGSDRangel::SWGPreset *apiPreset,
        const Preset& preset
)
{
    apiPreset->init();
    apiPreset->setPresetType(preset.getPresetType());
    apiPreset->setGroup(new QString(preset.getGroup()));
    apiPreset->setDescription(new QString(preset.getDescription()));
    apiPreset->setCenterFrequency(preset.getCenterFrequency());
    apiPreset->setDcOffsetCorrection(preset.hasDCOffsetCorrection() ? 1 : 0);
    apiPreset->setIqImbalanceCorrection(preset.hasIQImbalanceCorrection() ? 1 : 0);
    const QByteArray& spectrumConfig = preset.getSpectrumConfig();
    SpectrumSettings m_spectrumSettings;

    if (m_spectrumSettings.deserialize(spectrumConfig))
    {
        SWGSDRangel::SWGGLSpectrum *swgSpectrumConfig = apiPreset->getSpectrumConfig();
        swgSpectrumConfig->init();
        swgSpectrumConfig->setFftSize(m_spectrumSettings.m_fftSize);
        swgSpectrumConfig->setFftOverlap(m_spectrumSettings.m_fftOverlap);
        swgSpectrumConfig->setFftWindow((int) m_spectrumSettings.m_fftWindow);
        swgSpectrumConfig->setRefLevel(m_spectrumSettings.m_refLevel);
        swgSpectrumConfig->setPowerRange(m_spectrumSettings.m_powerRange);
        swgSpectrumConfig->setFpsPeriodMs(m_spectrumSettings.m_fpsPeriodMs);
        swgSpectrumConfig->setDisplayWaterfall(m_spectrumSettings.m_displayWaterfall ? 1 : 0);
        swgSpectrumConfig->setInvertedWaterfall(m_spectrumSettings.m_invertedWaterfall ? 1 : 0);
        swgSpectrumConfig->setDisplayMaxHold(m_spectrumSettings.m_displayMaxHold ? 1 : 0);
        swgSpectrumConfig->setDisplayHistogram(m_spectrumSettings.m_displayHistogram ? 1 : 0);
        swgSpectrumConfig->setDecay(m_spectrumSettings.m_decay);
        swgSpectrumConfig->setDisplayGrid(m_spectrumSettings.m_displayGrid ? 1 : 0);
        swgSpectrumConfig->setDisplayGridIntensity(m_spectrumSettings.m_displayGridIntensity);
        swgSpectrumConfig->setDecayDivisor(m_spectrumSettings.m_decayDivisor);
        swgSpectrumConfig->setHistogramStroke(m_spectrumSettings.m_histogramStroke);
        swgSpectrumConfig->setDisplayCurrent(m_spectrumSettings.m_displayCurrent ? 1 : 0);
        swgSpectrumConfig->setDisplayTraceIntensity(m_spectrumSettings.m_displayTraceIntensity);
        swgSpectrumConfig->setWaterfallShare(m_spectrumSettings.m_waterfallShare);
        swgSpectrumConfig->setAveragingMode((int) m_spectrumSettings.m_averagingMode);
        swgSpectrumConfig->setAveragingValue(SpectrumSettings::getAveragingValue(m_spectrumSettings.m_averagingIndex, m_spectrumSettings.m_averagingMode));
        swgSpectrumConfig->setLinear(m_spectrumSettings.m_linear ? 1 : 0);
        swgSpectrumConfig->setMarkersDisplay((int) m_spectrumSettings.m_markersDisplay);
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

    apiPreset->setLayout(new QString(preset.getLayout().toBase64().toStdString().c_str()));
}

void WebAPIAdapterBase::webapiUpdateFeatureSetPreset(
    bool force,
    SWGSDRangel::SWGFeatureSetPreset *apiPreset,
    const WebAPIAdapterInterface::FeatureSetPresetKeys& presetKeys,
    FeatureSetPreset *preset
)
{
    if (presetKeys.m_keys.contains("description")) {
        preset->setDescription(*apiPreset->getDescription());
    }
    if (presetKeys.m_keys.contains("group")) {
        preset->setGroup(*apiPreset->getGroup());
    }

    if (force) { // PUT replaces feature list possibly erasing it if no features are given
        preset->clearFeatures();
    }

    QList<WebAPIAdapterInterface::FeatureKeys>::const_iterator featureKeysIt = presetKeys.m_featureKeys.begin();
    int i = 0;
    QString errorMessage;

    for (; featureKeysIt != presetKeys.m_featureKeys.end(); ++featureKeysIt, i++)
    {
        SWGSDRangel::SWGFeatureConfig *swgFeatureConfig = apiPreset->getFeatureConfigs()->at(i);

        if (!swgFeatureConfig) { // safety measure but should not happen
            continue;
        }

        if (featureKeysIt->m_keys.contains("featureIdURI"))
        {
            QString *featureIdURI = swgFeatureConfig->getFeatureIdUri();

            if (!featureIdURI) {
                continue;
            }

            FeatureWebAPIAdapter *featureWebAPIAdapter = m_webAPIFeatureAdapters.getFeatureWebAPIAdapter(*featureIdURI, m_pluginManager);

            if (!featureWebAPIAdapter) {
                continue;
            }

            SWGSDRangel::SWGFeatureSettings *featureSettings = swgFeatureConfig->getConfig();

            featureWebAPIAdapter->webapiSettingsPutPatch(
                true, // features are always appended
                featureKeysIt->m_featureKeys,
                *featureSettings,
                errorMessage
            );

            QByteArray config = featureWebAPIAdapter->serialize();
            preset->addFeature(*featureIdURI, config);
        }
    }
}

void WebAPIAdapterBase::webapiUpdatePreset(
        bool force,
        SWGSDRangel::SWGPreset *apiPreset,
        const WebAPIAdapterInterface::PresetKeys& presetKeys,
        Preset *preset
)
{
    if (presetKeys.m_keys.contains("centerFrequency")) {
        preset->setCenterFrequency(apiPreset->getCenterFrequency());
    }
    if (presetKeys.m_keys.contains("dcOffsetCorrection")) {
        preset->setDCOffsetCorrection(apiPreset->getDcOffsetCorrection() != 0);
    }
    if (presetKeys.m_keys.contains("iqImbalanceCorrection")) {
        preset->setIQImbalanceCorrection(apiPreset->getIqImbalanceCorrection() != 0);
    }
    if (presetKeys.m_keys.contains("presetType")) {
        preset->setPresetType((Preset::PresetType) apiPreset->getPresetType());
    }
    if (presetKeys.m_keys.contains("description")) {
        preset->setDescription(*apiPreset->getDescription());
    }
    if (presetKeys.m_keys.contains("group")) {
        preset->setGroup(*apiPreset->getGroup());
    }
    if (presetKeys.m_keys.contains("layout")) {
        preset->setLayout(QByteArray::fromBase64(apiPreset->getLayout()->toUtf8()));
    }

    SpectrumSettings spectrumSettings;

    if (!force) {
        spectrumSettings.deserialize(preset->getSpectrumConfig());
    }

    QStringList::const_iterator spectrumIt = presetKeys.m_spectrumKeys.begin();
    for (; spectrumIt != presetKeys.m_spectrumKeys.end(); ++spectrumIt)
    {
        if (spectrumIt->contains("averagingMode")) {
            spectrumSettings.m_averagingMode = (SpectrumSettings::AveragingMode) apiPreset->getSpectrumConfig()->getAveragingMode();
        }
        if (spectrumIt->contains("averagingValue"))
        {
            spectrumSettings.m_averagingValue = apiPreset->getSpectrumConfig()->getAveragingValue();
            spectrumSettings.m_averagingIndex = SpectrumSettings::getAveragingIndex(spectrumSettings.m_averagingValue, spectrumSettings.m_averagingMode);
        }
        if (spectrumIt->contains("decay")) {
            spectrumSettings.m_decay = apiPreset->getSpectrumConfig()->getDecay();
        }
        if (spectrumIt->contains("decayDivisor")) {
            spectrumSettings.m_decayDivisor = apiPreset->getSpectrumConfig()->getDecayDivisor();
        }
        if (spectrumIt->contains("displayCurrent")) {
            spectrumSettings.m_displayCurrent = apiPreset->getSpectrumConfig()->getDisplayCurrent() != 0;
        }
        if (spectrumIt->contains("displayGrid")) {
            spectrumSettings.m_displayGrid = apiPreset->getSpectrumConfig()->getDisplayGrid() != 0;
        }
        if (spectrumIt->contains("displayGridIntensity")) {
            spectrumSettings.m_displayGridIntensity = apiPreset->getSpectrumConfig()->getDisplayGridIntensity();
        }
        if (spectrumIt->contains("displayHistogram")) {
            spectrumSettings.m_displayHistogram = apiPreset->getSpectrumConfig()->getDisplayHistogram() != 0;
        }
        if (spectrumIt->contains("displayMaxHold")) {
            spectrumSettings.m_displayMaxHold = apiPreset->getSpectrumConfig()->getDisplayMaxHold() != 0;
        }
        if (spectrumIt->contains("displayTraceIntensity")) {
            spectrumSettings.m_displayTraceIntensity = apiPreset->getSpectrumConfig()->getDisplayTraceIntensity();
        }
        if (spectrumIt->contains("displayWaterfall")) {
            spectrumSettings.m_displayWaterfall = apiPreset->getSpectrumConfig()->getDisplayWaterfall();
        }
        if (spectrumIt->contains("fftOverlap")) {
            spectrumSettings.m_fftOverlap = apiPreset->getSpectrumConfig()->getFftOverlap();
        }
        if (spectrumIt->contains("fftSize")) {
            spectrumSettings.m_fftSize = apiPreset->getSpectrumConfig()->getFftSize();
        }
        if (spectrumIt->contains("fftWindow")) {
            spectrumSettings.m_fftWindow = (FFTWindow::Function) apiPreset->getSpectrumConfig()->getFftWindow();
        }
        if (spectrumIt->contains("histogramStroke")) {
            spectrumSettings.m_histogramStroke = apiPreset->getSpectrumConfig()->getHistogramStroke();
        }
        if (spectrumIt->contains("invertedWaterfall")) {
            spectrumSettings.m_invertedWaterfall = apiPreset->getSpectrumConfig()->getInvertedWaterfall() != 0;
        }
        if (spectrumIt->contains("linear")) {
            spectrumSettings.m_linear = apiPreset->getSpectrumConfig()->getLinear() != 0;
        }
        if (spectrumIt->contains("powerRange")) {
            spectrumSettings.m_powerRange = apiPreset->getSpectrumConfig()->getPowerRange();
        }
        if (spectrumIt->contains("refLevel")) {
            spectrumSettings.m_refLevel = apiPreset->getSpectrumConfig()->getRefLevel();
        }
        if (spectrumIt->contains("fpsPeriodMs"))
        {
            qint32 fpsPeriodMs = apiPreset->getSpectrumConfig()->getFpsPeriodMs();
            spectrumSettings.m_fpsPeriodMs = fpsPeriodMs < 5 ? 5 : fpsPeriodMs > 500 ? 500 : fpsPeriodMs;
        }
        if (spectrumIt->contains("waterfallShare")) {
            spectrumSettings.m_waterfallShare = apiPreset->getSpectrumConfig()->getWaterfallShare();
        }
        if (spectrumIt->contains("markersDisplay")) {
            spectrumSettings.m_markersDisplay = (SpectrumSettings::MarkersDisplay) apiPreset->getSpectrumConfig()->getMarkersDisplay();
        }
    }

    preset->setSpectrumConfig(spectrumSettings.serialize());

    if (force) { // PUT replaces devices list possibly erasing it if no devices are given
        preset->clearDevices();
    }

    QString errorMessage;
    QList<WebAPIAdapterInterface::DeviceKeys>::const_iterator deviceKeysIt = presetKeys.m_devicesKeys.begin();
    int i = 0;
    for (; deviceKeysIt != presetKeys.m_devicesKeys.end(); ++deviceKeysIt, i++)
    {
        SWGSDRangel::SWGDeviceConfig *swgDeviceConfig = apiPreset->getDeviceConfigs()->at(i);
        if (!swgDeviceConfig) { // safety measure but should not happen
            continue;
        }

        QString deviceId;
        int deviceSequence = 0;
        QString deviceSerial;

        if (deviceKeysIt->m_keys.contains("deviceId")) {
            deviceId = *swgDeviceConfig->getDeviceId();
        }
        if (deviceKeysIt->m_keys.contains("deviceSequence")) {
            deviceSequence = swgDeviceConfig->getDeviceSequence();
        }
        if (deviceKeysIt->m_keys.contains("deviceSerial")) {
            deviceSerial = *swgDeviceConfig->getDeviceSerial();
        }

        DeviceWebAPIAdapter *deviceWebAPIAdapter = m_webAPIDeviceAdapters.getDeviceWebAPIAdapter(deviceId, m_pluginManager);

        if (deviceWebAPIAdapter)
        {
            deviceWebAPIAdapter->webapiSettingsPutPatch(
                force,
                deviceKeysIt->m_deviceKeys,
                *swgDeviceConfig->getConfig(),
                errorMessage
            );

            QByteArray config = deviceWebAPIAdapter->serialize();
            preset->setDeviceConfig(deviceId, deviceSerial, deviceSequence, config); // add or update device
        }
    }

    if (force) { // PUT replaces channel list possibly erasing it if no channels are given
        preset->clearChannels();
    }

    QList<WebAPIAdapterInterface::ChannelKeys>::const_iterator channelKeysIt = presetKeys.m_channelsKeys.begin();
    i = 0;
    for (; channelKeysIt != presetKeys.m_channelsKeys.end(); ++channelKeysIt, i++)
    {
        SWGSDRangel::SWGChannelConfig *swgChannelConfig = apiPreset->getChannelConfigs()->at(i);
        if (!swgChannelConfig) { // safety measure but should not happen
            continue;
        }

        if (channelKeysIt->m_keys.contains("channelIdURI"))
        {
            QString *channelIdURI = swgChannelConfig->getChannelIdUri();
            if (!channelIdURI) {
                continue;
            }

            ChannelWebAPIAdapter *channelWebAPIAdapter = m_webAPIChannelAdapters.getChannelWebAPIAdapter(*channelIdURI, m_pluginManager);
            if (!channelWebAPIAdapter) {
                continue;
            }

            SWGSDRangel::SWGChannelSettings *channelSettings = swgChannelConfig->getConfig();

            channelWebAPIAdapter->webapiSettingsPutPatch(
                true, // channels are always appended
                channelKeysIt->m_channelKeys,
                *channelSettings,
                errorMessage
            );

            QByteArray config = channelWebAPIAdapter->serialize();
            preset->addChannel(*channelIdURI, config);
        }
    }
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

FeatureWebAPIAdapter *WebAPIAdapterBase::WebAPIFeatureAdapters::getFeatureWebAPIAdapter(const QString& featureURI, const PluginManager *pluginManager)
{
    QString registeredFeatureURI = FeatureUtils::getRegisteredFeatureURI(featureURI);
    QMap<QString, FeatureWebAPIAdapter*>::iterator it = m_webAPIFeatureAdapters.find(registeredFeatureURI);

    if (it == m_webAPIFeatureAdapters.end())
    {
        const PluginInterface *pluginInterface = pluginManager->getFeaturePluginInterface(registeredFeatureURI);

        if (pluginInterface)
        {
            FeatureWebAPIAdapter *featureAPI = pluginInterface->createFeatureWebAPIAdapter();
            m_webAPIFeatureAdapters.insert(registeredFeatureURI, featureAPI);
            return featureAPI;
        }
        else
        {
            m_webAPIFeatureAdapters.insert(registeredFeatureURI, nullptr);
            return nullptr;
        }
    }
    else
    {
        return *it;
    }
}

void WebAPIAdapterBase::WebAPIFeatureAdapters::flush()
{
    foreach(FeatureWebAPIAdapter *featureAPI, m_webAPIFeatureAdapters) {
        delete featureAPI;
    }

    m_webAPIFeatureAdapters.clear();
}
