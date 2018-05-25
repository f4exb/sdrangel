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

#include "webapiadapterinterface.h"

QString WebAPIAdapterInterface::instanceSummaryURL = "/sdrangel";
QString WebAPIAdapterInterface::instanceDevicesURL = "/sdrangel/devices";
QString WebAPIAdapterInterface::instanceChannelsURL = "/sdrangel/channels";
QString WebAPIAdapterInterface::instanceLoggingURL = "/sdrangel/logging";
QString WebAPIAdapterInterface::instanceAudioURL = "/sdrangel/audio";
QString WebAPIAdapterInterface::instanceAudioInputParametersURL = "/sdrangel/audio/input/parameters";
QString WebAPIAdapterInterface::instanceAudioOutputParametersURL = "/sdrangel/audio/output/parameters";
QString WebAPIAdapterInterface::instanceAudioInputCleanupURL = "/sdrangel/audio/input/cleanup";
QString WebAPIAdapterInterface::instanceAudioOutputCleanupURL = "/sdrangel/audio/output/cleanup";
QString WebAPIAdapterInterface::instanceLocationURL = "/sdrangel/location";
QString WebAPIAdapterInterface::instanceDVSerialURL = "/sdrangel/dvserial";
QString WebAPIAdapterInterface::instancePresetsURL = "/sdrangel/presets";
QString WebAPIAdapterInterface::instancePresetURL = "/sdrangel/preset";
QString WebAPIAdapterInterface::instancePresetFileURL = "/sdrangel/preset/file";
QString WebAPIAdapterInterface::instanceDeviceSetsURL = "/sdrangel/devicesets";
QString WebAPIAdapterInterface::instanceDeviceSetURL = "/sdrangel/deviceset";

std::regex WebAPIAdapterInterface::devicesetURLRe("^/sdrangel/deviceset/([0-9]{1,2})$");
std::regex WebAPIAdapterInterface::devicesetFocusURLRe("^/sdrangel/deviceset/([0-9]{1,2})/focus$");
std::regex WebAPIAdapterInterface::devicesetDeviceURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device$");
std::regex WebAPIAdapterInterface::devicesetDeviceSettingsURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/settings$");
std::regex WebAPIAdapterInterface::devicesetDeviceRunURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/run");
std::regex WebAPIAdapterInterface::devicesetDeviceReportURLRe("^/sdrangel/deviceset/([0-9]{1,2})/device/report$");
std::regex WebAPIAdapterInterface::devicesetChannelsReportURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channels/report$");
std::regex WebAPIAdapterInterface::devicesetChannelURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel$");
std::regex WebAPIAdapterInterface::devicesetChannelIndexURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})$");
std::regex WebAPIAdapterInterface::devicesetChannelSettingsURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})/settings$");
std::regex WebAPIAdapterInterface::devicesetChannelReportURLRe("^/sdrangel/deviceset/([0-9]{1,2})/channel/([0-9]{1,2})/report");
