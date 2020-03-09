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

#ifndef SDRSRV_WEBAPI_WEBAPIADAPTERSRV_H_
#define SDRSRV_WEBAPI_WEBAPIADAPTERSRV_H_

#include <QtGlobal>

#include "webapi/webapiadapterinterface.h"

class MainCore;
class DeviceSet;

class WebAPIAdapterSrv: public WebAPIAdapterInterface
{
public:
    WebAPIAdapterSrv(MainCore& mainCore);
    virtual ~WebAPIAdapterSrv();

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

    virtual int instanceDVSerialGet(
            SWGSDRangel::SWGDVSerialDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceDVSerialPatch(
            bool dvserial,
            SWGSDRangel::SWGDVSerialDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAMBESerialGet(
            SWGSDRangel::SWGDVSerialDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAMBEDevicesGet(
            SWGSDRangel::SWGAMBEDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAMBEDevicesPut(
            SWGSDRangel::SWGAMBEDevices& query,
            SWGSDRangel::SWGAMBEDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAMBEDevicesPatch(
            SWGSDRangel::SWGAMBEDevices& query,
            SWGSDRangel::SWGAMBEDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceAMBEDevicesDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

#ifdef HAS_LIMERFEUSB
    virtual int instanceLimeRFESerialGet(
            SWGSDRangel::SWGLimeRFEDevices& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLimeRFEConfigGet(
            const QString& serial,
            SWGSDRangel::SWGLimeRFESettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLimeRFEConfigPut(
            SWGSDRangel::SWGLimeRFESettings& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLimeRFERunPut(
            SWGSDRangel::SWGLimeRFESettings& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instanceLimeRFEPowerGet(
            const QString& serial,
            SWGSDRangel::SWGLimeRFEPower& response,
            SWGSDRangel::SWGErrorResponse& error);
#endif

    virtual int instancePresetFilePut(
            SWGSDRangel::SWGPresetImport& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int instancePresetFilePost(
            SWGSDRangel::SWGPresetExport& query,
            SWGSDRangel::SWGPresetIdentifier& response,
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

    virtual int devicesetGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceSet& response,
            SWGSDRangel::SWGErrorResponse& error);

    virtual int devicesetFocusPatch(
            int deviceSetIndex,
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

private:
    MainCore& m_mainCore;

    void getDeviceSetList(SWGSDRangel::SWGDeviceSetList* deviceSetList);
    void getDeviceSet(SWGSDRangel::SWGDeviceSet *swgDeviceSet, const DeviceSet* deviceSet, int deviceUISetIndex);
    void getChannelsDetail(SWGSDRangel::SWGChannelsDetail *channelsDetail, const DeviceSet* deviceSet);
    static QtMsgType getMsgTypeFromString(const QString& msgTypeString);
    static void getMsgTypeString(const QtMsgType& msgType, QString& level);
};

#endif /* SDRSRV_WEBAPI_WEBAPIADAPTERSRV_H_ */
