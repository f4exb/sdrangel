///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef SDRBASE_CHANNEL_CHANNELWEBAPIUTILS_H_
#define SDRBASE_CHANNEL_CHANNELWEBAPIUTILS_H_

#include <QString>
#include <QJsonArray>
#include <QTimer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceReport.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"

#include "export.h"

class DeviceSet;
class Feature;
class ChannelAPI;
class DeviceAPI;

// Use ChannelWebAPIUtils::addDevice rather than this directly
class DeviceOpener : public QObject {
    Q_OBJECT
protected:
    DeviceOpener(int deviceIndex, int direction, const QStringList& settingsKeys, SWGSDRangel::SWGDeviceSettings *response);
private:
    int m_deviceIndex;
    int m_direction;
    int m_deviceSetIndex;
    QStringList m_settingsKeys;
    SWGSDRangel::SWGDeviceSettings *m_response;
    DeviceAPI *m_device;
    QTimer m_timer;

private slots:
    void deviceSetAdded(int index, DeviceAPI *device);
    void checkInitialised();
public:
    static bool open(const QString hwType, int direction, const QStringList& settingsKeys, SWGSDRangel::SWGDeviceSettings *response);
};

class SDRBASE_API ChannelWebAPIUtils
{
public:
    static bool getCenterFrequency(unsigned int deviceIndex, double &frequencyInHz);
    static bool setCenterFrequency(unsigned int deviceIndex, double frequencyInHz);
    static bool getLOPpmCorrection(unsigned int deviceIndex, int &ppmTenths);
    static bool setLOPpmCorrection(unsigned int deviceIndex, int ppmTenths);
    static bool getDevSampleRate(unsigned int deviceIndex, int &sampleRate);
    static bool setDevSampleRate(unsigned int deviceIndex, int sampleRate);
    static bool getGain(unsigned int deviceIndex, int stage, int &gain);
    static bool setGain(unsigned int deviceIndex, int stage, int gain);
    static bool getAGC(unsigned int deviceIndex, int &enabled);
    static bool setAGC(unsigned int deviceIndex, bool enabled);
    static bool getRFBandwidth(unsigned int deviceIndex, int &bw);
    static bool setRFBandwidth(unsigned int deviceIndex, int bw);
    static bool getSoftDecim(unsigned int deviceIndex, int &log2Decim);
    static bool setSoftDecim(unsigned int deviceIndex, int log2Decim);
    static bool getBiasTee(unsigned int deviceIndex, int &enabled);
    static bool setBiasTee(unsigned int deviceIndex, bool enabled);
    static bool getDCOffsetRemoval(unsigned int deviceIndex, int &enabled);
    static bool setDCOffsetRemoval(unsigned int deviceIndex, bool enabled);
    static bool getIQCorrection(unsigned int deviceIndex, int &enabled);
    static bool setIQCorrection(unsigned int deviceIndex, bool enabled);
    static bool run(unsigned int deviceIndex, int subsystemIndex=0);
    static bool stop(unsigned int deviceIndex, int subsystemIndex=0);
    static bool getFrequencyOffset(unsigned int deviceIndex, int channelIndex, int& offset);
    static bool setFrequencyOffset(unsigned int deviceIndex, int channelIndex, int offset);
    static bool setAudioMute(unsigned int deviceIndex, int channelIndex, bool mute);
    static bool startStopFileSinks(unsigned int deviceIndex, bool start);
    static bool satelliteAOS(const QString name, bool northToSouthPass, const QString &tle, QDateTime dateTime);
    static bool satelliteLOS(const QString name);
    static bool getDeviceSetting(unsigned int deviceIndex, const QString &setting, int &value);
    static bool getDeviceReportValue(unsigned int deviceIndex, const QString &key, QString &value);
    static bool getDeviceReportList(unsigned int deviceIndex, const QString &key, const QString &subKey, QList<int> &values);
    static bool getDevicePosition(unsigned int deviceIndex, float& latitude, float& longitude, float& altitude);
    static bool patchDeviceSetting(unsigned int deviceIndex, const QString &setting, int value);
    static bool runFeature(unsigned int featureSetIndex, unsigned int featureIndex);
    static bool stopFeature(unsigned int featureSetIndex, unsigned int featureIndex);
    static bool patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, const QString &value);
    static bool patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, double value);
    static bool patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, const QJsonArray& value);
    static bool patchChannelSetting(ChannelAPI *channel, const QString &setting, const QVariant &value);
    static bool patchChannelSetting(unsigned int deviceSetIndex, unsigned int channeIndex, const QString &setting, const QString &value);
    static bool patchChannelSetting(unsigned int deviceSetIndex, unsigned int channeIndex, const QString &setting, int value);
    static bool patchChannelSetting(unsigned int deviceSetIndex, unsigned int channeIndex, const QString &setting, double value);
    static bool patchChannelSetting(unsigned int deviceSetIndex, unsigned int channeIndex, const QString &setting, const QJsonArray& value);
    static bool getFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, int &value);
    static bool getFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, double &value);
    static bool getFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, QString &value);
    static bool getChannelSetting(unsigned int deviceSetIndex, unsigned int channelIndex, const QString &setting, int &value);
    static bool getChannelSetting(unsigned int deviceSetIndex, unsigned int channelIndex, const QString &setting, double &value);
    static bool getChannelSetting(unsigned int deviceSetIndex, unsigned int channelIndex, const QString &setting, QString &value);
    static bool getFeatureReportValue(unsigned int featureSetIndex, unsigned int featureIndex, const QString &key, int &value);
    static bool getFeatureReportValue(unsigned int featureSetIndex, unsigned int featureIndex, const QString &key, double &value);
    static bool getFeatureReportValue(unsigned int featureSetIndex, unsigned int featureIndex, const QString &key, QString &value);
    static bool getChannelReportValue(unsigned int deviceIndex, unsigned int channelIndex, const QString &key, int &value);
    static bool getChannelReportValue(unsigned int deviceIndex, unsigned int channelIndex, const QString &key, double &value);
    static bool getChannelReportValue(unsigned int deviceIndex, unsigned int channelIndex, const QString &key, QString &value);
    static bool getDeviceSettings(unsigned int deviceIndex, SWGSDRangel::SWGDeviceSettings &deviceSettingsResponse, DeviceSet *&deviceSet);
    static bool getDeviceReport(unsigned int deviceIndex, SWGSDRangel::SWGDeviceReport &deviceReport);
    static bool getFeatureSettings(unsigned int featureSetIndex, unsigned int featureIndex, SWGSDRangel::SWGFeatureSettings &featureSettingsResponse, Feature *&feature);
    static bool getFeatureReport(unsigned int featureSetIndex, unsigned int featureIndex, SWGSDRangel::SWGFeatureReport &featureReport);
    static bool getChannelSettings(unsigned int deviceIndex, unsigned int channelIndex, SWGSDRangel::SWGChannelSettings &channelSettingsResponse, ChannelAPI *&channel);
    static bool getChannelSettings(ChannelAPI *channel, SWGSDRangel::SWGChannelSettings &channelSettingsResponse);
    static bool getChannelReport(unsigned int deviceIndex, unsigned int channelIndex, SWGSDRangel::SWGChannelReport &channelReport);
    static bool addChannel(unsigned int deviceSetIndex, const QString& uri, int direction);
    static bool addDevice(const QString hwType, int direction, const QStringList& settingsKeys, SWGSDRangel::SWGDeviceSettings *response);
protected:
    static QString getDeviceHardwareId(unsigned int deviceIndex);
};

#endif // SDRBASE_CHANNEL_CHANNELWEBAPIUTILS_H_

