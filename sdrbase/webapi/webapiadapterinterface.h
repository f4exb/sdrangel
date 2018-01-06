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

#ifndef SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_
#define SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_

#include <QString>
#include <regex>

#include "SWGErrorResponse.h"

namespace SWGSDRangel
{
    class SWGInstanceSummaryResponse;
    class SWGInstanceDevicesResponse;
    class SWGInstanceChannelsResponse;
    class SWGLoggingInfo;
    class SWGAudioDevices;
    class SWGAudioDevicesSelect;
    class SWGLocationInformation;
    class SWGDVSeralDevices;
    class SWGPresets;
    class SWGPresetTransfer;
    class SWGPresetIdentifier;
    class SWGPresetImport;
    class SWGPresetExport;
    class SWGDeviceSetList;
    class SWGDeviceSet;
    class SWGDeviceListItem;
    class SWGDeviceSettings;
    class SWGDeviceState;
    class SWGChannelSettings;
    class SWGSuccessResponse;
}

class WebAPIAdapterInterface
{
public:
    virtual ~WebAPIAdapterInterface() {}

    /**
     * Handler of /sdrangel (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceSummary
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceSummary(
            SWGSDRangel::SWGInstanceSummaryResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceSummary
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDelete(
            SWGSDRangel::SWGSuccessResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/devices (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceDevices
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDevices(
            bool tx __attribute__((unused)),
            SWGSDRangel::SWGInstanceDevicesResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/channels (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceChannels(
            bool tx __attribute__((unused)),
            SWGSDRangel::SWGInstanceChannelsResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/logging (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLoggingGet(
            SWGSDRangel::SWGLoggingInfo& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/logging (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLoggingPut(
            SWGSDRangel::SWGLoggingInfo& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/audio (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioGet(
            SWGSDRangel::SWGAudioDevices& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/audio (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioPatch(
            SWGSDRangel::SWGAudioDevicesSelect& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/location (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLocationGet(
            SWGSDRangel::SWGLocationInformation& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/location (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLocationPut(
            SWGSDRangel::SWGLocationInformation& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/location (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDVSerialPatch(
            bool dvserial __attribute__((unused)),
            SWGSDRangel::SWGDVSeralDevices& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/presets (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetsGet(
            SWGSDRangel::SWGPresets& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetPatch(
            SWGSDRangel::SWGPresetTransfer& query __attribute__((unused)),
            SWGSDRangel::SWGPresetIdentifier& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetPut(
            SWGSDRangel::SWGPresetTransfer& query __attribute__((unused)),
            SWGSDRangel::SWGPresetIdentifier& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetPost(
            SWGSDRangel::SWGPresetTransfer& query __attribute__((unused)),
            SWGSDRangel::SWGPresetIdentifier& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetDelete(
            SWGSDRangel::SWGPresetIdentifier& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset/file (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetFilePut(
            SWGSDRangel::SWGPresetImport& query __attribute__((unused)),
            SWGSDRangel::SWGPresetIdentifier& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset/file (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetFilePost(
            SWGSDRangel::SWGPresetExport& query __attribute__((unused)),
            SWGSDRangel::SWGPresetIdentifier& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/devicesets (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDeviceSetsGet(
            SWGSDRangel::SWGDeviceSetList& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDeviceSetPost(
            bool tx __attribute__((unused)),
            SWGSDRangel::SWGSuccessResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDeviceSetDelete(
            SWGSDRangel::SWGSuccessResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex} (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetGet(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGDeviceSet& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/focus (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetFocusPatch(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGSuccessResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDevicePut(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGDeviceListItem& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/settings (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceSettingsGet(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGDeviceSettings& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/settings (PUT, PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceSettingsPutPatch(
            int deviceSetIndex __attribute__((unused)),
            bool force __attribute__((unused)), //!< true to force settings = put else patch
            const QStringList& channelSettingsKeys __attribute__((unused)),
            SWGSDRangel::SWGDeviceSettings& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/run (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceRunGet(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/run (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceRunPost(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/run (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceRunDelete(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelPost(
            int deviceSetIndex __attribute__((unused)),
            SWGSDRangel::SWGChannelSettings& query __attribute__((unused)),
			SWGSDRangel::SWGSuccessResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex} (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelDelete(
            int deviceSetIndex __attribute__((unused)),
            int channelIndex __attribute__((unused)),
            SWGSDRangel::SWGSuccessResponse& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
    {
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/settings (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelSettingsGet(
            int deviceSetIndex __attribute__((unused)),
            int channelIndex __attribute__((unused)),
            SWGSDRangel::SWGChannelSettings& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/settings (PUT, PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelSettingsPutPatch(
            int deviceSetIndex __attribute__((unused)),
            int channelIndex __attribute__((unused)),
            bool force __attribute__((unused)),
            const QStringList& channelSettingsKeys __attribute__((unused)),
            SWGSDRangel::SWGChannelSettings& response __attribute__((unused)),
            SWGSDRangel::SWGErrorResponse& error)
    {
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    static QString instanceSummaryURL;
    static QString instanceDevicesURL;
    static QString instanceChannelsURL;
    static QString instanceLoggingURL;
    static QString instanceAudioURL;
    static QString instanceLocationURL;
    static QString instanceDVSerialURL;
    static QString instancePresetsURL;
    static QString instancePresetURL;
    static QString instancePresetFileURL;
    static QString instanceDeviceSetsURL;
    static QString instanceDeviceSetURL;
    static std::regex devicesetURLRe;
    static std::regex devicesetFocusURLRe;
    static std::regex devicesetDeviceURLRe;
    static std::regex devicesetDeviceSettingsURLRe;
    static std::regex devicesetDeviceRunURLRe;
    static std::regex devicesetChannelURLRe;
    static std::regex devicesetChannelIndexURLRe;
    static std::regex devicesetChannelSettingsURLRe;
};



#endif /* SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_ */
