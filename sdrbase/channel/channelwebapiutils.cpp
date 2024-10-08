///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
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

#include <QDebug>

#include "channelwebapiutils.h"

#include "SWGDeviceState.h"
#include "SWGErrorResponse.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceReport.h"
#include "SWGChannelSettings.h"
#include "SWGChannelActions.h"
#include "SWGFileSinkActions.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"

#include "maincore.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "device/deviceenumerator.h"
#include "channel/channelapi.h"
#include "channel/channelutils.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "webapi/webapiutils.h"
#include "feature/featureset.h"
#include "feature/feature.h"

bool ChannelWebAPIUtils::getDeviceSettings(unsigned int deviceIndex, SWGSDRangel::SWGDeviceSettings &deviceSettingsResponse, DeviceSet *&deviceSet)
{
    QString errorResponse;
    int httpRC;

    // Get current device settings
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getDeviceSettings - not a sample source device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getDeviceSettings - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getDeviceSettings: get device settings error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::getDeviceReport(unsigned int deviceIndex, SWGSDRangel::SWGDeviceReport &deviceReport)
{
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get device report
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceReport.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceReport.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiReportGet(deviceReport, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceReport.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceReport.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiReportGet(deviceReport, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceReport.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceReport.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiReportGet(deviceReport, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getDeviceReport: unknown device type " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getDeviceReport: no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getDeviceReport: get device report error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::getFeatureSettings(unsigned int featureSetIndex, unsigned int featureIndex, SWGSDRangel::SWGFeatureSettings &featureSettingsResponse, Feature *&feature)
{
    QString errorResponse;
    int httpRC;
    FeatureSet *featureSet;

    // Get current feature settings
    std::vector<FeatureSet*> featureSets = MainCore::instance()->getFeatureeSets();
    if (featureSetIndex < featureSets.size())
    {
        featureSet = featureSets[featureSetIndex];
        if (featureIndex < (unsigned int)featureSet->getNumberOfFeatures())
        {
            feature = featureSet->getFeatureAt(featureIndex);
            httpRC = feature->webapiSettingsGet(featureSettingsResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getFeatureSettings: no feature " << featureSetIndex << ":" << featureIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getFeatureSettings: no feature set " << featureSetIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getFeatureSettings: get feature settings error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::getFeatureReport(unsigned int featureSetIndex, unsigned int featureIndex, SWGSDRangel::SWGFeatureReport &featureReport)
{
    QString errorResponse;
    int httpRC;
    FeatureSet *featureSet;
    Feature *feature;

    // Get feature report
    std::vector<FeatureSet*> featureSets = MainCore::instance()->getFeatureeSets();
    if (featureSetIndex < featureSets.size())
    {
        featureSet = featureSets[featureSetIndex];
        if (featureIndex < (unsigned int)featureSet->getNumberOfFeatures())
        {
            feature = featureSet->getFeatureAt(featureIndex);
            httpRC = feature->webapiReportGet(featureReport, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getFeatureReport: no feature " << featureSetIndex << ":" << featureIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getFeatureReport: no feature set " << featureSetIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getFeatureReport: get feature report error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::getChannelSettings(unsigned int deviceIndex, unsigned int channelIndex, SWGSDRangel::SWGChannelSettings &channelSettingsResponse, ChannelAPI *&channel)
{
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get current channel settings
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (channelIndex < (unsigned int) deviceSet->getNumberOfChannels())
        {
            channel = deviceSet->getChannelAt(channelIndex);
            httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getChannelSettings: no channel " << deviceIndex << ":" << channelIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getChannelSettings: no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getChannelSettings: get channel settings error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::getChannelSettings(ChannelAPI *channel, SWGSDRangel::SWGChannelSettings &channelSettingsResponse)
{
    QString errorResponse;
    int httpRC;

    httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getChannelSettings: get channel settings error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::getChannelReport(unsigned int deviceIndex, unsigned int channelIndex, SWGSDRangel::SWGChannelReport &channelReport)
{
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get channel report
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (channelIndex < (unsigned int) deviceSet->getNumberOfChannels())
        {
            ChannelAPI *channel = deviceSet->getChannelAt(channelIndex);
            httpRC = channel->webapiReportGet(channelReport, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getChannelReport: no channel " << deviceIndex << ":" << channelIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getChannelReport: no device set " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getChannelReport: get channel report error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

QString ChannelWebAPIUtils::getDeviceHardwareId(unsigned int deviceIndex)
{
    DeviceAPI *deviceAPI = MainCore::instance()->getDevice(deviceIndex);
    if (deviceAPI) {
        return deviceAPI->getHardwareId();
    } else {
        return QString();
    }
}

// Get device center frequency
bool ChannelWebAPIUtils::getCenterFrequency(unsigned int deviceIndex, double &frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", frequencyInHz);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

// Set device center frequency
// Doesn't support MIMO devices. We'd need stream index parameter
bool ChannelWebAPIUtils::setCenterFrequency(unsigned int deviceIndex, double frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    int httpRC = 404;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        // Patch centerFrequency
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        double freq;
        if (WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", freq))
        {
            WebAPIUtils::setSubObjectDouble(*jsonObj, "centerFrequency", frequencyInHz);
            QStringList deviceSettingsKeys;
            deviceSettingsKeys.append("centerFrequency");
            deviceSettingsResponse.init();
            deviceSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;
            delete jsonObj;

            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            if (source) {
                httpRC = source->webapiSettingsPutPatch(false, deviceSettingsKeys, deviceSettingsResponse, *errorResponse2.getMessage());
            }
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            if (sink) {
                httpRC = sink->webapiSettingsPutPatch(false, deviceSettingsKeys, deviceSettingsResponse, *errorResponse2.getMessage());
            }

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::setCenterFrequency: set device frequency %f OK", frequencyInHz);
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::setCenterFrequency: set device frequency error %d: %s",
                    httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            qWarning("ChannelWebAPIUtils::setCenterFrequency: no centerFrequency key in device settings");
            return false;
        }
    }
    else
    {
        return false;
    }
}

// Get local oscillator frequency correction
bool ChannelWebAPIUtils::getLOPpmCorrection(unsigned int deviceIndex, int &ppmTenths)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if (devId == "RTLSDR") {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "loPpmCorrection", ppmTenths);
    } else {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "LOppmTenths", ppmTenths);
    }
}

// Set local oscillator frequency correction
bool ChannelWebAPIUtils::setLOPpmCorrection(unsigned int deviceIndex, int ppmTenths)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if (devId == "RTLSDR") {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "loPpmCorrection", ppmTenths);  // RTLSDR
    } else {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "LOppmTenths", ppmTenths);      // Airspy, AirspyHF. BladeRF2, FCDPro, HackRF, Metis, Perseus, Pluto, SDRPlay, SDRplayV3, SaopySDR
    }
}

// Get device sample rate
bool ChannelWebAPIUtils::getDevSampleRate(unsigned int deviceIndex, int &devSampleRate)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if (devId == "AirspyHF")
    {
        QList<int> rates;
        int idx;
        if (ChannelWebAPIUtils::getDeviceReportList(deviceIndex, "sampleRates", "rate", rates)
            && ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "devSampleRateIndex", idx))
        {
            if (idx < rates.size())
            {
                devSampleRate = rates[idx];
                return true;
            }
        }
        return false;
    }
    else
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "devSampleRate", devSampleRate);
    }
}

// Set device sample rate
bool ChannelWebAPIUtils::setDevSampleRate(unsigned int deviceIndex, int devSampleRate)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if (devId == "AirspyHF")
    {
        QList<int> rates;
        ChannelWebAPIUtils::getDeviceReportList(deviceIndex, "sampleRates", "rate", rates);
        // Find first rate that is big enough. Higher rates are in list first
        int idx = 0;
        for (int i = rates.size() - 1; i >= 0; i--)
        {
            if (devSampleRate <= rates[i])
            {
                idx = i;
                break;
            }
        }
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "devSampleRateIndex", idx);
    }
    else
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "devSampleRate", devSampleRate);
    }
}

bool ChannelWebAPIUtils::getGain(unsigned int deviceIndex, int stage, int &gain)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    bool error = true;
    if ((devId == "Airspy"))
    {
        QStringList airspyStages = {"lnaGain", "mixerGain", "vgaGain"};
        if (stage < airspyStages.size())
        {
            error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, airspyStages[stage], gain);
            gain *= 10;
        }
    }
    else if ((devId == "AirspyHF"))
    {
        if (stage == 0)
        {
            error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "attenuatorSteps", gain);
            gain *= 10 * 6;
        }
    }
    else if ((devId == "BladeRF1"))
    {
        QStringList bladeRF1Stages = {"lnaGain", "vga1", "vga2"};
        if (stage < bladeRF1Stages.size())
        {
            error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, bladeRF1Stages[stage], gain);
            gain *= 10;
        }
    }
    else if ((devId == "HackRF"))
    {
        QStringList hackRFStages = {"lnaGain", "vgaGain"};
        if (stage < hackRFStages.size())
        {
            error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, hackRFStages[stage], gain);
            gain *= 10;
        }
    }
    else if ((devId == "FCDProPlus") || (devId == "KiwiSDR") || (devId == "LimeSDR") || (devId == "PlutoSDR") || (devId == "USRP") || (devId == "XTRX"))
    {
        if (stage == 0)
        {
            error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "gain", gain);
            gain *= 10;
        }
    }
    else if ((devId == "SDRplayV3"))
    {
        QStringList sdrplayStages = {"lnaGain", "ifGain"};
        if (stage < sdrplayStages.size())
        {
            error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, sdrplayStages[stage], gain);
            gain *= 10;
        }
    }
    else if ((devId == "RTLSDR"))
    {
        if (stage == 0)
        {
            error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "gain", gain);
        }
    }
    return error;
}

// Set gain for different stages in RF device
// Gain is generally in 10ths of a dB
// Stages should correspond with order in RemoteTCPInputGUI
bool ChannelWebAPIUtils::setGain(unsigned int deviceIndex, int stage, int gain)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if (devId == "Airspy")
    {
        QStringList airspyStages = {"lnaGain", "mixerGain", "vgaGain"};
        if (stage < airspyStages.size()) {
            return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, airspyStages[stage], gain / 10);
        }
    }
    else if (devId == "AirspyHF")
    {
        if (stage == 0) {
            return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "attenuatorSteps", gain / 10 / 6);
        }
    }
    else if (devId == "BladeRF1")
    {
        QStringList bladeRF1Stages = {"lnaGain", "vga1", "vga2"};
        if (stage < bladeRF1Stages.size()) {
            return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, bladeRF1Stages[stage], gain / 10);
        }
    }
    else if (devId == "HackRF")
    {
        QStringList hackRFStages = {"lnaGain", "vgaGain"};
        if (stage < hackRFStages.size()) {
            return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, hackRFStages[stage], gain / 10);
        }
    }
    else if ((devId == "FCDProPlus") || (devId == "KiwiSDR") || (devId == "LimeSDR") || (devId == "PlutoSDR") || (devId == "USRP") || (devId == "XTRX"))
    {
        if (stage == 0) {
            return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "gain", gain / 10);
        }
    }
    else if (devId == "SDRplayV3")
    {
        QStringList sdrplayStages = {"lnaGain", "ifGain"};
        if (stage < sdrplayStages.size()) {
            return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, sdrplayStages[stage], gain / 10);
        }
    }
    else if ((devId == "RTLSDR"))
    {
        if (stage == 0) {
            return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "gain", gain);
        }
    }
    return false;
}

bool ChannelWebAPIUtils::getAGC(unsigned int deviceIndex, int &enabled)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if ((devId == "Airspy"))
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "lnaAGC", enabled); // What about mixerAGC?
    }
    else if ((devId == "AirspyHF") || (devId == "KiwiSDR"))
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "useAGC", enabled);
    }
    else if ((devId == "LimeSDR") || (devId == "PlutoSDR") || (devId == "USRP") || (devId == "XTRX"))
    {
        bool error = ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "gainMode", enabled);
        enabled = !enabled;
        return error;
    }
    else if (devId == "RTLSDR")
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "agc", enabled);
    }
    else if (devId == "SDRplayV3")
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "ifAGC", enabled);
    }
    return false;
}

bool ChannelWebAPIUtils::setAGC(unsigned int deviceIndex, bool enabled)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if ((devId == "Airspy"))
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "lnaAGC", enabled)
            && ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "mixerAGC", enabled);
    }
    else if ((devId == "AirspyHF") || (devId == "KiwiSDR"))
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "useAGC", enabled);
    }
    else if ((devId == "LimeSDR") || (devId == "PlutoSDR") || (devId == "USRP") || (devId == "XTRX"))
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "gainMode", !enabled);
    }
    else if (devId == "RTLSDR")
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "agc", enabled);
    }
    else if (devId == "SDRplayV3")
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "ifAGC", enabled);
    }
    return false;
}

// Get RF bandwidth
bool ChannelWebAPIUtils::getRFBandwidth(unsigned int deviceIndex, int &bw)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if (devId == "RTLSDR")
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "rfBandwidth", bw);
    }
    else if ((devId == "BladeRF1") || (devId == "HackRF"))
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "bandwidth", bw);
    }
    else if (devId == "SDRplayV3")
    {
        QList<int> bws;
        int idx;

        if (ChannelWebAPIUtils::getDeviceReportList(deviceIndex, "bandwidths", "bandwidth", bws)
            && ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "bandwidthIndex", idx))
        {
            if (idx < bws.size())
            {
                bw = bws[idx];
                return true;
            }
        }
        return false;
    }
    else
    {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "lpfBW", bw);
    }
}

// Set RF bandwidth
bool ChannelWebAPIUtils::setRFBandwidth(unsigned int deviceIndex, int bw)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if (devId == "RTLSDR")
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "rfBandwidth", bw); // RTLSDR
    }
    else if ((devId == "BladeRF1") || (devId == "HackRF"))
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "bandwidth", bw);   // BladeRF1, HackRF
    }
    else if (devId == "SDRplayV3")
    {
        QList<int> bws;
        ChannelWebAPIUtils::getDeviceReportList(deviceIndex, "bandwidths", "bandwidth", bws);
        // Find first b/w that is big enough
        int idx = 0;
        for (int i = 0; i < bws.size(); i++)
        {
            if (bw <= bws[i]) {
                break;
            }
            idx++;
        }
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "bandwidthIndex", idx);
    }
    else
    {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "lpfBW", bw); // LimeSDR, PlutoSDR, USRP
    }

    // TODO: SDRPlay and SDRPlayV3 use bandwidthIndex
}

// Get software decimation
bool ChannelWebAPIUtils::getSoftDecim(unsigned int deviceIndex, int &log2Decim)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if ((devId == "LimeSDR") || (devId == "USRP")) {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "log2SoftDecim", log2Decim);
    } else {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "log2Decim", log2Decim);
    }
}

// Set software decimation
bool ChannelWebAPIUtils::setSoftDecim(unsigned int deviceIndex, int log2Decim)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if ((devId == "LimeSDR") || (devId == "USRP")) {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "log2SoftDecim", log2Decim);  // LimeSDR, USRP
    } else {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "log2Decim", log2Decim);
    }
}

// Get whether bias tee is enabled
bool ChannelWebAPIUtils::getBiasTee(unsigned int deviceIndex, int &enabled)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if ((devId == "RTLSDR") || (devId == "BladeRF") || (devId == "SDRplayV3")) {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "biasTee", enabled);
    } else {
        return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "biasT", enabled);
    }
}

// Set whether bias tee, if available, should be enabled
bool ChannelWebAPIUtils::setBiasTee(unsigned int deviceIndex, bool enabled)
{
    QString devId = ChannelWebAPIUtils::getDeviceHardwareId(deviceIndex);
    if ((devId == "RTLSDR") || (devId == "BladeRF") || (devId == "SDRplayV3")) {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "biasTee", enabled);  // RTLSDR, BladeRF, SDRplayV3
    } else {
        return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "biasT", enabled);    // Airspy, FCDProPlus, HackRF,
    }
}

// Get whether DC offset removal is enabled
bool ChannelWebAPIUtils::getDCOffsetRemoval(unsigned int deviceIndex, int &enabled)
{
    return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "dcBlock", enabled);
}

// Set whether DC offset removal should be enabled
bool ChannelWebAPIUtils::setDCOffsetRemoval(unsigned int deviceIndex, bool enabled)
{
    return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "dcBlock", enabled);
}

// Get whether IQ correction is enabled
bool ChannelWebAPIUtils::getIQCorrection(unsigned int deviceIndex, int &enabled)
{
    return ChannelWebAPIUtils::getDeviceSetting(deviceIndex, "iqCorrection", enabled);
}

// Set whether IQ correction should be enabled
bool ChannelWebAPIUtils::setIQCorrection(unsigned int deviceIndex, bool enabled)
{
    return ChannelWebAPIUtils::patchDeviceSetting(deviceIndex, "iqCorrection", enabled);
}

// Start acquisition
bool ChannelWebAPIUtils::run(unsigned int deviceIndex, int subsystemIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        runResponse.setState(new QString());
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiRun(1, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiRun(1, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiRun(1, subsystemIndex, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::run - unknown device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::run - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::run: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

// Stop acquisition
bool ChannelWebAPIUtils::stop(unsigned int deviceIndex, int subsystemIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        runResponse.setState(new QString());
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiRun(0, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiRun(0, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiRun(0, subsystemIndex, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::stop - unknown device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::stop - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::stop: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

// Get input frequency offset for a channel
bool ChannelWebAPIUtils::getFrequencyOffset(unsigned int deviceIndex, int channelIndex, int& offset)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    QJsonObject *jsonObj;
    double offsetD;

    ChannelAPI *channel = MainCore::instance()->getChannel(deviceIndex, channelIndex);
    if (channel != nullptr)
    {
        httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        if (httpRC/100 != 2)
        {
            qWarning("ChannelWebAPIUtils::getFrequencyOffset: get channel settings error %d: %s",
                httpRC, qPrintable(errorResponse));
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectDouble(*jsonObj, "inputFrequencyOffset", offsetD);
        delete jsonObj;
        if (result)
        {
            offset = (int)offsetD;
            return true;
        }
    }
    return false;
}

// Set input frequency offset for a channel
bool ChannelWebAPIUtils::setFrequencyOffset(unsigned int deviceIndex, int channelIndex, int offset)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    QJsonObject *jsonObj;

    ChannelAPI *channel = MainCore::instance()->getChannel(deviceIndex, channelIndex);
    if (channel != nullptr)
    {
        httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        if (httpRC/100 != 2)
        {
            qWarning("ChannelWebAPIUtils::setFrequencyOffset: get channel settings error %d: %s",
                httpRC, qPrintable(errorResponse));
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();

        if (WebAPIUtils::setSubObjectDouble(*jsonObj, "inputFrequencyOffset", (double)offset))
        {
            QStringList keys;
            keys.append("inputFrequencyOffset");
            channelSettingsResponse.init();
            channelSettingsResponse.fromJsonObject(*jsonObj);
            delete jsonObj;
            httpRC = channel->webapiSettingsPutPatch(false, keys, channelSettingsResponse, errorResponse);
            if (httpRC/100 != 2)
            {
                qWarning("ChannelWebAPIUtils::setFrequencyOffset: patch channel settings error %d: %s",
                    httpRC, qPrintable(errorResponse));
                return false;
            }

            return true;
        }
        delete jsonObj;
    }
    return false;
}

bool ChannelWebAPIUtils::setAudioMute(unsigned int deviceIndex, int channelIndex, bool mute)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    QJsonObject* jsonObj;

    ChannelAPI* channel = MainCore::instance()->getChannel(deviceIndex, channelIndex);
    if (channel != nullptr)
    {
        httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        if (httpRC / 100 != 2)
        {
            qWarning("ChannelWebAPIUtils::setAudioMute: get channel settings error %d: %s",
                httpRC, qPrintable(errorResponse));
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();

        if (WebAPIUtils::setSubObjectInt(*jsonObj, "audioMute", (int)mute))
        {
            QStringList keys;
            keys.append("audioMute");
            channelSettingsResponse.init();
            channelSettingsResponse.fromJsonObject(*jsonObj);
            delete jsonObj;
            httpRC = channel->webapiSettingsPutPatch(false, keys, channelSettingsResponse, errorResponse);
            if (httpRC / 100 != 2)
            {
                qWarning("ChannelWebAPIUtils::setAudioMute: patch channel settings error %d: %s",
                    httpRC, qPrintable(errorResponse));
                return false;
            }

            return true;
        }
        delete jsonObj;
    }
    return false;
}


// Start or stop all file sinks in a given device set
bool ChannelWebAPIUtils::startStopFileSinks(unsigned int deviceIndex, bool start)
{
    MainCore *mainCore = MainCore::instance();
    ChannelAPI *channel;
    int channelIndex = 0;
    while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
    {
        if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.filesink"))
        {
            QStringList channelActionKeys = {"record"};
            SWGSDRangel::SWGChannelActions channelActions;
            SWGSDRangel::SWGFileSinkActions *fileSinkAction = new SWGSDRangel::SWGFileSinkActions();
            QString errorResponse;
            int httpRC;

            fileSinkAction->setRecord(start);
            channelActions.setFileSinkActions(fileSinkAction);
            httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
            if (httpRC/100 != 2)
            {
                qWarning("ChannelWebAPIUtils::startStopFileSinks: webapiActionsPost error %d: %s",
                    httpRC, qPrintable(errorResponse));
                return false;
            }
        }
        channelIndex++;
    }
    return true;
}

// Send AOS actions to all channels that support it
// See also: FeatureWebAPIUtils::satelliteAOS
bool ChannelWebAPIUtils::satelliteAOS(const QString name, bool northToSouthPass, const QString &tle, QDateTime dateTime)
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*> deviceSets = mainCore->getDeviceSets();
    for (unsigned int deviceIndex = 0; deviceIndex < deviceSets.size(); deviceIndex++)
    {
        ChannelAPI *channel;
        int channelIndex = 0;
        while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
        {
            if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.aptdemod"))
            {
                QStringList channelActionKeys = {"aos"};
                SWGSDRangel::SWGChannelActions channelActions;
                SWGSDRangel::SWGAPTDemodActions *aptDemodAction = new SWGSDRangel::SWGAPTDemodActions();
                SWGSDRangel::SWGAPTDemodActions_aos *aosAction = new SWGSDRangel::SWGAPTDemodActions_aos();
                QString errorResponse;
                int httpRC;

                aosAction->setSatelliteName(new QString(name));
                aosAction->setNorthToSouthPass(northToSouthPass);
                aosAction->setTle(new QString(tle));
                aosAction->setDateTime(new QString(dateTime.toString(Qt::ISODateWithMs)));
                aptDemodAction->setAos(aosAction);

                channelActions.setAptDemodActions(aptDemodAction);
                httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
                if (httpRC/100 != 2)
                {
                    qWarning("ChannelWebAPIUtils::satelliteAOS: webapiActionsPost error %d: %s",
                        httpRC, qPrintable(errorResponse));
                    return false;
                }
            }
            channelIndex++;
        }
    }
    return true;
}

// Send LOS actions to all channels that support it
bool ChannelWebAPIUtils::satelliteLOS(const QString name)
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*> deviceSets = mainCore->getDeviceSets();
    for (unsigned int deviceIndex = 0; deviceIndex < deviceSets.size(); deviceIndex++)
    {
        ChannelAPI *channel;
        int channelIndex = 0;
        while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
        {
            if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.aptdemod"))
            {
                QStringList channelActionKeys = {"los"};
                SWGSDRangel::SWGChannelActions channelActions;
                SWGSDRangel::SWGAPTDemodActions *aptDemodAction = new SWGSDRangel::SWGAPTDemodActions();
                SWGSDRangel::SWGAPTDemodActions_los *losAction = new SWGSDRangel::SWGAPTDemodActions_los();
                QString errorResponse;
                int httpRC;

                losAction->setSatelliteName(new QString(name));
                aptDemodAction->setLos(losAction);

                channelActions.setAptDemodActions(aptDemodAction);
                httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
                if (httpRC/100 != 2)
                {
                    qWarning("ChannelWebAPIUtils::satelliteLOS: webapiActionsPost error %d: %s",
                        httpRC, qPrintable(errorResponse));
                    return false;
                }
            }
            channelIndex++;
        }
    }
    return true;
}

bool ChannelWebAPIUtils::getDeviceSetting(unsigned int deviceIndex, const QString &setting, int &value)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectInt(*jsonObj, setting, value);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getDeviceReportValue(unsigned int deviceIndex, const QString &key, QString &value)
{
    SWGSDRangel::SWGDeviceReport deviceReport;

    if (getDeviceReport(deviceIndex, deviceReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = deviceReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectString(*jsonObj, key, value);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getDeviceReportValue: no key %s in device report", qPrintable(key));
            return false;
        }
    }
    return false;
}

bool ChannelWebAPIUtils::getDeviceReportList(unsigned int deviceIndex, const QString &key, const QString &subKey, QList<int> &values)
{
    SWGSDRangel::SWGDeviceReport deviceReport;

    if (getDeviceReport(deviceIndex, deviceReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = deviceReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectIntList(*jsonObj, key, subKey, values);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getDeviceReportList: no key %s in device report", qPrintable(key));
            return false;
        }
    }
    return false;
}


bool ChannelWebAPIUtils::getDevicePosition(unsigned int deviceIndex, float& latitude, float& longitude, float& altitude)
{
    SWGSDRangel::SWGDeviceReport deviceReport;

    if (getDeviceReport(deviceIndex, deviceReport))
    {
        QJsonObject *jsonObj = deviceReport.asJsonObject();
        double latitudeDouble, longitudeDouble, altitudeDouble;
        bool result = WebAPIUtils::getSubObjectDouble(*jsonObj, "latitude", latitudeDouble)
            && WebAPIUtils::getSubObjectDouble(*jsonObj, "longitude", longitudeDouble)
            && WebAPIUtils::getSubObjectDouble(*jsonObj, "altitude", altitudeDouble);
        delete jsonObj;
        if (result)
        {
            if (!std::isnan(latitudeDouble) && !std::isnan(longitudeDouble) && !std::isnan(altitudeDouble))
            {
                latitude = (float) latitudeDouble;
                longitude = (float) longitudeDouble;
                altitude = (float) altitudeDouble;
                return true;
            }
        }
    }
    return false;
}

bool ChannelWebAPIUtils::runFeature(unsigned int featureSetIndex, unsigned int featureIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    FeatureSet *featureSet;
    Feature *feature;

    std::vector<FeatureSet*> featureSets = MainCore::instance()->getFeatureeSets();
    if (featureSetIndex < featureSets.size())
    {
        runResponse.setState(new QString());
        featureSet = featureSets[featureSetIndex];

        if (featureIndex < (unsigned int)featureSet->getNumberOfFeatures())
        {
            feature = featureSet->getFeatureAt(featureIndex);

            httpRC = feature->webapiRun(1, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::runFeature - no feature " << featureSetIndex << ':' << featureIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::runFeature - no feature set " << featureSetIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::runFeature: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::stopFeature(unsigned int featureSetIndex, unsigned int featureIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    FeatureSet *featureSet;
    Feature *feature;

    std::vector<FeatureSet*> featureSets = MainCore::instance()->getFeatureeSets();
    if (featureSetIndex < featureSets.size())
    {
        runResponse.setState(new QString());
        featureSet = featureSets[featureSetIndex];

        if (featureIndex < (unsigned int)featureSet->getNumberOfFeatures())
        {
            feature = featureSet->getFeatureAt(featureIndex);

            httpRC = feature->webapiRun(0, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::stopFeature - no feature " << featureSetIndex << ':' << featureIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::stopFeature - no feature set " << featureSetIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::stopFeature: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::patchDeviceSetting(unsigned int deviceIndex, const QString &setting, int value)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        // Patch setting
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        int oldValue;
        if (WebAPIUtils::getSubObjectInt(*jsonObj, setting, oldValue))
        {
            WebAPIUtils::setSubObjectInt(*jsonObj, setting, value);
            QStringList deviceSettingsKeys;
            deviceSettingsKeys.append(setting);
            deviceSettingsResponse.init();
            deviceSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;
            delete jsonObj;

            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();

            httpRC = source->webapiSettingsPutPatch(false, deviceSettingsKeys, deviceSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::patchDeviceSetting: set device setting %s OK", qPrintable(setting));
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::patchDeviceSetting: set device setting error %d: %s",
                    httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            delete jsonObj;
            qWarning("ChannelWebAPIUtils::patchDeviceSetting: no key %s in device settings", qPrintable(setting));
            return false;
        }
    }
    else
    {
        return false;
    }
}

// Set feature setting
bool ChannelWebAPIUtils::patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, const QString &value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    int httpRC;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        // Patch settings
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();
        QString oldValue;
        if (WebAPIUtils::getSubObjectString(*jsonObj, setting, oldValue))
        {
            WebAPIUtils::setSubObjectString(*jsonObj, setting, value);
            QStringList featureSettingsKeys;
            featureSettingsKeys.append(setting);
            featureSettingsResponse.init();
            featureSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;
            delete jsonObj;

            httpRC = feature->webapiSettingsPutPatch(false, featureSettingsKeys, featureSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %s OK", qPrintable(setting), qPrintable(value));
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %s error %d: %s",
                    qPrintable(setting), qPrintable(value), httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            delete jsonObj;
            qWarning("ChannelWebAPIUtils::patchFeatureSetting: no key %s in feature settings", qPrintable(setting));
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, double value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    QString errorResponse;
    int httpRC;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        // Patch settings
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();
        double oldValue;
        if (WebAPIUtils::getSubObjectDouble(*jsonObj, setting, oldValue))
        {
            WebAPIUtils::setSubObjectDouble(*jsonObj, setting, value);
            QStringList featureSettingsKeys;
            featureSettingsKeys.append(setting);
            featureSettingsResponse.init();
            featureSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;
            delete jsonObj;

            httpRC = feature->webapiSettingsPutPatch(false, featureSettingsKeys, featureSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %f OK", qPrintable(setting), value);
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %f error %d: %s",
                    qPrintable(setting), value, httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            delete jsonObj;
            qWarning("ChannelWebAPIUtils::patchFeatureSetting: no key %s in feature settings", qPrintable(setting));
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, const QJsonArray& value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    QString errorResponse;
    int httpRC;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        // Patch settings
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();

        // Set value
        bool found = false;
        for (QJsonObject::iterator it = jsonObj->begin(); it != jsonObj->end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                QJsonObject subObject = jsonValue.toObject();

                if (subObject.contains(setting))
                {
                    subObject[setting] = value;
                    it.value() = subObject;
                    found = true;
                    break;
                }
            }
        }
        if (!found)
        {
            for (QJsonObject::iterator it = jsonObj->begin(); it != jsonObj->end(); it++)
            {
                QJsonValueRef jsonValue = it.value();

                if (jsonValue.isObject())
                {
                    QJsonObject subObject = jsonValue.toObject();

                    subObject.insert(setting, value);
                    jsonValue = subObject;
                }
            }
        }

        QStringList featureSettingsKeys;
        featureSettingsKeys.append(setting);
        featureSettingsResponse.init();
        featureSettingsResponse.fromJsonObject(*jsonObj);
        SWGSDRangel::SWGErrorResponse errorResponse2;
        delete jsonObj;

        httpRC = feature->webapiSettingsPutPatch(false, featureSettingsKeys, featureSettingsResponse, *errorResponse2.getMessage());

        if (httpRC/100 == 2)
        {
            qDebug("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s OK", qPrintable(setting));
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s error %d: %s",
                qPrintable(setting), httpRC, qPrintable(*errorResponse2.getMessage()));
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::patchChannelSetting(ChannelAPI *channel, const QString &setting, const QVariant& value)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;

    if (getChannelSettings(channel, channelSettingsResponse))
    {
        // Patch settings
        QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();
        if (WebAPIUtils::hasSubObject(*jsonObj, setting))
        {
            WebAPIUtils::setSubObject(*jsonObj, setting, value);
            QStringList channelSettingsKeys;
            channelSettingsKeys.append(setting);
            channelSettingsResponse.init();
            channelSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;
            delete jsonObj;

            httpRC = channel->webapiSettingsPutPatch(false, channelSettingsKeys, channelSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::patchChannelSetting: set feature setting %s to %s OK", qPrintable(setting), qPrintable(value.toString()));
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::patchChannelSetting: set feature setting %s to %s error %d: %s",
                    qPrintable(setting), qPrintable(value.toString()), httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            delete jsonObj;
            qWarning("ChannelWebAPIUtils::patchChannelSetting: no key %s in channel settings", qPrintable(setting));
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::patchChannelSetting(unsigned int deviceSetIndex, unsigned int channelIndex, const QString &setting, const QString& value)
{
    ChannelAPI *channel = MainCore::instance()->getChannel(deviceSetIndex, channelIndex);

    if (channel) {
        return patchChannelSetting(channel, setting, value);
    } else {
        return false;
    }
}

bool ChannelWebAPIUtils::patchChannelSetting(unsigned int deviceSetIndex, unsigned int channelIndex, const QString &setting, int value)
{
    ChannelAPI *channel = MainCore::instance()->getChannel(deviceSetIndex, channelIndex);

    if (channel) {
        return patchChannelSetting(channel, setting, value);
    } else {
        return false;
    }
}

bool ChannelWebAPIUtils::patchChannelSetting(unsigned int deviceSetIndex, unsigned int channelIndex, const QString &setting, double value)
{
    ChannelAPI *channel = MainCore::instance()->getChannel(deviceSetIndex, channelIndex);

    if (channel) {
        return patchChannelSetting(channel, setting, value);
    } else {
        return false;
    }
}

bool ChannelWebAPIUtils::patchChannelSetting(unsigned int deviceSetIndex, unsigned int channelIndex, const QString &setting, const QJsonArray& value)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    ChannelAPI *channel;

    if (getChannelSettings(deviceSetIndex, channelIndex, channelSettingsResponse, channel))
    {
        // Patch setting
        QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();

        // Set value
        bool found = false;
        for (QJsonObject::iterator it = jsonObj->begin(); it != jsonObj->end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                QJsonObject subObject = jsonValue.toObject();

                if (subObject.contains(setting))
                {
                    subObject[setting] = value;
                    it.value() = subObject;
                    found = true;
                    break;
                }
            }
        }
        if (!found)
        {
            for (QJsonObject::iterator it = jsonObj->begin(); it != jsonObj->end(); it++)
            {
                QJsonValueRef jsonValue = it.value();

                if (jsonValue.isObject())
                {
                    QJsonObject subObject = jsonValue.toObject();

                    subObject.insert(setting, value);
                    jsonValue = subObject;
                }
            }
        }

        QStringList channelSettingsKeys;
        channelSettingsKeys.append(setting);
        channelSettingsResponse.init();
        channelSettingsResponse.fromJsonObject(*jsonObj);
        SWGSDRangel::SWGErrorResponse errorResponse2;
        delete jsonObj;

        httpRC = channel->webapiSettingsPutPatch(false, channelSettingsKeys, channelSettingsResponse, *errorResponse2.getMessage());

        if (httpRC/100 == 2)
        {
            qDebug("ChannelWebAPIUtils::patchChannelSetting: set channel setting %s OK", qPrintable(setting));
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::patchChannelSetting: set channel setting error %d: %s",
                httpRC, qPrintable(*errorResponse2.getMessage()));
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getFeatureSetting(unsigned int featureSetIndex,  unsigned int featureIndex, const QString &setting, int &value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectInt(*jsonObj, setting, value);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getFeatureSetting(unsigned int featureSetIndex,  unsigned int featureIndex, const QString &setting, double &value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectDouble(*jsonObj, setting, value);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getFeatureSetting(unsigned int featureSetIndex,  unsigned int featureIndex, const QString &setting, QString &value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectString(*jsonObj, setting, value);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getChannelSetting(unsigned int deviceSetIndex,  unsigned int channelIndex, const QString &setting, int &value)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    ChannelAPI *channel;

    if (getChannelSettings(deviceSetIndex, channelIndex, channelSettingsResponse, channel))
    {
        QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectInt(*jsonObj, setting, value);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getChannelSetting(unsigned int deviceSetIndex,  unsigned int channelIndex, const QString &setting, double &value)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    ChannelAPI *channel;

    if (getChannelSettings(deviceSetIndex, channelIndex, channelSettingsResponse, channel))
    {
        QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectDouble(*jsonObj, setting, value);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getChannelSetting(unsigned int deviceSetIndex,  unsigned int channelIndex, const QString &setting, QString &value)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    ChannelAPI *channel;

    if (getChannelSettings(deviceSetIndex, channelIndex, channelSettingsResponse, channel))
    {
        QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();
        bool result = WebAPIUtils::getSubObjectString(*jsonObj, setting, value);
        delete jsonObj;
        return result;
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getFeatureReportValue(unsigned int featureSetIndex, unsigned int featureIndex, const QString &key, int &value)
{
    SWGSDRangel::SWGFeatureReport featureReport;

    if (getFeatureReport(featureSetIndex, featureIndex, featureReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = featureReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectInt(*jsonObj, key, value);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getFeatureReportValue: no key %s in feature report", qPrintable(key));
            return false;
        }
    }
    return false;
}

bool ChannelWebAPIUtils::getFeatureReportValue(unsigned int featureSetIndex, unsigned int featureIndex, const QString &key, double &value)
{
    SWGSDRangel::SWGFeatureReport featureReport;

    if (getFeatureReport(featureSetIndex, featureIndex, featureReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = featureReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectDouble(*jsonObj, key, value);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getFeatureReportValue: no key %s in feature report", qPrintable(key));
            return false;
        }
    }
    return false;
}

bool ChannelWebAPIUtils::getFeatureReportValue(unsigned int featureSetIndex, unsigned int featureIndex, const QString &key, QString &value)
{
    SWGSDRangel::SWGFeatureReport featureReport;

    if (getFeatureReport(featureSetIndex, featureIndex, featureReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = featureReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectString(*jsonObj, key, value);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getFeatureReportValue: no key %s in feature report", qPrintable(key));
            return false;
        }
    }
    return false;
}


bool ChannelWebAPIUtils::getChannelReportValue(unsigned int deviceIndex, unsigned int channelIndex, const QString &key, int &value)
{
    SWGSDRangel::SWGChannelReport channelReport;

    if (getChannelReport(deviceIndex, channelIndex, channelReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = channelReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectInt(*jsonObj, key, value);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getChannelReportValue: no key %s in channel report", qPrintable(key));
            return false;
        }
    }
    return false;
}

bool ChannelWebAPIUtils::getChannelReportValue(unsigned int deviceIndex, unsigned int channelIndex, const QString &key, double &value)
{
    SWGSDRangel::SWGChannelReport channelReport;

    if (getChannelReport(deviceIndex, channelIndex, channelReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = channelReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectDouble(*jsonObj, key, value);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getChannelReportValue: no key %s in channel report", qPrintable(key));
            return false;
        }
    }
    return false;
}

bool ChannelWebAPIUtils::getChannelReportValue(unsigned int deviceIndex, unsigned int channelIndex, const QString &key, QString &value)
{
    SWGSDRangel::SWGChannelReport channelReport;

    if (getChannelReport(deviceIndex, channelIndex, channelReport))
    {
        // Get value of requested key
        QJsonObject *jsonObj = channelReport.asJsonObject();
        bool result = WebAPIUtils::getSubObjectString(*jsonObj, key, value);
        delete jsonObj;
        if (result)
        {
            // Done
            return true;
        }
        else
        {
            qWarning("ChannelWebAPIUtils::getChannelReportValue: no key %s in channel report", qPrintable(key));
            return false;
        }
    }
    return false;
}

bool ChannelWebAPIUtils::addChannel(unsigned int deviceSetIndex, const QString& uri, int direction)
{
    MainCore *mainCore = MainCore::instance();
    PluginAPI::ChannelRegistrations *channelRegistrations = mainCore->getPluginManager()->getRxChannelRegistrations();
    int nbRegistrations = channelRegistrations->size();
    int index = 0;

    for (; index < nbRegistrations; index++)
    {
        if (channelRegistrations->at(index).m_channelIdURI == uri) {
            break;
        }
    }

    if (index < nbRegistrations)
    {
        MainCore::MsgAddChannel *msg = MainCore::MsgAddChannel::create(deviceSetIndex, index, direction);
        mainCore->getMainMessageQueue()->push(msg);

        return true;
    }
    else
    {
        qWarning() << "ChannelWebAPIUtils::addChannel:" << uri << "plugin not available";
        return false;
    }
}

// response will be deleted after device is opened.
bool ChannelWebAPIUtils::addDevice(const QString hwType, int direction, const QStringList& settingsKeys, SWGSDRangel::SWGDeviceSettings *response)
{
    return DeviceOpener::open(hwType, direction, settingsKeys, response);
}

DeviceOpener::DeviceOpener(int deviceIndex, int direction, const QStringList& settingsKeys, SWGSDRangel::SWGDeviceSettings *response) :
    m_deviceIndex(deviceIndex),
    m_direction(direction),
    m_settingsKeys(settingsKeys),
    m_response(response),
    m_device(nullptr)
{
    connect(MainCore::instance(), &MainCore::deviceSetAdded, this, &DeviceOpener::deviceSetAdded);
    // Create DeviceSet
    MainCore *mainCore = MainCore::instance();
    m_deviceSetIndex = mainCore->getDeviceSets().size();
    MainCore::MsgAddDeviceSet *msg = MainCore::MsgAddDeviceSet::create(m_direction);
    mainCore->getMainMessageQueue()->push(msg);
}

void DeviceOpener::deviceSetAdded(int index, DeviceAPI *device)
{
    if (index == m_deviceSetIndex)
    {
        disconnect(MainCore::instance(), &MainCore::deviceSetAdded, this, &DeviceOpener::deviceSetAdded);

        m_device = device;
        // Set the correct device type
        MainCore::MsgSetDevice *msg = MainCore::MsgSetDevice::create(m_deviceSetIndex, m_deviceIndex, m_direction);
        MainCore::instance()->getMainMessageQueue()->push(msg);
        // Wait until device has initialised - FIXME: Better way to do this other than polling?
        m_timer.setInterval(250);
        connect(&m_timer, &QTimer::timeout, this, &DeviceOpener::checkInitialised);
        m_timer.start();
    }
}

void DeviceOpener::checkInitialised()
{
    if (m_device && m_device->getSampleSource() && (m_device->state() >= DeviceAPI::EngineState::StIdle))
    {
        m_timer.stop();

        QString errorMessage;
        if (200 != m_device->getSampleSource()->webapiSettingsPutPatch(false, m_settingsKeys, *m_response, errorMessage)) {
            qDebug() << "DeviceOpener::checkInitialised: webapiSettingsPutPatch failed: " << errorMessage;
        }

        delete m_response;
        delete this;
    }
}

bool DeviceOpener::open(const QString hwType, int direction, const QStringList& settingsKeys, SWGSDRangel::SWGDeviceSettings *response)
{
    if (direction) {
        return false; // FIXME: Only RX support for now
    }

    int nbSamplingDevices = DeviceEnumerator::instance()->getNbRxSamplingDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        const PluginInterface::SamplingDevice *samplingDevice;

        samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(i);

        if (!hwType.isEmpty() && (hwType != samplingDevice->hardwareId)) {
            continue;
        }

        new DeviceOpener(i, direction, settingsKeys, response);

        return true;
    }

    qWarning() << "DeviceOpener::open: Failed to find device with hwType " << hwType;
    return false;
}
