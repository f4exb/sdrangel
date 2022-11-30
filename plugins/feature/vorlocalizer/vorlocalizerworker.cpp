///////////////////////////////////////////////////////////////////////////////////
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

#include <QDebug>

#include "SWGDeviceState.h"
#include "SWGDeviceSettings.h"
#include "SWGChannelSettings.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"

#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "channel/channelapi.h"
#include "channel/channelwebapiutils.h"
#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"
#include "maincore.h"

#include "vorlocalizerreport.h"
#include "vorlocalizerworker.h"

MESSAGE_CLASS_DEFINITION(VorLocalizerWorker::MsgConfigureVORLocalizerWorker, Message)
MESSAGE_CLASS_DEFINITION(VorLocalizerWorker::MsgRefreshChannels, Message)

class DSPDeviceSourceEngine;

VorLocalizerWorker::VorLocalizerWorker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToFeature(nullptr),
    m_availableChannels(nullptr),
    m_updateTimer(this),
    m_rrTimer(this)
{
    qDebug("VorLocalizerWorker::VorLocalizerWorker");
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
}

VorLocalizerWorker::~VorLocalizerWorker()
{
    m_inputMessageQueue.clear();
}

void VorLocalizerWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void VorLocalizerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(&m_rrTimer, SIGNAL(timeout()), this, SLOT(rrNextTurn()));
    connect(thread(), SIGNAL(started()), this, SLOT(started()));
    connect(thread(), SIGNAL(finished()), this, SLOT(finished()));
}

// startWork() is called from main thread. Timers/sockets need to be started on worker thread
void VorLocalizerWorker::started()
{
    m_rrTimer.start(m_settings.m_rrTime * 1000);
    disconnect(thread(), SIGNAL(started()), this, SLOT(started()));
}

void VorLocalizerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void VorLocalizerWorker::finished()
{
    m_rrTimer.stop();
    disconnect(&m_rrTimer, SIGNAL(timeout()), this, SLOT(rrNextTurn()));
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
}

void VorLocalizerWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool VorLocalizerWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureVORLocalizerWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureVORLocalizerWorker& cfg = (MsgConfigureVORLocalizerWorker&) cmd;
        qDebug() << "VorLocalizerWorker::handleMessage: MsgConfigureVORLocalizerWorker";

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgRefreshChannels::match(cmd))
    {
        qDebug() << "VorLocalizerWorker::handleMessage: MsgRefreshChannels";
        updateChannels();

        return true;
    }
    else
    {
        return false;
    }
}

void VorLocalizerWorker::applySettings(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "VorLocalizerWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    // Remove sub-channels no longer needed
    for (int i = 0; i < m_vorChannels.size(); i++)
    {
        if (!settings.m_subChannelSettings.contains(m_vorChannels[i].m_subChannelId))
        {
            qDebug() << "VorLocalizerWorker::applySettings: Removing sink " << m_vorChannels[i].m_subChannelId;
            removeVORChannel(m_vorChannels[i].m_subChannelId);
        }
    }

    // Add new sub channels
    QHash<int, VORLocalizerSubChannelSettings>::const_iterator itr = settings.m_subChannelSettings.begin();

    while (itr != settings.m_subChannelSettings.end())
    {
        const VORLocalizerSubChannelSettings& subChannelSettings = itr.value();
        qDebug() << "VorLocalizerWorker::applySettings: subchannel " << subChannelSettings.m_id;
        int j = 0;

        for (; j < m_vorChannels.size(); j++)
        {
            if (subChannelSettings.m_id == m_vorChannels[j].m_subChannelId)
            {
                qDebug() << "VorLocalizerWorker::applySettings: subchannel "
                    << subChannelSettings.m_id
                    << "already present";
                break;
            }
        }

        if (j == m_vorChannels.size())
        {
            // Add a sub-channel sink
            qDebug() << "VorLocalizerWorker::applySettings: Adding subchannel " << subChannelSettings.m_id;
            addVORChannel(subChannelSettings);
        }

        ++itr;
    }

    for (auto subChannelSetting : settings.m_subChannelSettings)
    {
        int navId = subChannelSetting.m_id;

        if (m_settings.m_subChannelSettings.contains(navId))
        {
            if (subChannelSetting.m_audioMute != m_settings.m_subChannelSettings[navId].m_audioMute)
            {
                qDebug() << "VorLocalizerWorker::applySettings: audioMute:" << subChannelSetting.m_audioMute;
                setAudioMute(navId, subChannelSetting.m_audioMute);
            }
        }
    }

    if (settingsKeys.contains("rrTime") || force) {
        m_rrTimer.start(settings.m_rrTime * 1000);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void VorLocalizerWorker::updateHardware()
{
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    m_updateTimer.stop();
    m_mutex.unlock();
}

quint64 VorLocalizerWorker::getDeviceCenterFrequency(int deviceIndex)
{
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < (int) deviceSets.size())
    {
        DeviceSet *deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            return source->getCenterFrequency();
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            return sink->getCenterFrequency();
        }
    }
    return 0;
}

int VorLocalizerWorker::getDeviceSampleRate(int deviceIndex)
{
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < (int) deviceSets.size())
    {
        DeviceSet *deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            return source->getSampleRate();
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            return sink->getSampleRate();
        }
    }
    return 0;
}

// Does this device have a center frequency setting (FileInput doesn't)
bool VorLocalizerWorker::hasCenterFrequencySetting(int deviceIndex)
{
    double deviceFrequency;
    return !ChannelWebAPIUtils::getCenterFrequency(deviceIndex, deviceFrequency);
}

void VorLocalizerWorker::removeVORChannel(int navId)
{
    qDebug("VorLocalizerWorker::removeVORChannel: %d", navId);

    for (int i = 0; i < m_vorChannels.size(); i++)
    {
        if (m_vorChannels[i].m_subChannelId == navId)
        {
            m_vorChannels.removeAt(i);
            break;
        }
    }

    updateChannels();
}

void VorLocalizerWorker::addVORChannel(const VORLocalizerSubChannelSettings& subChannelSettings)
{
    qDebug("VorLocalizerWorker::addVORChannel: %d at %d Hz",
        subChannelSettings.m_id, subChannelSettings.m_frequency);

    VORLocalizerSettings::VORChannel vorChannel =
        VORLocalizerSettings::VORChannel{
            subChannelSettings.m_id,
            subChannelSettings.m_frequency,
            subChannelSettings.m_audioMute
        };
    m_vorChannels.push_back(vorChannel);
    updateChannels();
}

void VorLocalizerWorker::updateChannels()
{
    qDebug() << "VorLocalizerWorker::updateChannels: "
        << "#VORs:" << m_vorChannels.size()
        << "#Chans:" << m_availableChannels->size();

    if ((m_vorChannels.size() == 0) || (m_availableChannels->size() == 0)) {
        return;
    }

    QMutexLocker mlock(&m_mutex);
    std::sort(m_vorChannels.begin(), m_vorChannels.end());
    std::vector<RRTurnPlan> devicesChannels;
    getChannelsByDevice(m_availableChannels, devicesChannels);
    QList<VORLocalizerSettings::VORChannel> unallocatedVORs(m_vorChannels);
    m_rrPlans.clear();
    int deviceCount = 0;

    for (auto deviceChannel : devicesChannels)
    {
        unsigned int nbChannels = unallocatedVORs.size() < (int) deviceChannel.m_channels.size() ?
            unallocatedVORs.size() :
            deviceChannel.m_channels.size();
        std::vector<VORRange> vorRanges;

        while (nbChannels != 0)
        {
            getVORRanges(unallocatedVORs, nbChannels, vorRanges);
            filterVORRanges(vorRanges, deviceChannel.m_bandwidth);

            if (vorRanges.size() != 0) {
                break;
            }

            nbChannels--;
        }

        std::vector<QList<VORLocalizerSettings::VORChannel>> vorLists;

        for (auto vorRange : vorRanges)
        {
            QList<VORLocalizerSettings::VORChannel> vorList;

            for (auto index : vorRange.m_vorIndices) {
                vorList.append(VORLocalizerSettings::VORChannel(unallocatedVORs[index]));
            }

            vorLists.push_back(vorList);
        }

        // make one round robin turn for each VOR list for this device
        std::vector<RRTurnPlan> rrDevicePlans;

        for (auto vorList : vorLists)
        {
            RRTurnPlan turnPlan(deviceChannel);
            int fMin = vorList.front().m_frequency;
            int fMax = vorList.back().m_frequency;
            int devFreq;
            if (turnPlan.m_fixedCenterFrequency) {
                devFreq = getDeviceCenterFrequency(turnPlan.m_device.m_deviceIndex);
            } else {
                devFreq = (fMin + fMax) / 2;
            }
            turnPlan.m_device.m_frequency = devFreq;
            int iCh = 0;

            // qDebug() << "RR build plan "
            //     << "device:" << turnPlan.m_device.m_deviceIndex
            //     << "freq:" << turnPlan.m_device.m_frequency;

            for (auto vorChannel : vorList)
            {
                RRChannel& channel = turnPlan.m_channels[iCh];
                channel.m_frequencyShift = vorChannel.m_frequency - devFreq;
                channel.m_navId = vorChannel.m_subChannelId;
                // qDebug() << "VOR channel" << vorChannel.m_subChannelId
                //     << "freq:" << vorChannel.m_frequency
                //     << "channel:" << channel.m_channelIndex
                //     << "shift:" << channel.m_frequencyShift;
                // remove VOR from the unallocated list
                QList<VORLocalizerSettings::VORChannel>::iterator it = unallocatedVORs.begin();
                while (it != unallocatedVORs.end())
                {
                    if (it->m_subChannelId == vorChannel.m_subChannelId) {
                        it = unallocatedVORs.erase(it);
                    } else {
                        ++it;
                    }
                }
                iCh++;
            }

            rrDevicePlans.push_back(turnPlan);
        }

        m_rrPlans.push_back(rrDevicePlans);
        deviceCount++;
    }

    qDebug() << "VorLocalizerWorker::updateChannels: unallocatedVORs size:" << unallocatedVORs.size();

    // Fallback for unallocated VORs: add single channel plans for all unallocated VORs
    if ((unallocatedVORs.size() != 0) && (devicesChannels.size() != 0) && m_rrPlans.size() != 0)
    {
        VorLocalizerWorker::RRTurnPlan& deviceChannel = devicesChannels.front();
        std::vector<VORRange> vorRanges;
        getVORRanges(unallocatedVORs, 1, vorRanges);
        std::vector<VorLocalizerWorker::RRTurnPlan>& rrPlan = m_rrPlans.front();
        std::vector<QList<VORLocalizerSettings::VORChannel>> vorLists;

        for (auto vorRange : vorRanges)
        {
            QList<VORLocalizerSettings::VORChannel> vorList;

            for (auto index : vorRange.m_vorIndices) {
                vorList.append(VORLocalizerSettings::VORChannel(unallocatedVORs[index]));
            }

            vorLists.push_back(vorList);
        }

        for (auto vorList : vorLists)
        {
            RRTurnPlan turnPlan(deviceChannel);
            int fMin = vorList.front().m_frequency;
            int fMax = vorList.back().m_frequency;
            int devFreq;
            if (turnPlan.m_fixedCenterFrequency) {
                devFreq = getDeviceCenterFrequency(turnPlan.m_device.m_deviceIndex);
            } else {
                devFreq = (fMin + fMax) / 2;
            }
            turnPlan.m_device.m_frequency = devFreq;
            int iCh = 0;

            // qDebug() << "RR build plan "
            //     << "device:" << turnPlan.m_device.m_deviceIndex
            //     << "freq:" << turnPlan.m_device.m_frequency;

            for (auto vorChannel : vorList)
            {
                RRChannel& channel = turnPlan.m_channels[iCh];
                channel.m_frequencyShift = vorChannel.m_frequency - devFreq;
                channel.m_navId = vorChannel.m_subChannelId;
                // qDebug() << "VOR channel" << vorChannel.m_subChannelId
                //     << "freq:" << vorChannel.m_frequency
                //     << "channel:" << channel.m_channelIndex
                //     << "shift:" << channel.m_frequencyShift;
                iCh++;
            }

            rrPlan.push_back(turnPlan);
        }
    }

    for (auto rrPlans : m_rrPlans)
    {
        qDebug() << "VorLocalizerWorker::updateChannels: RR plans for one device";

        for (auto rrPlan : rrPlans)
        {
            qDebug() << "VorLocalizerWorker::updateChannels:   RR plan: "
                << "device:" << rrPlan.m_device.m_deviceIndex
                << "frequency:" << rrPlan.m_device.m_frequency;

            for (auto rrChannel : rrPlan.m_channels)
            {
                qDebug("VorLocalizerWorker::updateChannels: RR channel: %p index: %d shift: %d navId: %d",
                    rrChannel.m_channelAPI, rrChannel.m_channelIndex, rrChannel.m_frequencyShift, rrChannel.m_navId);
            }
        }
    }

    m_rrTurnCounters.resize(deviceCount);
    std::fill(m_rrTurnCounters.begin(), m_rrTurnCounters.end(), 0);
    rrNextTurn();
}

void VorLocalizerWorker::setChannelShift(int deviceIndex, int channelIndex, double targetOffset, int vorNavId)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;

    // Get channel settings containg inputFrequencyOffset, so we can patch them
    httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsGet(
        deviceIndex,
        channelIndex,
        channelSettingsResponse,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("VorLocalizerWorker::setChannelShift: get channel offset frequency error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
    }

    QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();

    if (!WebAPIUtils::setSubObjectDouble(*jsonObj, "inputFrequencyOffset", targetOffset))
    {
        qWarning("VorLocalizerWorker::setChannelShift: No inputFrequencyOffset key in channel settings");
        return;
    }

    if (!WebAPIUtils::setSubObjectInt(*jsonObj, "navId", vorNavId))
    {
        qWarning("VorLocalizerWorker::setChannelShift: No navId key in channel settings");
        return;
    }

    QStringList channelSettingsKeys;

    if (m_settings.m_subChannelSettings.contains(vorNavId))
    {
        if (!WebAPIUtils::setSubObjectInt(*jsonObj, "audioMute", m_settings.m_subChannelSettings[vorNavId].m_audioMute ? 1 : 0)) {
            qWarning("VorLocalizerWorker::setChannelShift: No audioMute key in channel settings");
        } else {
            channelSettingsKeys.append("audioMute");
        }
    }

    channelSettingsKeys.append("inputFrequencyOffset");
    channelSettingsKeys.append("navId");
    channelSettingsResponse.init();
    channelSettingsResponse.fromJsonObject(*jsonObj);

    httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsPutPatch(
        deviceIndex,
        channelIndex,
        false, // PATCH
        channelSettingsKeys,
        channelSettingsResponse,
        errorResponse
    );

    if (httpRC/100 == 2)
    {
        qDebug("VorLocalizerWorker::setChannelShift: inputFrequencyOffset: %f navId: %d OK", targetOffset, vorNavId);
    }
    else
    {
        qWarning("VorLocalizerWorker::setChannelShift: set inputFrequencyOffset and navId error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
    }
}

void VorLocalizerWorker::setAudioMute(int vorNavId, bool audioMute)
{
    QMutexLocker mlock(&m_mutex);

    if (!m_channelAllocations.contains(vorNavId)) {
        return;
    }

    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;
    int deviceIndex = m_channelAllocations[vorNavId].m_deviceIndex;
    int channelIndex = m_channelAllocations[vorNavId].m_channelIndex;

    // Get channel settings containg inputFrequencyOffset, so we can patch them
    httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsGet(
        deviceIndex,
        channelIndex,
        channelSettingsResponse,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("VorLocalizerWorker::setChannelShift: get channel offset frequency error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
    }

    QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();

    if (!WebAPIUtils::setSubObjectInt(*jsonObj, "audioMute", audioMute ? 1 : 0))
    {
        qWarning("VorLocalizerWorker::setAudioMute: No audioMute key in channel settings");
        return;
    }

    QStringList channelSettingsKeys;
    channelSettingsKeys.append("audioMute");
    channelSettingsResponse.init();
    channelSettingsResponse.fromJsonObject(*jsonObj);

    httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsPutPatch(
        deviceIndex,
        channelIndex,
        false, // PATCH
        channelSettingsKeys,
        channelSettingsResponse,
        errorResponse
    );

    if (httpRC/100 == 2)
    {
        qDebug("VorLocalizerWorker::setAudioMute: navId: %d audioMute: %d OK", vorNavId, audioMute ? 1 : 0);
    }
    else
    {
        qWarning("VorLocalizerWorker::setAudioMute: navId: %d set audioMute error %d: %s",
            vorNavId, httpRC, qPrintable(*errorResponse.getMessage()));
    }
}

void VorLocalizerWorker::generateIndexCombinations(int length, int subLength, std::vector<std::vector<int>>& indexCombinations)
{
    indexCombinations.clear();
    std::vector<int> sublist(subLength);
    std::vector<int>::iterator first = sublist.begin(), last = sublist.end();
    std::iota(first, last, 0);
    indexCombinations.push_back(sublist);

    while ((*first) != length - subLength)
    {
        std::vector<int>::iterator mt = last;

        while (*(--mt) == length-(last-mt));
        (*mt)++;
        while (++mt != last) *mt = *(mt-1)+1;

        indexCombinations.push_back(std::vector<int>(first, last));
    }
}

void VorLocalizerWorker::getVORRanges(const QList<VORLocalizerSettings::VORChannel>& vors, int subLength, std::vector<VORRange>& vorRanges)
{
    std::vector<std::vector<int>> indexCombinations;
    generateIndexCombinations(vors.size(), subLength, indexCombinations);
    vorRanges.clear();

    for (auto indexCombination : indexCombinations)
    {
        int fMax = vors.at(indexCombination.back()).m_frequency;
        int fMin = vors.at(indexCombination.front()).m_frequency;
        vorRanges.push_back(VORRange{indexCombination, fMax - fMin});
    }
}

void VorLocalizerWorker::filterVORRanges(std::vector<VORRange>& vorRanges, int thresholdBW)
{
    std::vector<VORRange> originalVORRanges(vorRanges.size());
    std::copy(vorRanges.begin(), vorRanges.end(), originalVORRanges.begin());
    vorRanges.clear();

    for (auto vorRange : originalVORRanges)
    {
        if (vorRange.m_frequencyRange < thresholdBW) {
            vorRanges.push_back(vorRange);
        }
    }
}

void VorLocalizerWorker::getChannelsByDevice(
    const QHash<ChannelAPI*, VORLocalizerSettings::AvailableChannel> *availableChannels,
    std::vector<RRTurnPlan>& devicesChannels
)
{
    struct
    {
        bool operator()(const RRTurnPlan& a, const RRTurnPlan& b)
        {
            unsigned int nbChannelsA = a.m_channels.size();
            unsigned int nbChannelsB = a.m_channels.size();

            if (nbChannelsA == nbChannelsB) {
                return a.m_bandwidth > b.m_bandwidth;
            } else {
                return nbChannelsA > nbChannelsB;
            }
        }
    } rrTurnPlanGreater;

    QHash<ChannelAPI*, VORLocalizerSettings::AvailableChannel>::const_iterator itr = availableChannels->begin();
    QMap<int, RRTurnPlan> devicesChannelsMap;

    for (; itr != availableChannels->end(); ++itr)
    {
        devicesChannelsMap[itr->m_deviceSetIndex].m_device.m_deviceIndex = itr->m_deviceSetIndex;
        devicesChannelsMap[itr->m_deviceSetIndex].m_bandwidth = getDeviceSampleRate(itr->m_deviceSetIndex); // Get b/w of device, not channel, as the latter may be decimated
        devicesChannelsMap[itr->m_deviceSetIndex].m_channels.push_back(RRChannel{itr->m_channelAPI, itr->m_channelIndex, 0, -1});
    }

    QMap<int, RRTurnPlan>::iterator itm = devicesChannelsMap.begin();
    devicesChannels.clear();

    for (; itm != devicesChannelsMap.end(); ++itm)
    {
        itm->m_fixedCenterFrequency = hasCenterFrequencySetting(itm->m_device.m_deviceIndex);
        devicesChannels.push_back(*itm);
    }

    std::sort(devicesChannels.begin(), devicesChannels.end(), rrTurnPlanGreater);
}

void VorLocalizerWorker::rrNextTurn()
{
    QMutexLocker mlock(&m_mutex);
    int iDevPlan = 0;
    VORLocalizerReport::MsgReportServiceddVORs *msg = VORLocalizerReport::MsgReportServiceddVORs::create();
    m_channelAllocations.clear();

    for (auto rrPlan : m_rrPlans)
    {
        unsigned int turnCount = m_rrTurnCounters[iDevPlan];
        int deviceIndex = rrPlan[turnCount].m_device.m_deviceIndex;
        int deviceFrequency = rrPlan[turnCount].m_device.m_frequency - m_settings.m_centerShift;

        qDebug() << "VorLocalizerWorker::rrNextTurn: "
            << "turn:" << turnCount
            << "device:" << deviceIndex
            << "frequency:" << deviceFrequency - m_settings.m_centerShift;

        if (!rrPlan[turnCount].m_fixedCenterFrequency) {
            ChannelWebAPIUtils::setCenterFrequency(deviceIndex, deviceFrequency);
        }

        for (auto channel : rrPlan[turnCount].m_channels)
        {
            int shift = channel.m_frequencyShift;
            if (!rrPlan[turnCount].m_fixedCenterFrequency) {
                shift += m_settings.m_centerShift;
            }

            qDebug() << "VorLocalizerWorker::rrNextTurn: "
                << "device:" << deviceIndex
                << "channel:" << channel.m_channelIndex
                << "shift:" << shift
                << "navId:" << channel.m_navId;
            setChannelShift(
                deviceIndex,
                channel.m_channelIndex,
                shift,
                channel.m_navId
            );
            m_channelAllocations[channel.m_navId] = ChannelAllocation{
                channel.m_navId,
                deviceIndex,
                channel.m_channelIndex
            };

            if(m_availableChannels->contains(channel.m_channelAPI))
            {
                VORLocalizerSettings::AvailableChannel& availableChannel = m_availableChannels->operator[](channel.m_channelAPI);
                availableChannel.m_navId = channel.m_navId;
            }

            msg->getNavIds().push_back(channel.m_navId);
            msg->getSinglePlans()[channel.m_navId] = (rrPlan.size() == 1);
        }

        turnCount++;

        if (turnCount == rrPlan.size()) {
            turnCount = 0;
        }

        m_rrTurnCounters[iDevPlan] = turnCount;
        iDevPlan++;
    }

    if (m_msgQueueToFeature) {
        m_msgQueueToFeature->push(msg);
    }
}
