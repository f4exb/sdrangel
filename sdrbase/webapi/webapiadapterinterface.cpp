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

#include "webapiadapterinterface.h"

QString WebAPIAdapterInterface::instanceSummaryURL = "/sdrangel";
QString WebAPIAdapterInterface::instanceConfigURL = "/sdrangel/config";
QString WebAPIAdapterInterface::instanceDevicesURL = "/sdrangel/devices";
QString WebAPIAdapterInterface::instanceChannelsURL = "/sdrangel/channels";
QString WebAPIAdapterInterface::instanceFeaturesURL = "/sdrangel/features";
QString WebAPIAdapterInterface::instanceLoggingURL = "/sdrangel/logging";
QString WebAPIAdapterInterface::instanceAudioURL = "/sdrangel/audio";
QString WebAPIAdapterInterface::instanceAudioInputParametersURL = "/sdrangel/audio/input/parameters";
QString WebAPIAdapterInterface::instanceAudioOutputParametersURL = "/sdrangel/audio/output/parameters";
QString WebAPIAdapterInterface::instanceAudioInputCleanupURL = "/sdrangel/audio/input/cleanup";
QString WebAPIAdapterInterface::instanceAudioOutputCleanupURL = "/sdrangel/audio/output/cleanup";
QString WebAPIAdapterInterface::instanceLocationURL = "/sdrangel/location";
QString WebAPIAdapterInterface::instancePresetsURL = "/sdrangel/presets";
QString WebAPIAdapterInterface::instancePresetURL = "/sdrangel/preset";
QString WebAPIAdapterInterface::instancePresetFileURL = "/sdrangel/preset/file";
QString WebAPIAdapterInterface::instancePresetBlobURL = "/sdrangel/preset/blob";
QString WebAPIAdapterInterface::instanceConfigurationsURL = "/sdrangel/configurations";
QString WebAPIAdapterInterface::instanceConfigurationURL = "/sdrangel/configuration";
QString WebAPIAdapterInterface::instanceConfigurationFileURL = "/sdrangel/configuration/file";
QString WebAPIAdapterInterface::instanceConfigurationBlobURL = "/sdrangel/configuration/blob";
QString WebAPIAdapterInterface::instanceFeaturePresetsURL = "/sdrangel/featurepresets";
QString WebAPIAdapterInterface::instanceFeaturePresetURL = "/sdrangel/featurepreset";
QString WebAPIAdapterInterface::instanceDeviceSetsURL = "/sdrangel/devicesets";
QString WebAPIAdapterInterface::instanceDeviceSetURL = "/sdrangel/deviceset";
QString WebAPIAdapterInterface::instanceWorkspaceURL = "/sdrangel/workspace";
QString WebAPIAdapterInterface::featuresetURL("/sdrangel/featureset");
QString WebAPIAdapterInterface::featuresetFeatureURL("/sdrangel/featureset/feature");
QString WebAPIAdapterInterface::featuresetPresetURL("/sdrangel/featureset/preset");

std::regex WebAPIAdapterInterface::devicesetURLRe("^/sdrangel/deviceset/([0-9]{1,2})$");
std::regex WebAPIAdapterInterface::devicesetSpectrumSettingsURLRe("^/sdrangel/deviceset/([0-9]{1,2})/spectrum/settings$");
std::regex WebAPIAdapterInterface::devicesetSpectrumServerURLRe("^/sdrangel/deviceset/([0-9]{1,2})/spectrum/server$");
std::regex WebAPIAdapterInterface::devicesetSpectrumWorkspaceURLRe("^/sdrangel/deviceset/([0-9]{1,2})/spectrum/workspace$");
std::regex WebAPIAdapterInterface::devicesetDeviceURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device$");
std::regex WebAPIAdapterInterface::devicesetDeviceSettingsURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/settings$");
std::regex WebAPIAdapterInterface::devicesetDeviceRunURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/run$");
std::regex WebAPIAdapterInterface::devicesetDeviceSubsystemRunURLRe("^/sdrangel/deviceset/([0-9]{1,2})/subdevice/([0-9]{1,2})/run$");
std::regex WebAPIAdapterInterface::devicesetDeviceReportURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/report$");
std::regex WebAPIAdapterInterface::devicesetDeviceActionsURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/actions$");
std::regex WebAPIAdapterInterface::devicesetDeviceWorkspaceURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/workspace$");
std::regex WebAPIAdapterInterface::devicesetChannelsReportURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channels/report$");
std::regex WebAPIAdapterInterface::devicesetChannelURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel$");
std::regex WebAPIAdapterInterface::devicesetChannelIndexURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})$");
std::regex WebAPIAdapterInterface::devicesetChannelSettingsURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})/settings$");
std::regex WebAPIAdapterInterface::devicesetChannelReportURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})/report");
std::regex WebAPIAdapterInterface::devicesetChannelActionsURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})/actions");
std::regex WebAPIAdapterInterface::devicesetChannelWorkspaceURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})/workspace");

std::regex WebAPIAdapterInterface::featuresetFeatureIndexURLRe("^/sdrangel/featureset/feature/([0-9]{1,2})$");
std::regex WebAPIAdapterInterface::featuresetFeatureRunURLRe("^/sdrangel/featureset/feature/([0-9]{1,2})/run$");
std::regex WebAPIAdapterInterface::featuresetFeatureSettingsURLRe("^/sdrangel/featureset/feature/([0-9]{1,2})/settings$");
std::regex WebAPIAdapterInterface::featuresetFeatureReportURLRe("^/sdrangel/featureset/feature/([0-9]{1,2})/report$");
std::regex WebAPIAdapterInterface::featuresetFeatureActionsURLRe("^/sdrangel/featureset/feature/([0-9]{1,2})/actions$");
std::regex WebAPIAdapterInterface::featuresetFeatureWorkspaceURLRe("^/sdrangel/featureset/feature/([0-9]{1,2})/workspace$");

void WebAPIAdapterInterface::ConfigKeys::debug() const
{
    qDebug("WebAPIAdapterInterface::ConfigKeys::debug");

    qDebug("preferences:");
    foreach(QString preferenceKey, m_preferencesKeys) {
        qDebug("  %s", qPrintable(preferenceKey));
    }

    qDebug("commands:");
    foreach(CommandKeys commandKeys, m_commandKeys)
    {
        qDebug("  {");
        foreach(QString commandKey, commandKeys.m_keys) {
            qDebug("    %s", qPrintable(commandKey));
        }
        qDebug("  }");
    }

    qDebug("presets:");
    foreach(PresetKeys presetKeys, m_presetKeys)
    {
        qDebug("  {");
        foreach(QString presetKey, presetKeys.m_keys) {
            qDebug("    %s", qPrintable(presetKey));
        }
        qDebug("    spectrumConfig:");
        foreach(QString spectrumKey, presetKeys.m_spectrumKeys) {
            qDebug("      %s", qPrintable(spectrumKey));
        }
        qDebug("    deviceConfigs:");
        foreach(DeviceKeys deviceKeys, presetKeys.m_devicesKeys)
        {
            qDebug("      {");
            qDebug("        config:");
            foreach(QString deviceKey, deviceKeys.m_deviceKeys) {
                qDebug("          %s", qPrintable(deviceKey));
            }
            qDebug("      }");
        }
        qDebug("    channelConfigs");
        foreach(ChannelKeys channelKeys, presetKeys.m_channelsKeys)
        {
            qDebug("      {");
            qDebug("        config:");
            foreach(QString channelKey, channelKeys.m_channelKeys) {
                qDebug("          %s", qPrintable(channelKey));
            }
            qDebug("      }");
        }
        qDebug("  }");
    }

    qDebug("featuresets:");
    foreach(FeatureSetPresetKeys presetKeys, m_featureSetPresetKeys)
    {
        qDebug("  {");
        foreach(QString presetKey, presetKeys.m_keys) {
            qDebug("    %s", qPrintable(presetKey));
        }
        qDebug("    featureConfigs");
        foreach(FeatureKeys featureKeys, presetKeys.m_featureKeys)
        {
            qDebug("      {");
            qDebug("        config:");
            foreach(QString featureKey, featureKeys.m_featureKeys) {
                qDebug("          %s", qPrintable(featureKey));
            }
            qDebug("      }");
        }
        qDebug("  }");
    }

    qDebug("workingPreset:");
    foreach(QString presetKey, m_workingPresetKeys.m_keys) {
        qDebug("  %s", qPrintable(presetKey));
    }
    qDebug("workingFeatureSetPreset:");
    foreach(QString presetKey, m_workingFeatureSetPresetKeys.m_keys) {
        qDebug("  %s", qPrintable(presetKey));
    }
    qDebug("  spectrumConfig:");
    foreach(QString spectrumKey, m_workingPresetKeys.m_spectrumKeys) {
        qDebug("    %s", qPrintable(spectrumKey));
    }
    qDebug("  deviceConfigs:");
    foreach(DeviceKeys deviceKeys, m_workingPresetKeys.m_devicesKeys)
    {
        qDebug("    {");
        qDebug("      config:");
        foreach(QString deviceKey, deviceKeys.m_deviceKeys) {
            qDebug("        %s", qPrintable(deviceKey));
        }
        qDebug("    }");
    }
    qDebug("  channelConfigs:");
    foreach(ChannelKeys channelKeys, m_workingPresetKeys.m_channelsKeys)
    {
        qDebug("    {");
        qDebug("      config:");
        foreach(QString channelKey, channelKeys.m_channelKeys) {
            qDebug("        %s", qPrintable(channelKey));
        }
        qDebug("    }");
    }
}
