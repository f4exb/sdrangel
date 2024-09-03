///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2023 Mohamed <mohamedadlyi@github.com>                          //
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

#include <QGlobalStatic>
#include <QCoreApplication>
#include <QString>
#include <QDebug>
#include <QGeoPositionInfoSource>

#include "loggerwithfile.h"
#include "dsp/dsptypes.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "channel/channelapi.h"
#ifdef ANDROID
#include "util/android.h"
#endif

#include "maincore.h"

MESSAGE_CLASS_DEFINITION(MainCore::MsgDVSerial, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteInstance, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgLoadPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSavePreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeletePreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgLoadConfiguration, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSaveConfiguration, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteConfiguration, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddWorkspace, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteEmptyWorkspaces, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgLoadFeatureSetPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSaveFeatureSetPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteFeatureSetPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgRemoveLastDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSetDevice, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddChannel, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteChannel, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgApplySettings, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddFeature, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteFeature, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgChannelReport, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgChannelSettings, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgChannelDemodReport, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgChannelDemodQuery, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgMoveDeviceUIToWorkspace, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgMoveMainSpectrumUIToWorkspace, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgMoveFeatureUIToWorkspace, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgMoveChannelUIToWorkspace, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgMapItem, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgPacket, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgTargetAzimuthElevation, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgStarTrackerTarget, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgStarTrackerDisplaySettings, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgStarTrackerDisplayLoSSettings, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSkyMapTarget, Message)

MainCore::MainCore()
{
	m_masterTimer.setTimerType(Qt::PreciseTimer);
	m_masterTimer.start(50);
    m_startMsecsSinceEpoch = QDateTime::currentMSecsSinceEpoch();
    m_masterElapsedTimer.start();
    // Position can take a while to determine, so we start updates at program startup
    initPosition();
#ifdef ANDROID
    QObject::connect(this, &MainCore::deviceStateChanged, this, &MainCore::updateWakeLock);
#endif
}

MainCore::~MainCore()
{}

Q_GLOBAL_STATIC(MainCore, mainCore)
MainCore *MainCore::instance()
{
	return mainCore;
}

void MainCore::setLoggingOptions()
{
    if (!m_logger)
    {
        qDebug() << "MainCore::setLoggingOptions: No logger.";
        return;
    }

    m_logger->setConsoleMinMessageLevel(m_settings.getConsoleMinLogLevel());

    if (m_settings.getUseLogFile())
    {
        qtwebapp::FileLoggerSettings fileLoggerSettings; // default values

        if (m_logger->hasFileLogger()) {
            fileLoggerSettings = m_logger->getFileLoggerSettings(); // values from file logger if it exists
        }

        fileLoggerSettings.fileName = m_settings.getLogFileName(); // put new values
        m_logger->createOrSetFileLogger(fileLoggerSettings, 2000); // create file logger if it does not exist and apply settings in any case
    }

    if (m_logger->hasFileLogger()) {
        m_logger->setFileMinMessageLevel(m_settings.getFileMinLogLevel());
    }

    m_logger->setUseFileLogger(m_settings.getUseLogFile());

    if (m_settings.getUseLogFile())
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
        QString appInfoStr(QString("%1 %2 Qt %3 %4b %5 %6 DSP Rx:%7b Tx:%8b PID %9")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(QSysInfo::currentCpuArchitecture())
                .arg(QSysInfo::prettyProductName())
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_TX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid()));
#else
        QString appInfoStr(QString("%1 %2 Qt %3 %4b DSP Rx:%5b Tx:%6b PID %7")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_RX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid()));
 #endif
        m_logger->logToFile(QtInfoMsg, appInfoStr);
    }
}

DeviceAPI *MainCore::getDevice(unsigned int deviceSetIndex)
{
    if (deviceSetIndex < m_deviceSets.size()) {
        return m_deviceSets[deviceSetIndex]->m_deviceAPI;
    } else {
        return nullptr;
    }
}

ChannelAPI *MainCore::getChannel(unsigned int deviceSetIndex, int channelIndex)
{
    if (deviceSetIndex < m_deviceSets.size()) {
        return m_deviceSets[deviceSetIndex]->getChannelAt(channelIndex);
    } else {
        return nullptr;
    }
}

Feature *MainCore::getFeature(unsigned int featureSetIndex, int featureIndex)
{
    if (featureSetIndex < m_featureSets.size()) {
        return m_featureSets[featureSetIndex]->getFeatureAt(featureIndex);
    } else {
        return nullptr;
    }
}

void MainCore::appendFeatureSet()
{
    int newIndex = m_featureSets.size();

    if (newIndex != 0)
    {
        qWarning("MainCore::appendFeatureSet: attempt to add more than one feature set (%d)", newIndex);
        return;
    }

    FeatureSet *featureSet = new FeatureSet(newIndex);
    m_featureSets.push_back(featureSet);
    m_featureSetsMap.insert(featureSet, newIndex);
}

void MainCore::removeFeatureSet(unsigned int index)
{
    if (index < m_featureSets.size())
    {
        FeatureSet *featureSet = m_featureSets[index];
        m_featureSetsMap.remove(featureSet);
        m_featureSets.erase(m_featureSets.begin() + index);
    }
}

void MainCore::removeLastFeatureSet()
{
    if (m_featureSets.size() != 0)
    {
        FeatureSet *featureSet = m_featureSets.back();
        m_featureSetsMap.remove(featureSet);
        m_featureSets.pop_back();
        delete featureSet;
    }
}

void MainCore::appendDeviceSet(int deviceType)
{
    int newIndex = m_deviceSets.size();
    DeviceSet *deviceSet = new DeviceSet(newIndex, deviceType);
    m_deviceSets.push_back(deviceSet);
    m_deviceSetsMap.insert(deviceSet, newIndex);
}

void MainCore::removeLastDeviceSet()
{
    if (m_deviceSets.size() != 0)
    {
        DeviceSet *deviceSet = m_deviceSets.back();
        m_deviceSetsMap.remove(deviceSet);
        m_deviceSets.pop_back();
        delete deviceSet;
    }
}

void MainCore::removeDeviceSet(int deviceSetIndex)
{
    if (deviceSetIndex < (int) m_deviceSets.size())
    {
        DeviceSet *deviceSet = m_deviceSets[deviceSetIndex];
        m_deviceSetsMap.remove(deviceSet);
        m_deviceSets.erase(m_deviceSets.begin() + deviceSetIndex);
        delete deviceSet;
    }

    // Renumerate
    for (int i = 0; i < (int) m_deviceSets.size(); i++)
    {
        m_deviceSets[i]->m_deviceAPI->setDeviceSetIndex(i);
        m_deviceSets[i]->setIndex(i);
    }
}

void MainCore::addChannelInstance(DeviceSet *deviceSet, ChannelAPI *channelAPI)
{
    m_channelsMap.insert(channelAPI, deviceSet);
    emit channelAdded(m_deviceSetsMap[deviceSet], channelAPI);
    // debugMaps();
}

void MainCore::removeChannelInstanceAt(DeviceSet *deviceSet, int channelIndex)
{
    int deviceSetIndex = m_deviceSetsMap[deviceSet];
    ChannelAPI *channelAPI = m_deviceSets[deviceSetIndex]->getChannelAt(channelIndex);

    if (channelAPI)
    {
        m_channelsMap.remove(channelAPI);
        emit channelRemoved(deviceSetIndex, channelAPI);
    }
}

void MainCore::removeChannelInstance(ChannelAPI *channelAPI)
{
    if (channelAPI)
    {
        int deviceSetIndex = m_deviceSetsMap[m_channelsMap[channelAPI]];
        m_channelsMap.remove(channelAPI);
        emit channelRemoved(deviceSetIndex, channelAPI);
    }
}

void MainCore::clearChannels(DeviceSet *deviceSet)
{
    for (int i = 0; i < deviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channelAPI = deviceSet->getChannelAt(i);
        m_channelsMap.remove(channelAPI);
        emit channelRemoved(m_deviceSetsMap[deviceSet], channelAPI);
    }
}

void MainCore::addFeatureInstance(FeatureSet *featureSet, Feature *feature)
{
    m_featuresMap.insert(feature, featureSet);
    emit featureAdded(m_featureSetsMap[featureSet], feature);
    // debugMaps();
}

void MainCore::removeFeatureInstanceAt(FeatureSet *featureSet, int featureIndex)
{
    int featureSetIndex = m_featureSetsMap[featureSet];
    Feature *feature = m_featureSets[featureSetIndex]->getFeatureAt(featureIndex);

    if (feature)
    {
        m_featuresMap.remove(feature);
        emit featureRemoved(featureSetIndex, feature);
    }
}

void MainCore::removeFeatureInstance(Feature *feature)
{
    if (feature)
    {
        int featureSetIndex = m_featureSetsMap[m_featuresMap[feature]];
        m_featuresMap.remove(feature);
        emit featureRemoved(featureSetIndex, feature);
    }
}

void MainCore::clearFeatures(FeatureSet *featureSet)
{
    for (int i = 0; i < featureSet->getNumberOfFeatures(); i++)
    {
        Feature *feature = featureSet->getFeatureAt(i);
        m_featuresMap.remove(feature);
        emit featureRemoved(m_featureSetsMap[featureSet], feature);
    }
}

void MainCore::debugMaps()
{
    QMap<DeviceSet*, int>::const_iterator dsIt = m_deviceSetsMap.begin();

    for (; dsIt != m_deviceSetsMap.end(); ++dsIt) {
        qDebug("MainCore::debugMaps: device set %d #%d", dsIt.key()->getIndex(), dsIt.value());
    }

    QMap<FeatureSet*, int>::const_iterator fsIt = m_featureSetsMap.begin();

    for (; fsIt != m_featureSetsMap.end(); ++fsIt) {
        qDebug("MainCore::debugMaps: feature set %d #%d", fsIt.key()->getIndex(), fsIt.value());
    }

    QMap<ChannelAPI*, DeviceSet*>::const_iterator chIt = m_channelsMap.begin();

    for (; chIt != m_channelsMap.end(); ++chIt) {
        qDebug("MainCore::debugMaps: channel ds: %d - %d: %s %s",
            chIt.value()->getIndex(), chIt.key()->getIndexInDeviceSet(), qPrintable(chIt.key()->getURI()), qPrintable(chIt.key()->getName()));
    }

    QMap<Feature*, FeatureSet*>::const_iterator feIt = m_featuresMap.begin();

    for (; feIt != m_featuresMap.end(); ++feIt) {
        qDebug("MainCore::debugMaps: feature fs: %d - %d: %s %s",
            feIt.value()->getIndex(), feIt.key()->getIndexInFeatureSet(), qPrintable(feIt.key()->getURI()), qPrintable(feIt.key()->getName()));
    }
}

void MainCore::initPosition()
{
    m_positionSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_positionSource)
    {
        qDebug() << "MainCore::initPosition: Using position source" << m_positionSource->sourceName();
        connect(m_positionSource, &QGeoPositionInfoSource::positionUpdated, this, &MainCore::positionUpdated);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        connect(m_positionSource, &QGeoPositionInfoSource::updateTimeout, this, &MainCore::positionUpdateTimeout);
        connect(m_positionSource, qOverload<QGeoPositionInfoSource::Error>(&QGeoPositionInfoSource::error), this, &MainCore::positionError);
#else
        connect(m_positionSource, &QGeoPositionInfoSource::errorOccurred, this, &MainCore::positionError);
#endif
        m_position = m_positionSource->lastKnownPosition();
        m_positionSource->setUpdateInterval(1000);
        m_positionSource->startUpdates();
    }
    else
    {
        qWarning() << "MainCore::initPosition: No position source.";
    }
}

const QGeoPositionInfo& MainCore::getPosition() const
{
    return m_position;
}

void MainCore::positionUpdated(const QGeoPositionInfo &info)
{
    if (info.isValid())
    {
        m_position = info;
        if (m_settings.getAutoUpdatePosition())
        {
            m_settings.setLatitude(m_position.coordinate().latitude());
            m_settings.setLongitude(m_position.coordinate().longitude());
            if (!std::isnan(m_position.coordinate().altitude())) {
                m_settings.setAltitude(m_position.coordinate().altitude());
            }
        }
    }
}

void MainCore::positionUpdateTimeout()
{
    qWarning() << "MainCore::positionUpdateTimeout: GPS signal lost";
}

void MainCore::positionError(QGeoPositionInfoSource::Error positioningError)
{
    #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    if (positioningError == 4){
        positionUpdateTimeout();
        return;
    }
    #endif
    qWarning() << "MainCore::positionError: " << positioningError;
}

#ifdef ANDROID
// On Android, we want to prevent the app from being put to sleep, when any
// device is running
void MainCore::updateWakeLock()
{
    bool running = false;
    for (size_t i = 0; i < m_deviceSets.size(); i++)
    {
        if (m_deviceSets[i]->m_deviceAPI->state() == DeviceAPI::StRunning)
        {
            running = true;
            break;
        }
    }
    if (running) {
        Android::acquireWakeLock();
    } else {
        Android::releaseWakeLock();
    }
}
#endif

AvailableChannelOrFeatureList MainCore::getAvailableChannels(const QStringList& uris)
{
    AvailableChannelOrFeatureList list;

    for (const auto deviceSet : m_deviceSets)
    {
        for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
        {
            ChannelAPI* channel = deviceSet->getChannelAt(chi);

            if ((uris.size() == 0) || uris.contains(channel->getURI()))
            {
                QChar type = getDeviceSetTypeId(deviceSet);
                int streamIdx = type == 'M' ? channel->getStreamIndex() : -1;

                AvailableChannelOrFeature item {
                    type,
                    deviceSet->getIndex(),
                    chi,
                    streamIdx,
                    channel->getIdentifier(),
                    channel
                };
                list.append(item);
            }
        }
    }

    return list;
}

AvailableChannelOrFeatureList MainCore::getAvailableFeatures(const QStringList& uris)
{
    AvailableChannelOrFeatureList list;
    std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

    for (const auto& featureSet : featureSets)
    {
        for (int fei = 0; fei < featureSet->getNumberOfFeatures(); fei++)
        {
            Feature *feature = featureSet->getFeatureAt(fei);

            if ((uris.size() == 0) || uris.contains(feature->getURI()))
            {
                AvailableChannelOrFeature item {
                    'F',
                    featureSet->getIndex(),
                    fei,
                    -1,
                    feature->getIdentifier(),
                    feature
                };
                list.append(item);
            }
        }
    }

    return list;
}

AvailableChannelOrFeatureList MainCore::getAvailableChannelsAndFeatures(const QStringList& uris, const QString& kinds)
{
    AvailableChannelOrFeatureList list;

    if (kinds != "F") {
        list.append(getAvailableChannels(uris));
    }
    if (kinds.contains("F")) {
        list.append(getAvailableFeatures(uris));
    }

    return list;
}

QChar MainCore::getDeviceSetTypeId(const DeviceSet* deviceSet)
{
    if (deviceSet->m_deviceMIMOEngine) {
        return 'M';
    } else if (deviceSet->m_deviceSinkEngine) {
        return 'T';
    } else if (deviceSet->m_deviceSourceEngine) {
        return 'R';
    } else {
        return 'X'; // Unknown
    }
}

QString MainCore::getDeviceSetId(const DeviceSet* deviceSet)
{
    QChar type = getDeviceSetTypeId(deviceSet);

    return QString("%1%2").arg(type).arg(deviceSet->getIndex());
}

QString MainCore::getChannelId(const ChannelAPI* channel)
{
    std::vector<DeviceSet*> deviceSets = getDeviceSets();
    DeviceSet* deviceSet = deviceSets[channel->getDeviceSetIndex()];
    QString deviceSetId = getDeviceSetId(deviceSet);
    int index = channel->getIndexInDeviceSet();
    if (deviceSet->m_deviceMIMOEngine) {
        return QString("%1:%2.%3").arg(deviceSetId).arg(index).arg(channel->getStreamIndex());
    } else {
        return QString("%1:%2").arg(deviceSetId).arg(index);
    }
}

QStringList MainCore::getDeviceSetIds(bool rx, bool tx, bool mimo)
{
    QStringList list;
    std::vector<DeviceSet*> deviceSets = getDeviceSets();

    for (const auto deviceSet : deviceSets)
    {
        DSPDeviceSourceEngine *deviceSourceEngine = deviceSet->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine = deviceSet->m_deviceSinkEngine;
        DSPDeviceMIMOEngine *deviceMIMOEngine = deviceSet->m_deviceMIMOEngine;

        if (((deviceSourceEngine != nullptr) && rx)
            || ((deviceSinkEngine != nullptr) && tx)
            || ((deviceMIMOEngine != nullptr) && mimo))
        {
            list.append(getDeviceSetId(deviceSet));
        }
    }
    return list;
}

bool MainCore::getDeviceSetTypeFromId(const QString& deviceSetId, QChar &type)
{
    if (!deviceSetId.isEmpty())
    {
        type = deviceSetId[0];
        return (type == 'R') || (type == 'T') || (type == 'M');
    }
    else
    {
        return false;
    }
}

bool MainCore::getDeviceSetIndexFromId(const QString& deviceSetId, unsigned int &deviceSetIndex)
{
    const QRegularExpression re("[RTM]([0-9]+)");
    QRegularExpressionMatch match = re.match(deviceSetId);

    if (match.hasMatch())
    {
        deviceSetIndex = match.capturedTexts()[1].toInt();
        return true;
    }
    else
    {
        return false;
    }
}

bool MainCore::getDeviceAndChannelIndexFromId(const QString& channelId, unsigned int &deviceSetIndex, unsigned int &channelIndex)
{
    const QRegularExpression re("[RTM]([0-9]+):([0-9]+)");
    QRegularExpressionMatch match = re.match(channelId);

    if (match.hasMatch())
    {
        deviceSetIndex = match.capturedTexts()[1].toInt();
        channelIndex = match.capturedTexts()[2].toInt();
        return true;
    }
    else
    {
        return false;
    }
}

bool MainCore::getFeatureIndexFromId(const QString& featureId, unsigned int &featureSetIndex, unsigned int &featureIndex)
{
    const QRegularExpression re("F([0-9]+)?:([0-9]+)");
    QRegularExpressionMatch match = re.match(featureId);

    if (match.hasMatch())
    {
        if (match.capturedTexts()[1].isEmpty()) {
            featureSetIndex = 0;
        } else {
            featureSetIndex = match.capturedTexts()[1].toInt();
        }
        featureIndex = match.capturedTexts()[2].toInt();
        return true;
    }
    else
    {
        return false;
    }
}

std::vector<ChannelAPI*> MainCore::getChannels(const QString& uri)
{
    std::vector<ChannelAPI*> channels;

    for (const auto deviceSet : m_deviceSets)
    {
        for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
        {
            ChannelAPI* channel = deviceSet->getChannelAt(chi);
            if (channel->getURI() == uri) {
                channels.push_back(channel);
            }
        }
    }

    return channels;
}

QStringList MainCore::getChannelIds(const QString& uri)
{
    QStringList list;
    std::vector<ChannelAPI*> channels = getChannels(uri);

    for (const auto channel : channels) {
        list.append(getChannelId(channel));
    }

    return list;
}
