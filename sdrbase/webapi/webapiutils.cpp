///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QJsonArray>
#include <QList>

#include "webapiutils.h"

const QMap<QString, QString> WebAPIUtils::m_channelURIToSettingsKey = {
    {"sdrangel.channel.adsbdemod", "ADSBDemodSettings"},
    {"sdrangel.channel.modais", "AISModSettings"},
    {"sdrangel.channel.aisdemod", "AISDemodSettings"},
    {"sdrangel.channel.amdemod", "AMDemodSettings"},
    {"sdrangel.channel.aptdemod", "APTDemodSettings"},
    {"de.maintech.sdrangelove.channel.am", "AMDemodSettings"}, // remap
    {"sdrangel.channeltx.modam", "AMModSettings"},
    {"sdrangel.channeltx.modatv", "ATVModSettings"},
    {"sdrangel.channeltx.moddatv", "DATVModSettings"},
    {"sdrangel.channel.bfm", "BFMDemodSettings"},
    {"sdrangel.channel.chanalyzer", "ChannelAnalyzerSettings"},
    {"sdrangel.channel.chanalyzerng", "ChannelAnalyzerSettings"}, // remap
    {"org.f4exb.sdrangelove.channel.chanalyzer", "ChannelAnalyzerSettings"}, // remap
    {"sdrangel.channel.chirpchatdemod", "ChirpChatDemodSettings"},
    {"sdrangel.channel.modchirpchat", "ChirpChatModSettings"},
    {"sdrangel.channel.demodatv", "ATVDemodSettings"},
    {"sdrangel.channel.demoddatv", "DATVDemodSettings"},
    {"sdrangel.channel.dabdemod", "DABDemodSettings"},
    {"sdrangel.channel.doa2", "DOA2Settings"},
    {"sdrangel.channel.dsddemod", "DSDDemodSettings"},
    {"sdrangel.channel.filesink", "FileSinkSettings"},
    {"sdrangel.channeltx.filesource", "FileSourceSettings"},
    {"sdrangel.channel.freedvdemod", "FreeDVDemodSettings"},
    {"sdrangel.channeltx.freedvmod", "FreeDVModSettings"},
    {"sdrangel.channel.freqtracker", "FreqTrackerSettings"},
    {"sdrangel.channel.m17demod", "M17DemodSettings"},
    {"sdrangel.channeltx.modm17", "M17ModSettings"},
    {"sdrangel.channel.nfmdemod", "NFMDemodSettings"},
    {"de.maintech.sdrangelove.channel.nfm", "NFMDemodSettings"}, // remap
    {"sdrangel.channeltx.modnfm", "NFMModSettings"},
    {"sdrangel.channel.noisefigure", "NoiseFigureSettings"},
    {"sdrangel.demod.localsink", "LocalSinkSettings"},
    {"sdrangel.channel.localsink", "LocalSinkSettings"}, // remap
    {"sdrangel.channel.localsource", "LocalSourceSettings"},
    {"sdrangel.channel.packetdemod", "PacketDemodSettings"},
    {"sdrangel.channel.pagerdemod", "PagerDemodSettings"},
    {"sdrangel.channeltx.modpacket", "PacketModSettings"},
    {"sdrangel.channeltx.mod802.15.4", "IEEE_802_15_4_ModSettings"},
    {"sdrangel.channel.radioclock", "RadioClockSettings"},
    {"sdrangel.channel.radiosondedemod", "RadiosondeDemodSettings"},
    {"sdrangel.demod.remotesink", "RemoteSinkSettings"},
    {"sdrangel.demod.remotetcpsink", "RemoteTCPSinkSettings"},
    {"sdrangel.channeltx.remotesource", "RemoteSourceSettings"},
    {"sdrangel.channeltx.modssb", "SSBModSettings"},
    {"sdrangel.channel.ssbdemod", "SSBDemodSettings"},
    {"sdrangel.channel.ft8demod", "FT8DemodSettings"},
    {"de.maintech.sdrangelove.channel.ssb", "SSBDemodSettings"}, // remap
    {"sdrangel.channel.radioastronomy", "RadioAstronomySettings"},
    {"sdrangel.channeltx.udpsource", "UDPSourceSettings"},
    {"sdrangel.channeltx.udpsink", "UDPSinkSettings"}, // remap
    {"sdrangel.channel.udpsink", "UDPSinkSettings"},
    {"sdrangel.channel.udpsrc", "UDPSourceSettings"}, // remap
    {"sdrangel.channel.vordemodmc", "VORDemodMCSettings"},
    {"sdrangel.channel.vordemod", "VORDemodSettings"},
    {"sdrangel.channel.wfmdemod", "WFMDemodSettings"},
    {"de.maintech.sdrangelove.channel.wfm", "WFMDemodSettings"}, // remap
    {"sdrangel.channeltx.modwfm", "WFMModSettings"},
    {"sdrangel.channel.beamsteeringcwmod", "BeamSteeringCWModSettings"},
    {"sdrangel.channelmimo.interferometer", "InterferometerSettings"},
    {"sdrangel.channel.sigmffilesink", "SigMFFileSinkSettings"}
};

const QMap<QString, QString> WebAPIUtils::m_deviceIdToSettingsKey = {
    {"sdrangel.samplesource.airspy", "airspySettings"},
    {"sdrangel.samplesource.airspyhf", "airspyHFSettings"},
    {"sdrangel.samplesource.audioinput", "audioInputSettings"},
    {"sdrangel.samplesink.audiooutput", "audioOutputSettings"},
    {"sdrangel.samplesource.bladerf1input", "bladeRF1InputSettings"},
    {"sdrangel.samplesource.bladerf", "bladeRF1InputSettings"}, // remap
    {"sdrangel.samplesink.bladerf1output", "bladeRF1OutputSettings"},
    {"sdrangel.samplesource.bladerf1output", "bladeRF1OutputSettings"}, // remap
    {"sdrangel.samplesource.bladerfoutput", "bladeRF1OutputSettings"}, // remap
    {"sdrangel.samplesource.bladerf2input", "bladeRF2InputSettings"},
    {"sdrangel.samplesink.bladerf2output", "bladeRF2OutputSettings"},
    {"sdrangel.samplesource.bladerf2output", "bladeRF2OutputSettings"}, // remap
    {"sdrangel.samplemimo.bladerf2mimo", "bladeRF2MIMOSettings"},
    {"sdrangel.samplesource.fcdpro", "fcdProSettings"},
    {"sdrangel.samplesource.fcdproplus", "fcdProPlusSettings"},
    {"sdrangel.samplesource.fileinput", "fileInputSettings"},
    {"sdrangel.samplesource.filesource", "fileInputSettings"}, // remap
    {"sdrangel.samplesource.hackrf", "hackRFInputSettings"},
    {"sdrangel.samplesink.hackrf", "hackRFOutputSettings"},
    {"sdrangel.samplesource.hackrfoutput", "hackRFOutputSettings"}, // remap
    {"sdrangel.samplesource.kiwisdrsource", "kiwiSDRSettings"},
    {"sdrangel.samplesource.limesdr", "limeSdrInputSettings"},
    {"sdrangel.samplesink.limesdr", "limeSdrOutputSettings"},
    {"sdrangel.samplesource.localinput", "localInputSettings"},
    {"sdrangel.samplesink.localoutput", "localOutputSettings"},
    {"sdrangel.samplesource.localoutput", "localOutputSettings"}, // remap
    {"sdrangel.samplemimo.metismiso", "metisMISOSettings"},
    {"sdrangel.samplesource.perseus", "perseusSettings"},
    {"sdrangel.samplesource.plutosdr", "plutoSdrInputSettings"},
    {"sdrangel.samplesink.plutosdr", "plutoSdrOutputSettings"},
    {"sdrangel.samplesource.rtlsdr", "rtlSdrSettings"},
    {"sdrangel.samplesource.remoteinput", "remoteInputSettings"},
    {"sdrangel.samplesource.remotetcpinput", "remoteTCPInputSettings"},
    {"sdrangel.samplesink.remoteoutput", "remoteOutputSettings"},
    {"sdrangel.samplesource.sdrplay", "sdrPlaySettings"},
    {"sdrangel.samplesource.sdrplayv3", "sdrPlayV3Settings"},
    {"sdrangel.samplesource.sigmffileinput", "sigMFFileInputSettings"},
    {"sdrangel.samplesource.soapysdrinput", "soapySDRInputSettings"},
    {"sdrangel.samplesink.soapysdroutput", "soapySDROutputSettings"},
    {"sdrangel.samplesource.testsource", "testSourceSettings"},
    {"sdrangel.samplemimo.testmi", "testMISettings"},
    {"sdrangel.samplemimo.testmosync", "testMOSyncSettings"},
    {"sdrangel.samplesource.usrp", "usrpInputSettings"},
    {"sdrangel.samplesink.usrp", "usrpOutputSettings"},
    {"sdrangel.samplesource.xtrx", "xtrxInputSettings"},
    {"sdrangel.samplesink.xtrx", "xtrxOutputSettings"},
    {"sdrangel.samplemimo.xtrxmimo", "xtrxMIMOSettings"}
};

const QMap<QString, QString> WebAPIUtils::m_channelTypeToSettingsKey = {
    {"ADSBDemod", "ADSBDemodSettings"},
    {"AISDemod", "AISDemodSettings"},
    {"AISMod", "AISModSettings"},
    {"APTDemod", "APTDemodSettings"},
    {"AMDemod", "AMDemodSettings"},
    {"AMMod", "AMModSettings"},
    {"ATVDemod", "ATVDemodSettings"},
    {"ATVMod", "ATVModSettings"},
    {"BFMDemod", "BFMDemodSettings"},
    {"ChannelAnalyzer", "ChannelAnalyzerSettings"},
    {"ChirpChatDemod", "ChirpChatDemodSettings"},
    {"ChirpChatMod", "ChirpChatModSettings"},
    {"DATVDemod", "DATVDemodSettings"},
    {"DATVMod", "DATVModSettings"},
    {"DABDemod", "DABDemodSettings"},
    {"DOA2", "DOA2Settings"},
    {"DSDDemod", "DSDDemodSettings"},
    {"FileSink", "FileSinkSettings"},
    {"FileSource", "FileSourceSettings"},
    {"FreeDVDemod", "FreeDVDemodSettings"},
    {"FreeDVMod", "FreeDVModSettings"},
    {"FreqTracker", "FreqTrackerSettings"},
    {"IEEE_802_15_4_Mod", "IEEE_802_15_4_ModSettings"},
    {"M17Demod", "M17DemodSettings"},
    {"M17Mod", "M17ModSettings"},
    {"NFMDemod", "NFMDemodSettings"},
    {"NFMMod", "NFMModSettings"},
    {"NoiseFigure", "NoiseFigureSettings"},
    {"PacketDemod", "PacketDemodSettings"},
    {"PacketMod", "PacketModSettings"},
    {"PagerDemod", "PagerDemodSettings"},
    {"LocalSink", "LocalSinkSettings"},
    {"LocalSource", "LocalSourceSettings"},
    {"RadioAstronomy", "RadioAstronomySettings"},
    {"RadioClock", "RadioClockSettings"},
    {"RadiosondeDemod", "RadiosondeDemodSettings"},
    {"RemoteSink", "RemoteSinkSettings"},
    {"RemoteSource", "RemoteSourceSettings"},
    {"RemoteTCPSink", "RemoteTCPSinkSettings"},
    {"SSBMod", "SSBModSettings"},
    {"SSBDemod", "SSBDemodSettings"},
    {"FT8Demod", "FT8DemodSettings"},
    {"UDPSink", "UDPSinkSettings"},
    {"UDPSource", "UDPSourceSettings"},
    {"VORDemodMC", "VORDemodMCSettings"},
    {"VORDemod", "VORDemodSettings"},
    {"WFMDemod", "WFMDemodSettings"},
    {"WFMMod", "WFMModSettings"},
    {"BeamSteeringCWMod", "BeamSteeringCWModSettings"},
    {"Interferometer", "InterferometerSettings"},
    {"SigMFFileSink", "SigMFFileSinkSettings"}
};

const QMap<QString, QString> WebAPIUtils::m_channelTypeToActionsKey = {
    {"AISMod", "AISModActions"},
    {"APTDemod", "APTDemodActions"},
    {"FileSink", "FileSinkActions"},
    {"FileSource", "FileSourceActions"},
    {"SigMFFileSink", "SigMFFileSinkActions"},
    {"IEEE_802_15_4_Mod", "IEEE_802_15_4_ModActions"},
    {"RadioAstronomy", "RadioAstronomyActions"},
    {"PacketMod", "PacketModActions"}
};

const QMap<QString, QString> WebAPIUtils::m_sourceDeviceHwIdToSettingsKey = {
    {"Airspy", "airspySettings"},
    {"AirspyHF", "airspyHFSettings"},
    {"AudioInput", "audioInputSettings"},
    {"BladeRF1", "bladeRF1InputSettings"},
    {"BladeRF2", "bladeRF2InputSettings"},
    {"FCDPro", "fcdProSettings"},
    {"FCDPro+", "fcdProPlusSettings"},
    {"FileInput", "fileInputSettings"},
    {"HackRF", "hackRFInputSettings"},
    {"KiwiSDR", "kiwiSDRSettings"},
    {"LimeSDR", "limeSdrInputSettings"},
    {"LocalInput", "localInputSettings"},
    {"Perseus", "perseusSettings"},
    {"PlutoSDR", "plutoSdrInputSettings"},
    {"RTLSDR", "rtlSdrSettings"},
    {"RemoteInput", "remoteInputSettings"},
    {"RemoteTCPInput", "remoteTCPInputSettings"},
    {"SDRplay1", "sdrPlaySettings"},
    {"SDRplayV3", "sdrPlayV3Settings"},
    {"SigMFFileInput", "sigMFFileInputSettings"},
    {"SoapySDR", "soapySDRInputSettings"},
    {"TestSource", "testSourceSettings"},
    {"USRP", "usrpInputSettings"},
    {"XTRX", "xtrxInputSettings"}
};

const QMap<QString, QString> WebAPIUtils::m_sourceDeviceHwIdToActionsKey = {
    {"Airspy", "airspyActions"},
    {"AirspyHF", "airspyHFActions"},
    {"AudioInput", "audioInputActions"},
    {"BladeRF1", "bladeRF1InputActions"},
    {"FCDPro", "fcdProActions"},
    {"FCDPro+", "fcdProPlusActions"},
    {"HackRF", "hackRFInputActions"},
    {"KiwiSDR", "kiwiSDRActions"},
    {"LimeSDR", "limeSdrInputActions"},
    {"LocalInput", "localInputActions"},
    {"Perseus", "perseusActions"},
    {"PlutoSDR", "plutoSdrInputActions"},
    {"RemoteInput", "remoteInputActions"},
    {"RTLSDR", "rtlSdrActions"},
    {"SDRplay1", "sdrPlayActions"},
    {"SDRplayV3", "sdrPlayV3Actions"},
    {"SigMFFileInput", "sigMFFileActions"},
    {"SoapySDR", "soapySDRInputActions"},
    {"TestSource", "testSourceActions"},
    {"USRP", "usrpSourceActions"},
    {"XTRX", "xtrxInputActions"}
};

const QMap<QString, QString> WebAPIUtils::m_sinkDeviceHwIdToSettingsKey = {
    {"AudioOutput", "audioOutputSettings"},
    {"BladeRF1", "bladeRF1OutputSettings"},
    {"BladeRF2", "bladeRF2OutputSettings"},
    {"FileOutput", "fileOutputSettings"},
    {"HackRF", "hackRFOutputSettings"},
    {"LimeSDR", "limeSdrOutputSettings"},
    {"LocalOutput", "localOutputSettings"},
    {"PlutoSDR", "plutoSdrOutputSettings"},
    {"RemoteOutput", "remoteOutputSettings"},
    {"SoapySDR", "soapySDROutputSettings"},
    {"USRP", "usrpOutputSettings"},
    {"XTRX", "xtrxOutputSettings"}
};

const QMap<QString, QString> WebAPIUtils::m_sinkDeviceHwIdToActionsKey = {
};

const QMap<QString, QString> WebAPIUtils::m_mimoDeviceHwIdToSettingsKey = {
    {"BladeRF2", "bladeRF2MIMOSettings"},
    {"MetisMISO", "metisMISOSettings"},
    {"TestMI", "testMISettings"},
    {"TestMOSync", "testMOSyncSettings"},
    {"XTRX", "xtrxMIMOSettings"}
};

const QMap<QString, QString> WebAPIUtils::m_mimoDeviceHwIdToActionsKey = {
};

const QMap<QString, QString> WebAPIUtils::m_featureTypeToSettingsKey = {
    {"AFC", "AFCSettings"},
    {"AIS", "AISSettings"},
    {"AMBE", "AMBESettings"},
    {"AntennaTools", "AntennaToolsSettings"},
    {"APRS", "APRSSettings"},
    {"DemodAnalyzer", "DemodAnalyzerSettings"},
    {"JogdialController", "JogdialControllerSettings"},
    {"GS232Controller", "GS232ControllerSettings"}, // a.k.a Rotator Controller
    {"LimeRFE", "LimeRFESettings"},
    {"Map", "MapSettings"},
    {"PERTester", "PERTesterSettings"},
    {"Radiosonde", "RadiosondeSettings"},
    {"RigCtlServer", "RigCtlServerSettings"},
    {"SatelliteTracker", "SatelliteTrackerSettings"},
    {"SimplePTT", "SimplePTTSettings"},
    {"StarTracker", "StarTrackerSettings"},
    {"VORLocalizer", "VORLocalizerSettings"}
};

const QMap<QString, QString> WebAPIUtils::m_featureTypeToActionsKey = {
    {"AFC", "AFCActions"},
    {"AMBE", "AMBEActions"},
    {"GS232Controller", "GS232ControllerActions"},
    {"LimeRFE", "LimeRFEActions"},
    {"Map", "MapActions"},
    {"PERTester", "PERTesterActions"},
    {"RigCtlServer", "RigCtlServerActions"},
    {"SatelliteTracker", "SatelliteTrackerActions"},
    {"SimplePTT", "SimplePTTActions"},
    {"StarTracker", "StarTrackerActions"},
    {"VORLocalizer", "VORLocalizerActions"}
};

const QMap<QString, QString> WebAPIUtils::m_featureURIToSettingsKey = {
    {"sdrangel.feature.afc", "AFCSettings"},
    {"sdrangel.feature.ais", "AISSSettings"},
    {"sdrangel.feature.ambe", "AMBESSettings"},
    {"sdrangel.feature.antennatools", "AntennaToolsSettings"},
    {"sdrangel.feature.aprs", "APRSSettings"},
    {"sdrangel.feature.demodanalyzer", "DemodAnalyzerSettings"},
    {"sdrangel.feature.jogdialcontroller", "JogdialControllerSettings"},
    {"sdrangel.feature.gs232controller", "GS232ControllerSettings"},
    {"sdrangel.feature.limerfe", "LimeRFESettings"},
    {"sdrangel.feature.map", "MapSettings"},
    {"sdrangel.feature.pertester", "PERTesterSettings"},
    {"sdrangel.feature.radiosonde", "RadiosondeSettings"},
    {"sdrangel.feature.rigctlserver", "RigCtlServerSettings"},
    {"sdrangel.feature.satellitetracker", "SatelliteTrackerSettings"},
    {"sdrangel.feature.simpleptt", "SimplePTTSettings"},
    {"sdrangel.feature.startracker", "StarTrackerSettings"},
    {"sdrangel.feature.vorlocalizer", "VORLocalizerSettings"}
};

// Get integer value from within JSON object
bool WebAPIUtils::getObjectInt(const QJsonObject &json, const QString &key, int &value)
{
    if (json.contains(key))
    {
        value = json[key].toInt();
        return true;
    }

    return false;
}

// Get string value from within JSON object
bool WebAPIUtils::getObjectString(const QJsonObject &json, const QString &key, QString &value)
{
    if (json.contains(key))
    {
        value = json[key].toString();
        return true;
    }

    return false;
}

// Get a list of JSON objects from within JSON object
bool WebAPIUtils::getObjectObjects(const QJsonObject &json, const QString &key, QList<QJsonObject> &objects)
{
    bool processed = false;

    if (json.contains(key))
    {
        if (json[key].isArray())
        {
            QJsonArray a = json[key].toArray();

            for (QJsonArray::const_iterator  it = a.begin(); it != a.end(); it++)
            {
                if (it->isObject())
                {
                    objects.push_back(it->toObject());
                    processed = true;
                }
            }
        }
    }

    return processed;
}

// Get double value from within nested JSON object
bool WebAPIUtils::getSubObjectDouble(const QJsonObject &json, const QString &key, double &value)
{
    for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                value = subObject[key].toDouble();
                return true;
            }
        }
    }

    return false;
}

// Set double value withing nested JSON object
bool WebAPIUtils::setSubObjectDouble(QJsonObject &json, const QString &key, double value)
{
    for (QJsonObject::iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                subObject[key] = value;
                it.value() = subObject;
                return true;
            }
        }
    }

    return false;
}

// Get integer value from within nested JSON object
bool WebAPIUtils::getSubObjectInt(const QJsonObject &json, const QString &key, int &value)
{
    for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                value = subObject[key].toInt();
                return true;
            }
        }
    }

    return false;
}

// Set integer value withing nested JSON object
bool WebAPIUtils::setSubObjectInt(QJsonObject &json, const QString &key, int value)
{
    for (QJsonObject::iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                subObject[key] = value;
                it.value() = subObject;
                return true;
            }
        }
    }

    return false;
}

// Get string value from within nested JSON object
bool WebAPIUtils::getSubObjectString(const QJsonObject &json, const QString &key, QString &value)
{
    for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                value = subObject[key].toString();
                return true;
            }
        }
    }

    return false;
}

// Set string value withing nested JSON object
bool WebAPIUtils::setSubObjectString(QJsonObject &json, const QString &key, const QString &value)
{
    for (QJsonObject::iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                subObject[key] = value;
                it.value() = subObject;
                return true;
            }
        }
    }

    return false;
}

// Get string list from within nested JSON object
bool WebAPIUtils::getSubObjectIntList(const QJsonObject &json, const QString &key, const QString &subKey, QList<int> &values)
{
    for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                QJsonValue value = subObject[key];
                if (value.isArray())
                {
                    QJsonArray array = value.toArray();
                    for (int i = 0; i < array.size(); i++)
                    {
                        QJsonObject element = array[i].toObject();
                        if (element.contains(subKey)) {
                            values.append(element[subKey].toInt());
                        }
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

// look for value in key=value
bool WebAPIUtils::extractValue(const QJsonObject &json, const QString &key, QJsonValue &value)
{
    // final
    if (json.contains(key))
    {
        value = json[key];
        return true;
    }
    else
    {
        for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                if (extractValue(jsonValue.toObject(), key, value)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// look for [...] in key=[...]
bool WebAPIUtils::extractArray(const QJsonObject &json, const QString &key, QJsonArray &value)
{
    // final
    if (json.contains(key))
    {
        if (json[key].isArray())
        {
            value = json[key].toArray();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                if (extractArray(jsonValue.toObject(), key, value)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// look for {...} in key={...}
bool WebAPIUtils::extractObject(const QJsonObject &json, const QString &key, QJsonObject &value)
{
    // final
    if (json.contains(key))
    {
        if (json[key].isObject())
        {
            value = json[key].toObject();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                if (extractObject(jsonValue.toObject(), key, value)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// set value in key=value
bool WebAPIUtils::setValue(const QJsonObject &json, const QString &key, const QJsonValue &value)
{
    // final
    if (json.contains(key))
    {
        json[key] = value;
        return true;
    }
    else
    {
        for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                if (setValue(jsonValue.toObject(), key, value)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// set [...] in key=[...]
bool WebAPIUtils::setArray(const QJsonObject &json, const QString &key, const QJsonArray &value)
{
    // final
    if (json.contains(key))
    {
        if (json[key].isArray())
        {
            json[key] = value;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                if (setArray(jsonValue.toObject(), key, value)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// set {...} in key={...}
bool WebAPIUtils::setObject(const QJsonObject &json, const QString &key, const QJsonObject &value)
{
    // final
    if (json.contains(key))
    {
        if (json[key].isObject())
        {
            json[key] = value;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
        {
            QJsonValue jsonValue = it.value();

            if (jsonValue.isObject())
            {
                if (setObject(jsonValue.toObject(), key, value)) {
                    return true;
                }
            }
        }
    }

    return false;
}
