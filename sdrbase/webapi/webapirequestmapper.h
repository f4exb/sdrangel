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

#ifndef SDRBASE_WEBAPI_WEBAPIREQUESTMAPPER_H_
#define SDRBASE_WEBAPI_WEBAPIREQUESTMAPPER_H_

#include <QJsonParseError>

#include "httprequesthandler.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "staticfilecontroller.h"
#include "webapiadapterinterface.h"

#include "export.h"

namespace SWGSDRangel
{
    class SWGPresetTransfer;
    class SWGPresetIdentifier;
    class SWGPreset;
    class SWGChannelConfig;
    class SWGDeviceConfig;
    class SWGFeatureConfig;
    class SWGFeatureActions;
    class SWGFeatureSetPreset;
}

class SDRBASE_API WebAPIRequestMapper : public qtwebapp::HttpRequestHandler {
    Q_OBJECT
public:
    WebAPIRequestMapper(QObject* parent=0);
    ~WebAPIRequestMapper();
    void service(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void setAdapter(WebAPIAdapterInterface *adapter) { m_adapter = adapter; }

private:
    WebAPIAdapterInterface *m_adapter;
    qtwebapp::StaticFileController *m_staticFileController;

    void instanceSummaryService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceConfigService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceDevicesService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceChannelsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceFeaturesService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceLoggingService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceAudioService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceAudioInputParametersService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceAudioOutputParametersService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceAudioInputCleanupService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceAudioOutputCleanupService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceLocationService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instancePresetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instancePresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instancePresetFileService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instancePresetBlobService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceConfigurationsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceConfigurationService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceConfigurationFileService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceConfigurationBlobService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceFeaturePresetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceFeaturePresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceDeviceSetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceDeviceSetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceWorkspaceService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);

    void devicesetService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetSpectrumSettingsService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetSpectrumServerService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetSpectrumWorkspaceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceSettingsService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceRunService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceSubsystemRunService(const std::string& indexStr, const std::string& subsystemIndexStr,qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceReportService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceActionsService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceWorkspaceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelsReportService(const std::string& deviceSetIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelService(const std::string& deviceSetIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelIndexService(const std::string& deviceSetIndexStr, const std::string& channelIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelSettingsService(const std::string& deviceSetIndexStr, const std::string& channelIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelReportService(const std::string& deviceSetIndexStr, const std::string& channelIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelActionsService(const std::string& deviceSetIndexStr, const std::string& channelIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelWorkspaceService(const std::string& deviceSetIndexStr, const std::string& channelIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);

    void featuresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetFeatureService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetPresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetFeatureIndexService(const std::string& featureIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetFeatureRunService(const std::string& featureIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetFeatureSettingsService(const std::string& featureIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetFeatureReportService(const std::string& featureIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetFeatureActionsService(const std::string& featureIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void featuresetFeatureWorkspaceService(const std::string& featureIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);

    bool validatePresetTransfer(SWGSDRangel::SWGPresetTransfer& presetTransfer);
    bool validatePresetIdentifer(SWGSDRangel::SWGPresetIdentifier& presetIdentifier);
    bool validatePresetExport(SWGSDRangel::SWGPresetExport& presetExport);
    bool validateSpectrumSettings(SWGSDRangel::SWGGLSpectrum& spectrumSettings, QJsonObject& jsonObject, QStringList& spectrumSettingsKeys);
    bool validateDeviceListItem(SWGSDRangel::SWGDeviceListItem& deviceListItem, QJsonObject& jsonObject);
    bool validateDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings, QJsonObject& jsonObject, QStringList& deviceSettingsKeys);
    bool validateDeviceActions(SWGSDRangel::SWGDeviceActions& deviceActions, QJsonObject& jsonObject, QStringList& deviceActionsKeys);
    bool validateChannelSettings(SWGSDRangel::SWGChannelSettings& channelSettings, QJsonObject& jsonObject, QStringList& channelSettingsKeys);
    bool validateChannelActions(SWGSDRangel::SWGChannelActions& channelActions, QJsonObject& jsonObject, QStringList& channelActionsKeys);
    bool validateFeaturePresetIdentifer(SWGSDRangel::SWGFeaturePresetIdentifier& presetIdentifier);
    bool validateFeatureSettings(SWGSDRangel::SWGFeatureSettings& featureSettings, QJsonObject& jsonObject, QStringList& featureSettingsKeys);
    bool validateFeatureActions(SWGSDRangel::SWGFeatureActions& featureActions, QJsonObject& jsonObject, QStringList& featureActionsKeys);
    bool validateAudioInputDevice(SWGSDRangel::SWGAudioInputDevice& audioInputDevice, QJsonObject& jsonObject, QStringList& audioInputDeviceKeys);
    bool validateAudioOutputDevice(SWGSDRangel::SWGAudioOutputDevice& audioOutputDevice, QJsonObject& jsonObject, QStringList& audioOutputDeviceKeys);
    bool validateConfig(SWGSDRangel::SWGInstanceConfigResponse& config, QJsonObject& jsonObject, WebAPIAdapterInterface::ConfigKeys& configKeys);
    bool validateWorkspaceInfo(SWGSDRangel::SWGWorkspaceInfo& workspaceInfo, QJsonObject& jsonObject);
    bool validateConfigurationIdentifier(SWGSDRangel::SWGConfigurationIdentifier& configurationIdentifier);
    bool validateConfigurationImportExport(SWGSDRangel::SWGConfigurationImportExport& configuratopmImportExport);

    bool appendFeatureSetPresetKeys(
        SWGSDRangel::SWGFeatureSetPreset *preset,
        const QJsonObject& presetJson,
        WebAPIAdapterInterface::FeatureSetPresetKeys& featureSetPresetKeys
    );

    bool appendPresetKeys(
            SWGSDRangel::SWGPreset *preset,
            const QJsonObject& presetJson,
            WebAPIAdapterInterface::PresetKeys& presetKeys);

    bool appendPresetFeatureKeys(
        SWGSDRangel::SWGFeatureConfig *feature,
        const QJsonObject& featureSettingsJson,
        WebAPIAdapterInterface::FeatureKeys& featureKeys
    );

    bool appendPresetChannelKeys(
            SWGSDRangel::SWGChannelConfig *channel,
            const QJsonObject& channelSettngsJson,
            WebAPIAdapterInterface::ChannelKeys& channelKeys
    );

    bool getChannelSettings(
        const QString& channelSettingsKey,
        SWGSDRangel::SWGChannelSettings *channelSettings,
        const QJsonObject& channelSettingsJson,
        QStringList& channelSettingsKeys
    );

    bool getChannelActions(
        const QString& channelActionsKey,
        SWGSDRangel::SWGChannelActions *channelActions,
        const QJsonObject& channelActionsJson,
        QStringList& channelSettingsKeys
    );

    bool appendPresetDeviceKeys(
            SWGSDRangel::SWGDeviceConfig *device,
            const QJsonObject& deviceSettngsJson,
            WebAPIAdapterInterface::DeviceKeys& devicelKeys
    );

    bool getDeviceSettings(
        const QString& deviceSettingsKey,
        SWGSDRangel::SWGDeviceSettings *deviceSettings,
        const QJsonObject& deviceSettingsJson,
        QStringList& deviceActionsKeys
    );

    bool getDeviceActions(
        const QString& deviceActionsKey,
        SWGSDRangel::SWGDeviceActions *deviceActions,
        const QJsonObject& deviceActionsJson,
        QStringList& deviceActionsKeys
    );

    void extractKeys(
        const QJsonObject& rootJsonObject,
        QStringList& keyList
    );

    bool getFeatureSettings(
        const QString& featureSettingsKey,
        SWGSDRangel::SWGFeatureSettings *featureSettings,
        const QJsonObject& featureSettingsJson,
        QStringList& featureSettingsKeys
    );

    bool getFeatureActions(
        const QString& featureActionsKey,
        SWGSDRangel::SWGFeatureActions *featureActions,
        const QJsonObject& featureActionsJson,
        QStringList& featureSettingsKeys
    );

    bool parseJsonBody(QString& jsonStr, QJsonObject& jsonObject, qtwebapp::HttpResponse& response);

    void resetSpectrumSettings(SWGSDRangel::SWGGLSpectrum& spectrumSettings);
    void resetDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings);
    void resetDeviceReport(SWGSDRangel::SWGDeviceReport& deviceReport);
    void resetDeviceActions(SWGSDRangel::SWGDeviceActions& deviceActions);
    void resetChannelSettings(SWGSDRangel::SWGChannelSettings& deviceSettings);
    void resetChannelReport(SWGSDRangel::SWGChannelReport& channelReport);
    void resetChannelActions(SWGSDRangel::SWGChannelActions& channelActions);
    void resetAudioInputDevice(SWGSDRangel::SWGAudioInputDevice& audioInputDevice);
    void resetAudioOutputDevice(SWGSDRangel::SWGAudioOutputDevice& audioOutputDevice);
    void resetFeatureSettings(SWGSDRangel::SWGFeatureSettings& deviceSettings);
    void resetFeatureReport(SWGSDRangel::SWGFeatureReport& featureReport);
    void resetFeatureActions(SWGSDRangel::SWGFeatureActions& featureActions);

    static const QMap<QString, QString> m_channelURIToSettingsKey;
    static const QMap<QString, QString> m_deviceIdToSettingsKey;
    static const QMap<QString, QString> m_channelTypeToSettingsKey;
    static const QMap<QString, QString> m_sourceDeviceHwIdToSettingsKey;
    static const QMap<QString, QString> m_sinkDeviceHwIdToSettingsKey;
    static const QMap<QString, QString> m_mimoDeviceHwIdToSettingsKey;
    static const QMap<QString, QString> m_channelTypeToActionsKey;
    static const QMap<QString, QString> m_sourceDeviceHwIdToActionsKey;
    static const QMap<QString, QString> m_sinkDeviceHwIdToActionsKey;
    static const QMap<QString, QString> m_mimoDeviceHwIdToActionsKey;
    static const QMap<QString, QString> m_featureTypeToSettingsKey;
    static const QMap<QString, QString> m_featureTypeToActionsKey;
    static const QMap<QString, QString> m_featureURIToSettingsKey;
};

#endif /* SDRBASE_WEBAPI_WEBAPIREQUESTMAPPER_H_ */
