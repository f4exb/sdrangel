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

#include <QApplication>
#include <QList>

#include <unistd.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "loggerwithfile.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "device/deviceenumerator.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/dspengine.h"
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "channel/channelsinkapi.h"
#include "channel/channelsourceapi.h"

#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGDeviceListItem.h"
#include "SWGAudioDevices.h"
#include "SWGAudioDevicesSelect.h"
#include "SWGLocationInformation.h"
#include "SWGDVSeralDevices.h"
#include "SWGDVSerialDevice.h"
#include "SWGPresets.h"
#include "SWGPresetGroup.h"
#include "SWGPresetItem.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGErrorResponse.h"

#include "webapiadaptergui.h"

WebAPIAdapterGUI::WebAPIAdapterGUI(MainWindow& mainWindow) :
    m_mainWindow(mainWindow)
{
}

WebAPIAdapterGUI::~WebAPIAdapterGUI()
{
}

int WebAPIAdapterGUI::instanceSummary(
            Swagger::SWGInstanceSummaryResponse& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{

    *response.getVersion() = qApp->applicationVersion();

    Swagger::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_mainWindow.m_logger->getUseFileLogger() ? 1 : 0);

    if (logging->getDumpToFile()) {
        m_mainWindow.m_logger->getLogFileName(*logging->getFileName());
        m_mainWindow.m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }

    m_mainWindow.m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    Swagger::SWGDeviceSetList *deviceSetList = response.getDevicesetlist();
    getDeviceSetList(deviceSetList);

    return 200;
}

int WebAPIAdapterGUI::instanceDevices(
            bool tx,
            Swagger::SWGInstanceDevicesResponse& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    int nbSamplingDevices = tx ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();
    response.setDevicecount(nbSamplingDevices);
    QList<Swagger::SWGDeviceListItem*> *devices = response.getDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        PluginInterface::SamplingDevice samplingDevice = tx ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);
        devices->append(new Swagger::SWGDeviceListItem);
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

int WebAPIAdapterGUI::instanceChannels(
            bool tx,
            Swagger::SWGInstanceChannelsResponse& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    PluginAPI::ChannelRegistrations *channelRegistrations = tx ? m_mainWindow.m_pluginManager->getTxChannelRegistrations() : m_mainWindow.m_pluginManager->getRxChannelRegistrations();
    int nbChannelDevices = channelRegistrations->size();
    response.setChannelcount(nbChannelDevices);
    QList<Swagger::SWGChannelListItem*> *channels = response.getChannels();

    for (int i = 0; i < nbChannelDevices; i++)
    {
        channels->append(new Swagger::SWGChannelListItem);
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

int WebAPIAdapterGUI::instanceLoggingGet(
            Swagger::SWGLoggingInfo& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    response.setDumpToFile(m_mainWindow.m_logger->getUseFileLogger() ? 1 : 0);

    if (response.getDumpToFile()) {
        m_mainWindow.m_logger->getLogFileName(*response.getFileName());
        m_mainWindow.m_logger->getFileMinMessageLevelStr(*response.getFileLevel());
    }

    m_mainWindow.m_logger->getConsoleMinMessageLevelStr(*response.getConsoleLevel());

    return 200;
}

int WebAPIAdapterGUI::instanceLoggingPut(
            Swagger::SWGLoggingInfo& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    bool dumpToFile = (response.getDumpToFile() != 0);
    QString* consoleLevel = response.getConsoleLevel();
    QString* fileLevel = response.getFileLevel();
    QString* fileName = response.getFileName();

    // perform actions
    if (consoleLevel) {
        m_mainWindow.m_settings.setConsoleMinLogLevel(getMsgTypeFromString(*consoleLevel));
    }

    if (fileLevel) {
        m_mainWindow.m_settings.setFileMinLogLevel(getMsgTypeFromString(*fileLevel));
    }

    m_mainWindow.m_settings.setUseLogFile(dumpToFile);

    if (fileName) {
        m_mainWindow.m_settings.setLogFileName(*fileName);
    }

    m_mainWindow.setLoggingOpions();

    // build response
    response.init();
    getMsgTypeString(m_mainWindow.m_settings.getConsoleMinLogLevel(), *response.getConsoleLevel());
    response.setDumpToFile(m_mainWindow.m_settings.getUseLogFile() ? 1 : 0);
    getMsgTypeString(m_mainWindow.m_settings.getFileMinLogLevel(), *response.getFileLevel());
    *response.getFileName() = m_mainWindow.m_settings.getLogFileName();

    return 200;
}

int WebAPIAdapterGUI::instanceAudioGet(
            Swagger::SWGAudioDevices& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainWindow.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainWindow.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    response.init();
    response.setNbInputDevices(nbInputDevices);
    response.setInputDeviceSelectedIndex(m_mainWindow.m_audioDeviceInfo.getInputDeviceIndex());
    response.setNbOutputDevices(nbOutputDevices);
    response.setOutputDeviceSelectedIndex(m_mainWindow.m_audioDeviceInfo.getOutputDeviceIndex());
    response.setInputVolume(m_mainWindow.m_audioDeviceInfo.getInputVolume());
    QList<Swagger::SWGAudioDevice*> *inputDevices = response.getInputDevices();
    QList<Swagger::SWGAudioDevice*> *outputDevices = response.getOutputDevices();

    for (int i = 0; i < nbInputDevices; i++)
    {
        inputDevices->append(new Swagger::SWGAudioDevice);
        *inputDevices->back()->getName() = audioInputDevices.at(i).deviceName();
    }

    for (int i = 0; i < nbOutputDevices; i++)
    {
        outputDevices->append(new Swagger::SWGAudioDevice);
        *outputDevices->back()->getName() = audioOutputDevices.at(i).deviceName();
    }

    return 200;
}

int WebAPIAdapterGUI::instanceAudioPatch(
            Swagger::SWGAudioDevicesSelect& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    float inputVolume = response.getInputVolume();
    int inputIndex = response.getInputIndex();
    int outputIndex = response.getOutputIndex();

    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainWindow.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainWindow.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    inputVolume = inputVolume < 0.0 ? 0.0 : inputVolume > 1.0 ? 1.0 : inputVolume;
    inputIndex = inputIndex < -1 ? -1 : inputIndex > nbInputDevices ? nbInputDevices-1 : inputIndex;
    outputIndex = outputIndex < -1 ? -1 : outputIndex > nbOutputDevices ? nbOutputDevices-1 : outputIndex;

    m_mainWindow.m_audioDeviceInfo.setInputVolume(inputVolume);
    m_mainWindow.m_audioDeviceInfo.setInputDeviceIndex(inputIndex);
    m_mainWindow.m_audioDeviceInfo.setOutputDeviceIndex(outputIndex);

    m_mainWindow.m_dspEngine->setAudioInputVolume(inputVolume);
    m_mainWindow.m_dspEngine->setAudioInputDeviceIndex(inputIndex);
    m_mainWindow.m_dspEngine->setAudioOutputDeviceIndex(outputIndex);

    response.setInputVolume(m_mainWindow.m_audioDeviceInfo.getInputVolume());
    response.setInputIndex(m_mainWindow.m_audioDeviceInfo.getInputDeviceIndex());
    response.setOutputIndex(m_mainWindow.m_audioDeviceInfo.getOutputDeviceIndex());

    return 200;
}

int WebAPIAdapterGUI::instanceLocationGet(
            Swagger::SWGLocationInformation& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    response.setLatitude(m_mainWindow.m_settings.getLatitude());
    response.setLongitude(m_mainWindow.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterGUI::instanceLocationPut(
            Swagger::SWGLocationInformation& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    float latitude = response.getLatitude();
    float longitude = response.getLongitude();

    latitude = latitude < -90.0 ? -90.0 : latitude > 90.0 ? 90.0 : latitude;
    longitude = longitude < -180.0 ? -180.0 : longitude > 180.0 ? 180.0 : longitude;

    m_mainWindow.m_settings.setLatitude(latitude);
    m_mainWindow.m_settings.setLongitude(longitude);

    response.setLatitude(m_mainWindow.m_settings.getLatitude());
    response.setLongitude(m_mainWindow.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterGUI::instanceDVSerialPatch(
            bool dvserial,
            Swagger::SWGDVSeralDevices& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    m_mainWindow.m_dspEngine->setDVSerialSupport(dvserial);
    m_mainWindow.ui->action_DV_Serial->setChecked(dvserial);
    response.init();

    if (dvserial)
    {
        std::vector<std::string> deviceNames;
        m_mainWindow.m_dspEngine->getDVSerialNames(deviceNames);
        response.setNbDevices((int) deviceNames.size());
        QList<Swagger::SWGDVSerialDevice*> *deviceNamesList = response.getDvSerialDevices();

        std::vector<std::string>::iterator it = deviceNames.begin();
        std::string deviceNamesStr = "DV Serial devices found: ";

        while (it != deviceNames.end())
        {
            deviceNamesList->append(new Swagger::SWGDVSerialDevice);
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

int WebAPIAdapterGUI::instancePresetGet(
            Swagger::SWGPresets& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    int nbPresets = m_mainWindow.m_settings.getPresetCount();
    int nbGroups = 0;
    int nbPresetsThisGroup = 0;
    QString groupName;
    response.init();
    QList<Swagger::SWGPresetGroup*> *groups = response.getGroups();
    QList<Swagger::SWGPresetItem*> *swgPresets = 0;
    int i = 0;

    // Presets are sorted by group first

    for (; i < nbPresets; i++)
    {
        const Preset *preset = m_mainWindow.m_settings.getPreset(i);

        if ((i == 0) || (groupName != preset->getGroup())) // new group
        {
            if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
            groups->append(new Swagger::SWGPresetGroup);
            groups->back()->init();
            groupName = preset->getGroup();
            *groups->back()->getGroupName() = groupName;
            swgPresets = groups->back()->getPresets();
            nbGroups++;
            nbPresetsThisGroup = 0;
        }

        swgPresets->append(new Swagger::SWGPresetItem);
        swgPresets->back()->setCenterFrequency(preset->getCenterFrequency());
        *swgPresets->back()->getType() = preset->isSourcePreset() ? "R" : "T";
        *swgPresets->back()->getName() = preset->getDescription();
        nbPresetsThisGroup++;
    }

    if (i > 0) { groups->back()->setNbPresets(nbPresetsThisGroup); }
    response.setNbGroups(nbGroups);

    return 200;
}

int WebAPIAdapterGUI::instancePresetPatch(
        Swagger::SWGPresetTransfer& query,
        Swagger::SWGPresetIdentifier& response,
        Swagger::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    Swagger::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();

    if (deviceSetIndex > nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
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

    DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];

    if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
    {
        *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
        return 404;
    }

    if (deviceUI->m_deviceSinkEngine && selectedPreset->isSourcePreset())
    {
        *error.getMessage() = QString("Preset type (R) and device set type (Tx) mismatch");
        return 404;
    }

    MainWindow::MsgLoadPreset *msg = MainWindow::MsgLoadPreset::create(selectedPreset, deviceSetIndex);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

int WebAPIAdapterGUI::instancePresetPut(
        Swagger::SWGPresetTransfer& query,
        Swagger::SWGPresetIdentifier& response,
        Swagger::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    Swagger::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();

    if (deviceSetIndex > nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
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
        DeviceUISet *deviceUI = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if (deviceUI->m_deviceSourceEngine && !selectedPreset->isSourcePreset())
        {
            *error.getMessage() = QString("Preset type (T) and device set type (Rx) mismatch");
            return 404;
        }

        if (deviceUI->m_deviceSinkEngine && selectedPreset->isSourcePreset())
        {
            *error.getMessage() = QString("Preset type (R) and device set type (Tx) mismatch");
            return 404;
        }
    }

    MainWindow::MsgSavePreset *msg = MainWindow::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, false);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

int WebAPIAdapterGUI::instancePresetPost(
        Swagger::SWGPresetTransfer& query,
        Swagger::SWGPresetIdentifier& response,
        Swagger::SWGErrorResponse& error)
{
    int deviceSetIndex = query.getDeviceSetIndex();
    Swagger::SWGPresetIdentifier *presetIdentifier = query.getPreset();
    int nbDeviceSets = m_mainWindow.m_deviceUIs.size();

    if (deviceSetIndex > nbDeviceSets)
    {
        *error.getMessage() = QString("There is no device set at index %1. Number of device sets is %2").arg(deviceSetIndex).arg(nbDeviceSets);
        return 404;
    }

    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*presetIdentifier->getGroupName(),
            presetIdentifier->getCenterFrequency(),
            *presetIdentifier->getName());

    if (selectedPreset == 0) // save on a new preset
    {
        selectedPreset = m_mainWindow.m_settings.newPreset(*presetIdentifier->getGroupName(), *presetIdentifier->getName());
    }
    else
    {
        *error.getMessage() = QString("Preset already exists [%1, %2, %3]")
                .arg(*presetIdentifier->getGroupName())
                .arg(presetIdentifier->getCenterFrequency())
                .arg(*presetIdentifier->getName());
        return 409;
    }

    MainWindow::MsgSavePreset *msg = MainWindow::MsgSavePreset::create(const_cast<Preset*>(selectedPreset), deviceSetIndex, true);
    m_mainWindow.m_inputMessageQueue.push(msg);

    response.init();
    response.setCenterFrequency(selectedPreset->getCenterFrequency());
    *response.getGroupName() = selectedPreset->getGroup();
    *response.getType() = selectedPreset->isSourcePreset() ? "R" : "T";
    *response.getName() = selectedPreset->getDescription();

    return 200;
}

int WebAPIAdapterGUI::instancePresetDelete(
        Swagger::SWGPresetIdentifier& response,
        Swagger::SWGErrorResponse& error)
{
    const Preset *selectedPreset = m_mainWindow.m_settings.getPreset(*response.getGroupName(),
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

    MainWindow::MsgDeletePreset *msg = MainWindow::MsgDeletePreset::create(const_cast<Preset*>(selectedPreset));
    m_mainWindow.m_inputMessageQueue.push(msg);

    return 200;
}

int WebAPIAdapterGUI::instanceDeviceSetsGet(
            Swagger::SWGDeviceSetList& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    getDeviceSetList(&response);
    return 200;
}

int WebAPIAdapterGUI::instanceDeviceSetsPost(
        bool tx,
        Swagger::SWGDeviceSet& response,
        Swagger::SWGErrorResponse& error __attribute__((unused)))
{
    MainWindow::MsgAddDeviceSet *msg = MainWindow::MsgAddDeviceSet::create(tx);
    m_mainWindow.m_inputMessageQueue.push(msg);

    usleep(100000);

    const DeviceUISet *lastDeviceSet = m_mainWindow.m_deviceUIs.back();
    getDeviceSet(&response,lastDeviceSet, (int) m_mainWindow.m_deviceUIs.size() - 1);

    return 200;
}

int WebAPIAdapterGUI::instanceDeviceSetsDelete(
            Swagger::SWGDeviceSetList& response,
            Swagger::SWGErrorResponse& error)
{
    if (m_mainWindow.m_deviceUIs.size() > 1)
    {
        MainWindow::MsgRemoveLastDeviceSet *msg = MainWindow::MsgRemoveLastDeviceSet::create();
        m_mainWindow.m_inputMessageQueue.push(msg);

        usleep(100000);

        getDeviceSetList(&response);

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = "No more device sets to be removed";

        return 404;
    }
}

int WebAPIAdapterGUI::devicesetGet(
        int deviceSetIndex,
        Swagger::SWGDeviceSet& response,
        Swagger::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        const DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];
        getDeviceSet(&response, deviceSet, deviceSetIndex);

        return 200;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

int WebAPIAdapterGUI::devicesetDevicePut(
        int deviceSetIndex,
        Swagger::SWGDeviceListItem& response,
        Swagger::SWGErrorResponse& error)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_mainWindow.m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_mainWindow.m_deviceUIs[deviceSetIndex];

        if ((response.getTx() == 0) && (deviceSet->m_deviceSinkEngine))
        {
            *error.getMessage() = QString("Device type (Rx) and device set type (Tx) mismatch");
            return 404;
        }

        if ((response.getTx() != 0) && (deviceSet->m_deviceSourceEngine))
        {
            *error.getMessage() = QString("Device type (Tx) and device set type (Rx) mismatch");
            return 404;
        }

        int nbSamplingDevices = response.getTx() != 0 ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();
        int tx = response.getTx();

        for (int i = 0; i < nbSamplingDevices; i++)
        {
            PluginInterface::SamplingDevice samplingDevice = response.getTx() ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);

            if (response.getDisplayedName() && (*response.getDisplayedName() != samplingDevice.displayedName)) {
                continue;
            }

            if (response.getHwType() && (*response.getHwType() != samplingDevice.hardwareId)) {
                continue;
            }

            if ((response.getSequence() >= 0) && (response.getSequence() != samplingDevice.sequence)) {
                continue;
            }

            if (response.getSerial() && (*response.getSerial() != samplingDevice.serial)) {
                continue;
            }

            if ((response.getStreamIndex() >= 0) && (response.getStreamIndex() != samplingDevice.deviceItemIndex)) {
                continue;
            }

            MainWindow::MsgSetDevice *msg = MainWindow::MsgSetDevice::create(deviceSetIndex, i, response.getTx() != 0);
            m_mainWindow.m_inputMessageQueue.push(msg);

            response.init();
            *response.getDisplayedName() = samplingDevice.displayedName;
            *response.getHwType() = samplingDevice.hardwareId;
            *response.getSerial() = samplingDevice.serial;
            response.setSequence(samplingDevice.sequence);
            response.setTx(tx);
            response.setNbStreams(samplingDevice.deviceNbItems);
            response.setStreamIndex(samplingDevice.deviceItemIndex);
            response.setDeviceSetIndex(deviceSetIndex);
            response.setIndex(i);

            return 200;
        }

        *error.getMessage() = QString("Device not found");
        return 404;
    }
    else
    {
        error.init();
        *error.getMessage() = QString("There is no device set with index %1").arg(deviceSetIndex);

        return 404;
    }
}

void WebAPIAdapterGUI::getDeviceSetList(Swagger::SWGDeviceSetList* deviceSetList)
{
    deviceSetList->init();
    deviceSetList->setDevicesetcount((int) m_mainWindow.m_deviceUIs.size());

    std::vector<DeviceUISet*>::const_iterator it = m_mainWindow.m_deviceUIs.begin();

    for (int i = 0; it != m_mainWindow.m_deviceUIs.end(); ++it, i++)
    {
        QList<Swagger::SWGDeviceSet*> *deviceSets = deviceSetList->getDeviceSets();
        deviceSets->append(new Swagger::SWGDeviceSet());

        getDeviceSet(deviceSets->back(), *it, i);
    }
}

void WebAPIAdapterGUI::getDeviceSet(Swagger::SWGDeviceSet *deviceSet, const DeviceUISet* deviceUISet, int deviceUISetIndex)
{
    Swagger::SWGSamplingDevice *samplingDevice = deviceSet->getSamplingDevice();
    samplingDevice->init();
    samplingDevice->setIndex(deviceUISetIndex);
    samplingDevice->setTx(deviceUISet->m_deviceSinkEngine != 0);

    if (deviceUISet->m_deviceSinkEngine) // Tx data
    {
        *samplingDevice->getHwType() = deviceUISet->m_deviceSinkAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceUISet->m_deviceSinkAPI->getSampleSinkSerial();
        samplingDevice->setSequence(deviceUISet->m_deviceSinkAPI->getSampleSinkSequence());
        samplingDevice->setNbStreams(deviceUISet->m_deviceSinkAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceUISet->m_deviceSinkAPI->getItemIndex());
        deviceUISet->m_deviceSinkAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSink *sampleSink = deviceUISet->m_deviceSinkEngine->getSink();

        if (sampleSink) {
            samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSink->getSampleRate());
        }

        deviceSet->setChannelcount(deviceUISet->m_deviceSinkAPI->getNbChannels());
        QList<Swagger::SWGChannel*> *channels = deviceSet->getChannels();

        for (int i = 0; i <  deviceSet->getChannelcount(); i++)
        {
            channels->append(new Swagger::SWGChannel);
            ChannelSourceAPI *channel = deviceUISet->m_deviceSinkAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceUISet->m_deviceSourceEngine) // Rx data
    {
        *samplingDevice->getHwType() = deviceUISet->m_deviceSourceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceUISet->m_deviceSourceAPI->getSampleSourceSerial();
        samplingDevice->setSequence(deviceUISet->m_deviceSourceAPI->getSampleSourceSequence());
        samplingDevice->setNbStreams(deviceUISet->m_deviceSourceAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceUISet->m_deviceSourceAPI->getItemIndex());
        deviceUISet->m_deviceSourceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSource *sampleSource = deviceUISet->m_deviceSourceEngine->getSource();

        if (sampleSource) {
            samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSource->getSampleRate());
        }

        deviceSet->setChannelcount(deviceUISet->m_deviceSourceAPI->getNbChannels());
        QList<Swagger::SWGChannel*> *channels = deviceSet->getChannels();

        for (int i = 0; i <  deviceSet->getChannelcount(); i++)
        {
            channels->append(new Swagger::SWGChannel);
            ChannelSinkAPI *channel = deviceUISet->m_deviceSourceAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }
}

QtMsgType WebAPIAdapterGUI::getMsgTypeFromString(const QString& msgTypeString)
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

void WebAPIAdapterGUI::getMsgTypeString(const QtMsgType& msgType, QString& levelStr)
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
