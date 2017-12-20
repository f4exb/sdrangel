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

#include <QCoreApplication>
#include <QList>
#include <QTextStream>

#include <unistd.h>

#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGLoggingInfo.h"
#include "SWGAudioDevices.h"
#include "SWGAudioDevicesSelect.h"
#include "SWGLocationInformation.h"
#include "SWGDVSeralDevices.h"
#include "SWGPresetImport.h"
#include "SWGPresetExport.h"
#include "SWGPresets.h"
#include "SWGPresetTransfer.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"

#include "maincore.h"
#include "loggerwithfile.h"
#include "device/deviceset.h"
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"
#include "device/deviceenumerator.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspengine.h"
#include "channel/channelsourceapi.h"
#include "channel/channelsinkapi.h"
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "webapiadaptersrv.h"

WebAPIAdapterSrv::WebAPIAdapterSrv(MainCore& mainCore) :
    m_mainCore(mainCore)
{
}

WebAPIAdapterSrv::~WebAPIAdapterSrv()
{
}

int WebAPIAdapterSrv::instanceSummary(
        SWGSDRangel::SWGInstanceSummaryResponse& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{

    *response.getVersion() = QCoreApplication::instance()->applicationVersion();
    *response.getQtVersion() = QString(QT_VERSION_STR);

    SWGSDRangel::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_mainCore.m_logger->getUseFileLogger() ? 1 : 0);

    if (logging->getDumpToFile()) {
        m_mainCore.m_logger->getLogFileName(*logging->getFileName());
        m_mainCore.m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }

    m_mainCore.m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    SWGSDRangel::SWGDeviceSetList *deviceSetList = response.getDevicesetlist();
    getDeviceSetList(deviceSetList);

    return 200;
}

int WebAPIAdapterSrv::instanceDelete(
        SWGSDRangel::SWGInstanceSummaryResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    instanceSummary(response, error);

    MainCore::MsgDeleteInstance *msg = MainCore::MsgDeleteInstance::create();
    m_mainCore.getInputMessageQueue()->push(msg);

    return 200;
}

int WebAPIAdapterSrv::instanceDevices(
            bool tx,
            SWGSDRangel::SWGInstanceDevicesResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    int nbSamplingDevices = tx ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();
    response.setDevicecount(nbSamplingDevices);
    QList<SWGSDRangel::SWGDeviceListItem*> *devices = response.getDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        PluginInterface::SamplingDevice samplingDevice = tx ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);
        devices->append(new SWGSDRangel::SWGDeviceListItem);
        *devices->back()->getDisplayedName() = samplingDevice.displayedName;
        *devices->back()->getHwType() = samplingDevice.hardwareId;
        *devices->back()->getSerial() = samplingDevice.serial;
        devices->back()->setSequence(samplingDevice.sequence);
        devices->back()->setTx(!samplingDevice.rxElseTx);
        devices->back()->setNbStreams(samplingDevice.deviceNbItems);
        devices->back()->setDeviceSetIndex(samplingDevice.claimed);
        devices->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterSrv::instanceChannels(
            bool tx,
            SWGSDRangel::SWGInstanceChannelsResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    PluginAPI::ChannelRegistrations *channelRegistrations = tx ? m_mainCore.m_pluginManager->getTxChannelRegistrations() : m_mainCore.m_pluginManager->getRxChannelRegistrations();
    int nbChannelDevices = channelRegistrations->size();
    response.setChannelcount(nbChannelDevices);
    QList<SWGSDRangel::SWGChannelListItem*> *channels = response.getChannels();

    for (int i = 0; i < nbChannelDevices; i++)
    {
        channels->append(new SWGSDRangel::SWGChannelListItem);
        PluginInterface *channelInterface = channelRegistrations->at(i).m_plugin;
        const PluginDescriptor& pluginDescriptor = channelInterface->getPluginDescriptor();
        *channels->back()->getVersion() = pluginDescriptor.version;
        *channels->back()->getName() = pluginDescriptor.displayedName;
        channels->back()->setTx(tx);
        *channels->back()->getIdUri() = channelRegistrations->at(i).m_channelIdURI;
        *channels->back()->getId() = channelRegistrations->at(i).m_channelId;
        channels->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterSrv::instanceLoggingGet(
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.setDumpToFile(m_mainCore.m_logger->getUseFileLogger() ? 1 : 0);

    if (response.getDumpToFile()) {
    	m_mainCore.m_logger->getLogFileName(*response.getFileName());
    	m_mainCore.m_logger->getFileMinMessageLevelStr(*response.getFileLevel());
    }

    m_mainCore.m_logger->getConsoleMinMessageLevelStr(*response.getConsoleLevel());

    return 200;
}

int WebAPIAdapterSrv::instanceLoggingPut(
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    bool dumpToFile = (response.getDumpToFile() != 0);
    QString* consoleLevel = response.getConsoleLevel();
    QString* fileLevel = response.getFileLevel();
    QString* fileName = response.getFileName();

    // perform actions
    if (consoleLevel) {
    	m_mainCore.m_settings.setConsoleMinLogLevel(getMsgTypeFromString(*consoleLevel));
    }

    if (fileLevel) {
    	m_mainCore.m_settings.setFileMinLogLevel(getMsgTypeFromString(*fileLevel));
    }

    m_mainCore.m_settings.setUseLogFile(dumpToFile);

    if (fileName) {
    	m_mainCore.m_settings.setLogFileName(*fileName);
    }

    m_mainCore.setLoggingOptions();

    // build response
    response.init();
    getMsgTypeString(m_mainCore.m_settings.getConsoleMinLogLevel(), *response.getConsoleLevel());
    response.setDumpToFile(m_mainCore.m_settings.getUseLogFile() ? 1 : 0);
    getMsgTypeString(m_mainCore.m_settings.getFileMinLogLevel(), *response.getFileLevel());
    *response.getFileName() = m_mainCore.m_settings.getLogFileName();

    return 200;
}

int WebAPIAdapterSrv::instanceAudioGet(
        SWGSDRangel::SWGAudioDevices& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainCore.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainCore.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    response.init();
    response.setNbInputDevices(nbInputDevices);
    response.setInputDeviceSelectedIndex(m_mainCore.m_audioDeviceInfo.getInputDeviceIndex());
    response.setNbOutputDevices(nbOutputDevices);
    response.setOutputDeviceSelectedIndex(m_mainCore.m_audioDeviceInfo.getOutputDeviceIndex());
    response.setInputVolume(m_mainCore.m_audioDeviceInfo.getInputVolume());
    QList<SWGSDRangel::SWGAudioDevice*> *inputDevices = response.getInputDevices();
    QList<SWGSDRangel::SWGAudioDevice*> *outputDevices = response.getOutputDevices();

    for (int i = 0; i < nbInputDevices; i++)
    {
        inputDevices->append(new SWGSDRangel::SWGAudioDevice);
        *inputDevices->back()->getName() = audioInputDevices.at(i).deviceName();
    }

    for (int i = 0; i < nbOutputDevices; i++)
    {
        outputDevices->append(new SWGSDRangel::SWGAudioDevice);
        *outputDevices->back()->getName() = audioOutputDevices.at(i).deviceName();
    }

    return 200;
}

int WebAPIAdapterSrv::instanceAudioPatch(
        SWGSDRangel::SWGAudioDevicesSelect& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    float inputVolume = response.getInputVolume();
    int inputIndex = response.getInputIndex();
    int outputIndex = response.getOutputIndex();

    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainCore.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainCore.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    inputVolume = inputVolume < 0.0 ? 0.0 : inputVolume > 1.0 ? 1.0 : inputVolume;
    inputIndex = inputIndex < -1 ? -1 : inputIndex > nbInputDevices ? nbInputDevices-1 : inputIndex;
    outputIndex = outputIndex < -1 ? -1 : outputIndex > nbOutputDevices ? nbOutputDevices-1 : outputIndex;

    m_mainCore.m_audioDeviceInfo.setInputVolume(inputVolume);
    m_mainCore.m_audioDeviceInfo.setInputDeviceIndex(inputIndex);
    m_mainCore.m_audioDeviceInfo.setOutputDeviceIndex(outputIndex);

    m_mainCore.m_dspEngine->setAudioInputVolume(inputVolume);
    m_mainCore.m_dspEngine->setAudioInputDeviceIndex(inputIndex);
    m_mainCore.m_dspEngine->setAudioOutputDeviceIndex(outputIndex);

    response.setInputVolume(m_mainCore.m_audioDeviceInfo.getInputVolume());
    response.setInputIndex(m_mainCore.m_audioDeviceInfo.getInputDeviceIndex());
    response.setOutputIndex(m_mainCore.m_audioDeviceInfo.getOutputDeviceIndex());

    return 200;
}

int WebAPIAdapterSrv::instanceLocationGet(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.setLatitude(m_mainCore.m_settings.getLatitude());
    response.setLongitude(m_mainCore.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterSrv::instanceLocationPut(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    float latitude = response.getLatitude();
    float longitude = response.getLongitude();

    latitude = latitude < -90.0 ? -90.0 : latitude > 90.0 ? 90.0 : latitude;
    longitude = longitude < -180.0 ? -180.0 : longitude > 180.0 ? 180.0 : longitude;

    m_mainCore.m_settings.setLatitude(latitude);
    m_mainCore.m_settings.setLongitude(longitude);

    response.setLatitude(m_mainCore.m_settings.getLatitude());
    response.setLongitude(m_mainCore.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterSrv::instanceDVSerialPatch(
            bool dvserial,
            SWGSDRangel::SWGDVSeralDevices& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    m_mainCore.m_dspEngine->setDVSerialSupport(dvserial);
    response.init();

    if (dvserial)
    {
        std::vector<std::string> deviceNames;
        m_mainCore.m_dspEngine->getDVSerialNames(deviceNames);
        response.setNbDevices((int) deviceNames.size());
        QList<SWGSDRangel::SWGDVSerialDevice*> *deviceNamesList = response.getDvSerialDevices();

        std::vector<std::string>::iterator it = deviceNames.begin();
        std::string deviceNamesStr = "DV Serial devices found: ";

        while (it != deviceNames.end())
        {
            deviceNamesList->append(new SWGSDRangel::SWGDVSerialDevice);
            *deviceNamesList->back()->getDeviceName() = QString::fromStdString(*it);
            ++it;
        }
    }
    else
    {
        response.setNbDevices(0);
    }

    return 200;
}

int WebAPIAdapterSrv::instancePresetFilePut(
            SWGSDRangel::SWGPresetImport& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    const QString& fileName = *query.getFilePath();

    if (fileName != "")
    {
        QFile exportFile(fileName);

        if (exportFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QByteArray base64Str;
            QTextStream instream(&exportFile);
            instream >> base64Str;
            exportFile.close();

            Preset* preset = m_mainCore.m_settings.newPreset("", "");
            preset->deserialize(QByteArray::fromBase64(base64Str));

            if (query.getGroupName() && (query.getGroupName()->size() > 0)) {
                preset->setGroup(*query.getGroupName());
            }

            if (query.getDescription() && (query.getDescription()->size() > 0)) {
                preset->setDescription(*query.getDescription());
            }

            response.init();
            response.setCenterFrequency(preset->getCenterFrequency());
            *response.getGroupName() = preset->getGroup();
            *response.getType() = preset->isSourcePreset() ? "R" : "T";
            *response.getName() = preset->getDescription();

            return 200;
        }
        else
        {
            *error.getMessage() = QString("File %1 not found or not readable").arg(fileName);
            return 404;
        }
    }
    else
    {
        *error.getMessage() = QString("Empty file path");
        return 404;
    }
}

int WebAPIAdapterSrv::instancePresetFilePost(
            SWGSDRangel::SWGPresetExport& query,
            SWGSDRangel::SWGPresetIdentifier& response,
            SWGSDRangel::SWGErrorResponse& error)
{
    QString filePath = *query.getFilePath();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();

    const Preset *selectedPreset = m_mainCore.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0)
    {
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName());
        return 404;
    }

    QString base64Str = selectedPreset->serialize().toBase64();

    if (filePath != "")
    {
        QFileInfo fileInfo(filePath);

        if (fileInfo.suffix() != "prex") {
            filePath += ".prex";
        }

        QFile exportFile(filePath);

        if (exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream outstream(&exportFile);
            outstream << base64Str;
            exportFile.close();

            response.init();
            response.setCenterFrequency(selectedPreset->getCenterFrequency());
            *response.getGroupName() = selectedPreset->getGroup();
            *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
            *response.getName() = selectedPreset->getDescription();

            return 200;
        }
        else
        {
            *error.getMessage() = QString("File %1 cannot be written").arg(filePath);
            return 404;
        }
    }
    else
    {
        *error.getMessage() = QString("Empty file path");
        return 404;
    }
}

int WebAPIAdapterSrv::instancePresetGet(
        SWGSDRangel::SWGPresets& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    int nbPresets = m_mainCore.m_settings.getPresetCount();
    int nbGroups = 0;
    int nbPresetsThisGroup = 0;
    QString groupName;
    response.init();
    QList<SWGSDRangel::SWGPresetGroup*> *groups = response.getGroups();
    QList<SWGSDRangel::SWGPresetItem*> *swgPresets = 0;
    int i = 0;

    // Presets are sorted by group first

    for (; i < nbPresets; i++)
    {
        const Preset *preset = m_mainCore.m_settings.getPreset(i);

        if ((i == 0) || (groupName != preset->getGroup())) // new group
        {
            if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
            groups->append(new SWGSDRangel::SWGPresetGroup);
            groups->back()->init();
            groupName = preset->getGroup();
            *groups->back()->getGroupName() = groupName;
            swgPresets = groups->back()->getPresets();
            nbGroups++;
            nbPresetsThisGroup = 0;
        }

        swgPresets->append(new SWGSDRangel::SWGPresetItem);
        swgPresets->back()->setCenterFrequency(preset->getCenterFrequency());
        *swgPresets->back()->getType() = preset->isSourcePreset() ? "R" : "T";
        *swgPresets->back()->getName() = preset->getDescription();
        nbPresetsThisGroup++;
    }

    if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
    response.setNbGroups(nbGroups);

    return 200;
}

int WebAPIAdapterSrv::instancePresetPatch(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainCore.m_deviceSets.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainCore.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0)
    {
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName());
        return 404;
    }

    DeviceSet *deviceSet = m_mainCore.m_deviceSets[deviceSetIndex];

    if (deviceSet->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
    {
        *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
        return 404;
    }

    if (deviceSet->m_deviceSinkEngine && selectedPreset->isSourcePreset())
    {
        *error.getMessage() = QString("Preset type (R) and device set type (Tx) mismatch");
        return 404;
    }

    MainCore::MsgLoadPreset *msg = MainCore::MsgLoadPreset::create(selectedPreset, deviceSetIndex);
    m_mainCore.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

int WebAPIAdapterSrv::instancePresetPut(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainCore.m_deviceSets.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainCore.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0)
    {
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName());
        return 404;
    }
    else // update existing preset
    {
        DeviceSet *deviceSet = m_mainCore.m_deviceSets[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
        {
            *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
            return 404;
        }

        if (deviceSet->m_deviceSinkEngine && selectedPreset->isSourcePreset())
        {
            *error.getMessage() = QString("Preset type (R) and device set type (Tx) mismatch");
            return 404;
        }
    }

    MainCore::MsgSavePreset *msg = MainCore::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, false);
    m_mainCore.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

int WebAPIAdapterSrv::instancePresetPost(
        SWGSDRangel::SWGPresetTransfer& query,
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainCore.m_deviceSets.size();

    if (deviceSetIndex >= nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    DeviceSet *deviceSet = m_mainCore.m_deviceSets[deviceSetIndex];
    int deviceCenterFrequency = 0;

    if (deviceSet->m_deviceSourceEngine) { // Rx
        deviceCenterFrequency = deviceSet->m_deviceSourceEngine->getSource()->getCenterFrequency();
    } else if (deviceSet->m_deviceSinkEngine) { // Tx
        deviceCenterFrequency = deviceSet->m_deviceSinkEngine->getSink()->getCenterFrequency();
    } else {
        *error.getMessage() = QString("Unknown device in device set (not Rx nor Tx)");
        return 500;
    }

    const Preset *selectedPreset = m_mainCore.m_settings.getPreset(*presetIdentifier->getGroupName(),
            deviceCenterFrequency,
            *presetIdentifier->getName());

    if (selectedPreset == 0) // save on a new preset
    {
        selectedPreset = m_mainCore.m_settings.newPreset(*presetIdentifier->getGroupName(), *presetIdentifier->getName());
    }
    else
    {
        *error.getMessage() = QString("Preset already exists [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(deviceCenterFrequency)
                .arg(*presetIdentifier->getName());
        return 409;
    }

    MainCore::MsgSavePreset *msg = MainCore::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, true);
    m_mainCore.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

int WebAPIAdapterSrv::instancePresetDelete(
        SWGSDRangel::SWGPresetIdentifier& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    const Preset *selectedPreset = m_mainCore.m_settings.getPreset(*response.getGroupName(),
            response.getCenterFrequency(),
            *response.getName());

    if (selectedPreset == 0)
    {
        *error.getMessage() = QString("There is no preset [%1, %2, %3]")
                .arg(*response.getGroupName())
                .arg(response.getCenterFrequency())
                .arg(*response.getName());
        return 404;
    }

    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    MainCore::MsgDeletePreset *msg = MainCore::MsgDeletePreset::create(const_cast<Preset*>(selectedPreset));
    m_mainCore.m_inputMessageQueue.push(msg);

    return 200;
}

int WebAPIAdapterSrv::instanceDeviceSetsGet(
        SWGSDRangel::SWGDeviceSetList& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    getDeviceSetList(&response);
    return 200;
}

int WebAPIAdapterSrv::instanceDeviceSetsPost(
        bool tx,
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    MainCore::MsgAddDeviceSet *msg = MainCore::MsgAddDeviceSet::create(tx);
    m_mainCore.m_inputMessageQueue.push(msg);

    response.init();
    *response.getMessage() = QString("MsgAddDeviceSet message submitted");

    return 200;
}

int WebAPIAdapterSrv::instanceDeviceSetsDelete(
        SWGSDRangel::SWGSuccessResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    if (m_mainCore.m_deviceSets.size() > 0)
    {
        MainCore::MsgRemoveLastDeviceSet *msg = MainCore::MsgRemoveLastDeviceSet::create();
        m_mainCore.m_inputMessageQueue.push(msg);

        response.init();
        *response.getMessage() = QString("MsgRemoveLastDeviceSet message submitted");

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = "No more device sets to be removed";

        return 404;
    }
}

void WebAPIAdapterSrv::getDeviceSetList(SWGSDRangel::SWGDeviceSetList* deviceSetList)
{
    deviceSetList->init();
    deviceSetList->setDevicesetcount((int) m_mainCore.m_deviceSets.size());

    std::vector<DeviceSet*>::const_iterator it = m_mainCore.m_deviceSets.begin();

    for (int i = 0; it != m_mainCore.m_deviceSets.end(); ++it, i++)
    {
        QList<SWGSDRangel::SWGDeviceSet*> *deviceSets = deviceSetList->getDeviceSets();
        deviceSets->append(new SWGSDRangel::SWGDeviceSet());

        getDeviceSet(deviceSets->back(), *it, i);
    }
}

void WebAPIAdapterSrv::getDeviceSet(SWGSDRangel::SWGDeviceSet *swgDeviceSet, const DeviceSet* deviceSet, int deviceUISetIndex)
{
    SWGSDRangel::SWGSamplingDevice *samplingDevice = swgDeviceSet->getSamplingDevice();
    samplingDevice->init();
    samplingDevice->setIndex(deviceUISetIndex);
    samplingDevice->setTx(deviceSet->m_deviceSinkEngine != 0);

    if (deviceSet->m_deviceSinkEngine) // Tx data
    {
        *samplingDevice->getHwType() = deviceSet->m_deviceSinkAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceSet->m_deviceSinkAPI->getSampleSinkSerial();
        samplingDevice->setSequence(deviceSet->m_deviceSinkAPI->getSampleSinkSequence());
        samplingDevice->setNbStreams(deviceSet->m_deviceSinkAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceSet->m_deviceSinkAPI->getItemIndex());
        deviceSet->m_deviceSinkAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSink *sampleSink = deviceSet->m_deviceSinkEngine->getSink();

        if (sampleSink) {
            samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSink->getSampleRate());
        }

        swgDeviceSet->setChannelcount(deviceSet->m_deviceSinkAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = swgDeviceSet->getChannels();

        for (int i = 0; i <  swgDeviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            ChannelSourceAPI *channel = deviceSet->m_deviceSinkAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceSet->m_deviceSourceEngine) // Rx data
    {
        *samplingDevice->getHwType() = deviceSet->m_deviceSourceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceSet->m_deviceSourceAPI->getSampleSourceSerial();
        samplingDevice->setSequence(deviceSet->m_deviceSourceAPI->getSampleSourceSequence());
        samplingDevice->setNbStreams(deviceSet->m_deviceSourceAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceSet->m_deviceSourceAPI->getItemIndex());
        deviceSet->m_deviceSourceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSource *sampleSource = deviceSet->m_deviceSourceEngine->getSource();

        if (sampleSource) {
            samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSource->getSampleRate());
        }

        swgDeviceSet->setChannelcount(deviceSet->m_deviceSourceAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = swgDeviceSet->getChannels();

        for (int i = 0; i <  swgDeviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            ChannelSinkAPI *channel = deviceSet->m_deviceSourceAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }
}

QtMsgType WebAPIAdapterSrv::getMsgTypeFromString(const QString& msgTypeString)
{
    if (msgTypeString == "debug") {
        return QtDebugMsg;
    } else if (msgTypeString == "info") {
        return QtInfoMsg;
    } else if (msgTypeString == "warning") {
        return QtWarningMsg;
    } else if (msgTypeString == "error") {
        return QtCriticalMsg;
    } else {
        return QtDebugMsg;
    }
}

void WebAPIAdapterSrv::getMsgTypeString(const QtMsgType& msgType, QString& levelStr)
{
    switch (msgType)
    {
    case QtDebugMsg:
        levelStr = "debug";
        break;
    case QtInfoMsg:
        levelStr = "info";
        break;
    case QtWarningMsg:
        levelStr = "warning";
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        levelStr = "error";
        break;
    default:
        levelStr = "debug";
        break;
    }
}
