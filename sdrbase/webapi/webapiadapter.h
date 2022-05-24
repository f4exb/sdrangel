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

#ifndef SDRBASE_WEBAPI_WEBAPIADAPTER_H_
#define SDRBASE_WEBAPI_WEBAPIADAPTER_H_

#include <QtGlobal>

#include "webapi/webapiadapterinterface.h"
#include "export.h"

class MainCore;
class DeviceSet;
class FeatureSet;

class SDRBASE_API WebAPIAdapter: public WebAPIAdapterInterface
{
public:
    WebAPIAdapter();
    virtual ~WebAPIAdapter();

    virtual int instanceSummary(
            SWGSDRangel::SWGInstanceSummaryResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigGet(
            SWGSDRangel::SWGInstanceConfigResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigPutPatch(
            bool force, // PUT else PATCH
            SWGSDRangel::SWGInstanceConfigResponse& query,
            const ConfigKeys& configKeys,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceDevices(
            int direction,
            SWGSDRangel::SWGInstanceDevicesResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceChannels(
            int direction,
            SWGSDRangel::SWGInstanceChannelsResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceFeatures(
            SWGSDRangel::SWGInstanceFeaturesResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLoggingGet(
            SWGSDRangel::SWGLoggingInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLoggingPut(
            SWGSDRangel::SWGLoggingInfo& query,
            SWGSDRangel::SWGLoggingInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAudioGet(
            SWGSDRangel::SWGAudioDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAudioInputPatch(
            SWGSDRangel::SWGAudioInputDevice& response,
            const QStringList& audioInputKeys,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAudioOutputPatch(
            SWGSDRangel::SWGAudioOutputDevice& response,
            const QStringList& audioOutputKeys,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAudioInputDelete(
            SWGSDRangel::SWGAudioInputDevice& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAudioOutputDelete(
            SWGSDRangel::SWGAudioOutputDevice& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAudioInputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAudioOutputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLocationGet(
            SWGSDRangel::SWGLocationInformation& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLocationPut(
            SWGSDRangel::SWGLocationInformation& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetsGet(
            SWGSDRangel::SWGPresets& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetPatch(
            SWGSDRangel::SWGPresetTransfer& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetPut(
            SWGSDRangel::SWGPresetTransfer& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetPost(
            SWGSDRangel::SWGPresetTransfer& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetDelete(
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetFilePut(
            SWGSDRangel::SWGFilePath& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetFilePost(
            SWGSDRangel::SWGPresetExport& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetBlobPut(
            SWGSDRangel::SWGBase64Blob& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetBlobPost(
            SWGSDRangel::SWGPresetIdentifier& query,
            SWGSDRangel::SWGBase64Blob& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationsGet(
            SWGSDRangel::SWGConfigurations& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationPatch(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationPut(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationPost(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationDelete(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationFilePut(
            SWGSDRangel::SWGFilePath& query,
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationFilePost(
            SWGSDRangel::SWGConfigurationImportExport& query,
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationBlobPut(
            SWGSDRangel::SWGBase64Blob& query,
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceConfigurationBlobPost(
            SWGSDRangel::SWGConfigurationIdentifier& query,
            SWGSDRangel::SWGBase64Blob& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceFeaturePresetsGet(
            SWGSDRangel::SWGFeaturePresets& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceFeaturePresetDelete(
            SWGSDRangel::SWGFeaturePresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceDeviceSetsGet(
            SWGSDRangel::SWGDeviceSetList& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceDeviceSetPost(
            int direction,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceDeviceSetDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceWorkspacePost(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceWorkspaceDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceSet& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetSpectrumSettingsGet(
            int deviceSetIndex,
            SWGSDRangel::SWGGLSpectrum& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetSpectrumSettingsPutPatch(
            int deviceSetIndex,
            bool force, //!< true to force settings = put else patch
            const QStringList& spectrumSettingsKeys,
            SWGSDRangel::SWGGLSpectrum& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetSpectrumServerGet(
            int deviceSetIndex,
            SWGSDRangel::SWGSpectrumServer& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetSpectrumServerPost(
            int deviceSetIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetSpectrumServerDelete(
            int deviceSetIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetSpectrumWorkspaceGet(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetSpectrumWorkspacePut(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDevicePut(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceListItem& query,
            SWGSDRangel::SWGDeviceListItem& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceSettingsGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceActionsPost(
            int deviceSetIndex,
            const QStringList& deviceActionsKeys,
            SWGSDRangel::SWGDeviceActions& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceWorkspaceGet(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceWorkspacePut(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceSettingsPutPatch(
            int deviceSetIndex,
            bool force,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceRunGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceRunPost(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceRunDelete(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceSubsystemRunGet(
            int deviceSetIndex,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceSubsystemRunPost(
            int deviceSetIndex,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceSubsystemRunDelete(
            int deviceSetIndex,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetDeviceReportGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceReport& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelsReportGet(
            int deviceSetIndex,
            SWGSDRangel::SWGChannelsDetail& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelPost(
            int deviceSetIndex,
            SWGSDRangel::SWGChannelSettings& query,
			SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelDelete(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelSettingsGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelActionsPost(
            int deviceSetIndex,
            int channelIndex,
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelSettingsPutPatch(
            int deviceSetIndex,
            int channelIndex,
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelReportGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelReport& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelWorkspaceGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetChannelWorkspacePut(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetGet(
            int deviceSetIndex,
            SWGSDRangel::SWGFeatureSet& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeaturePost(
            int featureSetIndex,
            SWGSDRangel::SWGFeatureSettings& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureDelete(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureRunGet(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureRunPost(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureRunDelete(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetPresetPatch(
            int featureSetIndex,
            SWGSDRangel::SWGFeaturePresetIdentifier& query,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetPresetPut(
            int featureSetIndex,
            SWGSDRangel::SWGFeaturePresetIdentifier& query,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetPresetPost(
            int featureSetIndex,
            SWGSDRangel::SWGFeaturePresetIdentifier& query,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureSettingsGet(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGFeatureSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureSettingsPutPatch(
            int featureSetIndex,
            int featureIndex,
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureReportGet(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGFeatureReport& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureActionsPost(
            int featureSetIndex,
            int featureIndex,
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureWorkspaceGet(
            int featureIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int featuresetFeatureWorkspacePut(
            int featureIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

private:
    MainCore *m_mainCore;

    void getDeviceSetList(SWGSDRangel::SWGDeviceSetList* deviceSetList);
    void getDeviceSet(SWGSDRangel::SWGDeviceSet *swgDeviceSet, const DeviceSet* deviceSet, int deviceSetIndex);
    void getChannelsDetail(SWGSDRangel::SWGChannelsDetail *channelsDetail, const DeviceSet* deviceSet);
    void getFeatureSet(SWGSDRangel::SWGFeatureSet *swgFeatureSet, const FeatureSet* featureSet);
    static QtMsgType getMsgTypeFromString(const QString& msgTypeString);
    static void getMsgTypeString(const QtMsgType& msgType, QString& level);
};

#endif /* SDRGUI_WEBAPI_WEBAPIADAPTERGUI_H_ */
