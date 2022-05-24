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

#ifndef SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_
#define SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_

#include <QString>
#include <QStringList>
#include <regex>

#include "SWGErrorResponse.h"

#include "export.h"

namespace SWGSDRangel
{
    class SWGInstanceSummaryResponse;
    class SWGInstanceConfigResponse;
    class SWGInstanceDevicesResponse;
    class SWGInstanceChannelsResponse;
    class SWGInstanceFeaturesResponse;
    class SWGPreferences;
    class SWGLoggingInfo;
    class SWGAudioDevices;
    class SWGAudioInputDevice;
    class SWGAudioOutputDevice;
    class SWGLocationInformation;
    class SWGLimeRFEDevices;
    class SWGLimeRFESettings;
    class SWGLimeRFEPower;
    class SWGPresets;
    class SWGPresetTransfer;
    class SWGPresetIdentifier;
    class SWGPresetImport;
    class SWGPresetExport;
    class SWGPresetDeserialize;
    class SWGPresetSerialize;
    class SWGBase64Blob;
    class SWGFilePath;
    class SWGConfigurations;
    class SWGConfigurationIdentifier;
    class SWGConfigurationImportExport;
    class SWGConfigurationDeserialize;
    class SWGDeviceSetList;
    class SWGDeviceSet;
    class SWGDeviceListItem;
    class SWGDeviceSettings;
    class SWGDeviceState;
    class SWGDeviceReport;
    class SWGDeviceActions;
    class SWGWorkspaceInfo;
    class SWGChannelsDetail;
    class SWGChannelSettings;
    class SWGChannelReport;
    class SWGChannelActions;
    class SWGSuccessResponse;
    class SWGFeaturePresets;
    class SWGFeaturePresetIdentifier;
    class SWGFeaturePresetTransfer;
    class SWGFeatureSetList;
    class SWGFeatureSet;
    class SWGFeatureSettings;
    class SWGFeatureReport;
    class SWGFeatureActions;
    class SWGGLSpectrum;
    class SWGSpectrumServer;
}

class SDRBASE_API WebAPIAdapterInterface
{
public:
    struct ChannelKeys
    {
        QStringList m_keys;
        QStringList m_channelKeys;
    };
    struct DeviceKeys
    {
        QStringList m_keys;
        QStringList m_deviceKeys;
    };
    struct FeatureKeys
    {
        QStringList m_keys;
        QStringList m_featureKeys;
    };
    struct PresetKeys
    {
        QStringList m_keys;
        QStringList m_spectrumKeys;
        QList<ChannelKeys> m_channelsKeys;
        QList<DeviceKeys> m_devicesKeys;
    };
    struct FeatureSetPresetKeys
    {
        QStringList m_keys;
        QList<FeatureKeys> m_featureKeys;
        QList<DeviceKeys> m_devicesKeys;
    };
    struct CommandKeys
    {
        QStringList m_keys;
    };
    struct ConfigKeys
    {
        QStringList m_preferencesKeys;
        PresetKeys m_workingPresetKeys;
        FeatureSetPresetKeys m_workingFeatureSetPresetKeys;
        QList<PresetKeys> m_presetKeys;
        QList<FeatureSetPresetKeys> m_featureSetPresetKeys;
        QList<CommandKeys> m_commandKeys;
        void debug() const;
    };

    virtual ~WebAPIAdapterInterface() {}

    /**
     * Handler of /sdrangel (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceSummary
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceSummary(
            SWGSDRangel::SWGInstanceSummaryResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceSummary
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/config (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceSummary
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigGet(
            SWGSDRangel::SWGInstanceConfigResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/config (PUT, PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceSummary
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigPutPatch(
        bool force, // PUT else PATCH
        SWGSDRangel::SWGInstanceConfigResponse& query,
        const ConfigKeys& configKeys,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error
    )
    {
        (void) force;
        (void) query;
        (void) configKeys;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/devices (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceDevices
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDevices(
            int direction,
            SWGSDRangel::SWGInstanceDevicesResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) direction;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/channels (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceChannels(
            int direction,
            SWGSDRangel::SWGInstanceChannelsResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) direction;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/features (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceFeatures(
            SWGSDRangel::SWGInstanceFeaturesResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/logging (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLoggingGet(
            SWGSDRangel::SWGLoggingInfo& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/logging (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLoggingPut(
            SWGSDRangel::SWGLoggingInfo& query,
            SWGSDRangel::SWGLoggingInfo& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/audio (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioGet(
            SWGSDRangel::SWGAudioDevices& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/audio/input/parameters (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioInputPatch(
            SWGSDRangel::SWGAudioInputDevice& response,
            const QStringList& audioInputKeys,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
        (void) audioInputKeys;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/audio/output/parameters (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioOutputPatch(
            SWGSDRangel::SWGAudioOutputDevice& response,
            const QStringList& audioOutputKeys,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
        (void) audioOutputKeys;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/audio/input/parameters (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioInputDelete(
            SWGSDRangel::SWGAudioInputDevice& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/audio/output/paramaters (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioOutputDelete(
            SWGSDRangel::SWGAudioOutputDevice& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/audio/input/cleanup (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioInputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/audio/output/cleanup (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioOutputCleanupPatch(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/location (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLocationGet(
            SWGSDRangel::SWGLocationInformation& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/location (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLocationPut(
            SWGSDRangel::SWGLocationInformation& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/limerfe/serial (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLimeRFESerialGet(
            SWGSDRangel::SWGLimeRFEDevices& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/limerfe/config (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLimeRFEConfigGet(
            const QString& serial,
            SWGSDRangel::SWGLimeRFESettings& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) serial;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/limerfe/config (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLimeRFEConfigPut(
            SWGSDRangel::SWGLimeRFESettings& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/limerfe/run (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLimeRFERunPut(
            SWGSDRangel::SWGLimeRFESettings& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/presets (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetsGet(
            SWGSDRangel::SWGPresets& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetPatch(
            SWGSDRangel::SWGPresetTransfer& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetPut(
            SWGSDRangel::SWGPresetTransfer& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetPost(
            SWGSDRangel::SWGPresetTransfer& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetDelete(
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset/file (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetFilePut(
            SWGSDRangel::SWGFilePath& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset/file (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetFilePost(
            SWGSDRangel::SWGPresetExport& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset/blob (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetBlobPut(
            SWGSDRangel::SWGBase64Blob& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/preset/blob (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instancePresetBlobPost(
            SWGSDRangel::SWGPresetIdentifier& query,
            SWGSDRangel::SWGBase64Blob& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/presets (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationsGet(
            SWGSDRangel::SWGConfigurations& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationPatch(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationPut(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationPost(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationDelete(
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration/file (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationFilePut(
            SWGSDRangel::SWGFilePath& query,
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration/file (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationFilePost(
            SWGSDRangel::SWGConfigurationImportExport& query,
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration/blob (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationBlobPut(
            SWGSDRangel::SWGBase64Blob& query,
            SWGSDRangel::SWGConfigurationIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/configuration/blob (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceConfigurationBlobPost(
            SWGSDRangel::SWGConfigurationIdentifier& query,
            SWGSDRangel::SWGBase64Blob& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featurepresets (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceFeaturePresetsGet(
            SWGSDRangel::SWGFeaturePresets& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featurepreset (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceFeaturePresetDelete(
            SWGSDRangel::SWGFeaturePresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/devicesets (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDeviceSetsGet(
            SWGSDRangel::SWGDeviceSetList& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/workspace (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceWorkspacePost(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/workspace (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceWorkspaceDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDeviceSetPost(
            int direction,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) direction;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDeviceSetDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featurets (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceFeatureSetsGet(
            SWGSDRangel::SWGFeatureSetList& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceFeatureSetPost(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceFeatureSetDelete(
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex} (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceSet& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/spectrum/settings (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetSpectrumSettingsGet(
            int deviceSetIndex,
            SWGSDRangel::SWGGLSpectrum& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/spectrum/settings (PUT, PATCH)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetSpectrumSettingsPutPatch(
            int deviceSetIndex,
            bool force, //!< true to force settings = put else patch
            const QStringList& spectrumSettingsKeys,
            SWGSDRangel::SWGGLSpectrum& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) force;
        (void) spectrumSettingsKeys;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/spectrum/server (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetSpectrumServerGet(
            int deviceSetIndex,
            SWGSDRangel::SWGSpectrumServer& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/spectrum/server (POST)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetSpectrumServerPost(
            int deviceSetIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/spectrum/server (DELETE)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetSpectrumServerDelete(
            int deviceSetIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/spectrum/workspace (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetSpectrumWorkspaceGet(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/spectrum/workspace (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetSpectrumWorkspacePut(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDevicePut(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceListItem& query,
            SWGSDRangel::SWGDeviceListItem& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) query;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/settings (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceSettingsGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/settings (PUT, PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceSettingsPutPatch(
            int deviceSetIndex,
            bool force, //!< true to force settings = put else patch
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) force;
        (void) channelSettingsKeys;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/run (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceRunGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/subdevice/{subsystemIndex}/run (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceSubsystemRunGet(
            int deviceSetIndex,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) subsystemIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/run (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceRunPost(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/subdevice/{subsystemIndex}/run (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceSubsystemRunPost(
            int deviceSetIndex,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) subsystemIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/run (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceRunDelete(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/subdevice/{subsystemIndex}/run (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceSubsystemRunDelete(
            int deviceSetIndex,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) subsystemIndex;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/report (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceReportGet(
            int deviceSetIndex,
            SWGSDRangel::SWGDeviceReport& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/channels/report (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelsReportGet(
            int deviceSetIndex,
            SWGSDRangel::SWGChannelsDetail& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/device/actions (POST)
     * post action(s) on device
     */
    virtual int devicesetDeviceActionsPost(
            int deviceSetIndex,
            const QStringList& deviceActionsKeys,
            SWGSDRangel::SWGDeviceActions& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error
    )
    {
        (void) deviceSetIndex;
        (void) deviceActionsKeys;
        (void) query;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/workspace (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceWorkspaceGet(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{devicesetIndex}/device/workspace (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetDeviceWorkspacePut(
            int deviceSetIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) query;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelPost(
            int deviceSetIndex,
            SWGSDRangel::SWGChannelSettings& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) query;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex} (DELETE) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelDelete(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) channelIndex;
        (void) response;
        (void) error;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/settings (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelSettingsGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) channelIndex;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/settings (PUT, PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelSettingsPutPatch(
            int deviceSetIndex,
            int channelIndex,
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) channelIndex;
        (void) force;
        (void) channelSettingsKeys;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }


    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/report (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelReportGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGChannelReport& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) channelIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/actions (POST)
     * posts an action on the channel (default 501: not implemented)
     */
    virtual int devicesetChannelActionsPost(
            int deviceSetIndex,
            int channelIndex,
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) channelIndex;
        (void) channelActionsKeys;
        (void) query;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/workspace (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelWorkspaceGet(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) channelIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/workspace (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int devicesetChannelWorkspacePut(
            int deviceSetIndex,
            int channelIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) deviceSetIndex;
        (void) channelIndex;
        (void) query;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex} (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetGet(
            int featureSetIndex,
            SWGSDRangel::SWGFeatureSet& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) response;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature (POST)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeaturePost(
            int featureSetIndex,
            SWGSDRangel::SWGFeatureSettings& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) query;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/run (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureDelete(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) response;
        (void) error;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/run (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureRunGet(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) response;
        (void) error;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/run (POST)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureRunPost(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) response;
        (void) error;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/run (DELETE)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureRunDelete(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) response;
        (void) error;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/preset (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetPresetPatch(
            int featureSetIndex,
            SWGSDRangel::SWGFeaturePresetIdentifier& query,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) query;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/preset (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetPresetPut(
            int featureSetIndex,
            SWGSDRangel::SWGFeaturePresetIdentifier& query,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) query;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/preset (POST) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetPresetPost(
            int featureSetIndex,
            SWGSDRangel::SWGFeaturePresetIdentifier& query,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) query;
    	error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/settings (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureSettingsGet(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGFeatureSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/settings (PUT, PATCH)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureSettingsPutPatch(
            int featureSetIndex,
            int featureIndex,
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) force;
        (void) featureSettingsKeys;
        (void) response;
        error.init();
    	*error.getMessage() = QString("Function not implemented");
    	return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/report (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureReportGet(
            int featureSetIndex,
            int featureIndex,
            SWGSDRangel::SWGFeatureReport& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/{featuresetIndex}/feature/{featureIndex}/actions (POST)
     * posts an action on the channel (default 501: not implemented)
     */
    virtual int featuresetFeatureActionsPost(
            int featureSetIndex,
            int featureIndex,
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureSetIndex;
        (void) featureIndex;
        (void) featureActionsKeys;
        (void) query;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/feature/{featureIndex}/workspace (GET)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureWorkspaceGet(
            int featureIndex,
            SWGSDRangel::SWGWorkspaceInfo& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureIndex;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    /**
     * Handler of /sdrangel/featureset/feature/{featureIndex}/workspace (PUT)
     * returns the Http status code (default 501: not implemented)
     */
    virtual int featuresetFeatureWorkspacePut(
            int featureIndex,
            SWGSDRangel::SWGWorkspaceInfo& query,
            SWGSDRangel::SWGSuccessResponse& response,
            SWGSDRangel::SWGErrorResponse& error)
    {
        (void) featureIndex;
        (void) query;
        (void) response;
        error.init();
        *error.getMessage() = QString("Function not implemented");
        return 501;
    }

    static QString instanceSummaryURL;
    static QString instanceConfigURL;
    static QString instanceDevicesURL;
    static QString instanceChannelsURL;
    static QString instanceFeaturesURL;
    static QString instanceLoggingURL;
    static QString instanceAudioURL;
    static QString instanceAudioInputParametersURL;
    static QString instanceAudioOutputParametersURL;
    static QString instanceAudioInputCleanupURL;
    static QString instanceAudioOutputCleanupURL;
    static QString instanceLocationURL;
    static QString instancePresetsURL;
    static QString instancePresetURL;
    static QString instancePresetFileURL;
    static QString instancePresetBlobURL;
    static QString instanceConfigurationsURL;
    static QString instanceConfigurationURL;
    static QString instanceConfigurationFileURL;
    static QString instanceConfigurationBlobURL;
    static QString instanceFeaturePresetsURL;
    static QString instanceFeaturePresetURL;
    static QString instanceDeviceSetsURL;
    static QString instanceDeviceSetURL;
    static QString instanceWorkspaceURL;
    static QString featuresetURL;
    static QString featuresetFeatureURL;
    static QString featuresetPresetURL;
    static std::regex devicesetURLRe;
    static std::regex devicesetSpectrumSettingsURLRe;
    static std::regex devicesetSpectrumServerURLRe;
    static std::regex devicesetSpectrumWorkspaceURLRe;
    static std::regex devicesetDeviceURLRe;
    static std::regex devicesetDeviceSettingsURLRe;
    static std::regex devicesetDeviceRunURLRe;
    static std::regex devicesetDeviceSubsystemRunURLRe;
    static std::regex devicesetDeviceReportURLRe;
    static std::regex devicesetDeviceActionsURLRe;
    static std::regex devicesetDeviceWorkspaceURLRe;
    static std::regex devicesetChannelURLRe;
    static std::regex devicesetChannelIndexURLRe;
    static std::regex devicesetChannelSettingsURLRe;
    static std::regex devicesetChannelReportURLRe;
    static std::regex devicesetChannelActionsURLRe;
    static std::regex devicesetChannelWorkspaceURLRe;
    static std::regex devicesetChannelsReportURLRe;
    static std::regex featuresetFeatureIndexURLRe;
    static std::regex featuresetFeatureRunURLRe;
    static std::regex featuresetFeatureSettingsURLRe;
    static std::regex featuresetFeatureReportURLRe;
    static std::regex featuresetFeatureActionsURLRe;
    static std::regex featuresetFeatureWorkspaceURLRe;
};



#endif /* SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_ */
