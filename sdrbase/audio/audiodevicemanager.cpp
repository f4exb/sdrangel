///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "util/messagequeue.h"
#include "dsp/dspcommands.h"

#include <QDataStream>
#include <QSet>
#include <QDebug>

const float AudioDeviceManager::m_defaultAudioInputVolume = 0.15f;
const QString AudioDeviceManager::m_defaultUDPAddress = "127.0.0.1";
const QString AudioDeviceManager::m_defaultDeviceName = "System default device";

QDataStream& operator<<(QDataStream& ds, const AudioDeviceManager::InputDeviceInfo& info)
{
    ds << info.sampleRate << info.volume;
    return ds;
}

QDataStream& operator>>(QDataStream& ds, AudioDeviceManager::InputDeviceInfo& info)
{
    ds >> info.sampleRate >> info.volume;
    return ds;
}

QDataStream& operator<<(QDataStream& ds, const AudioDeviceManager::OutputDeviceInfo& info)
{
    ds << info.sampleRate << info.udpAddress << info.udpPort << info.copyToUDP << info.udpUseRTP << (int) info.udpChannelMode;
    return ds;
}

QDataStream& operator>>(QDataStream& ds, AudioDeviceManager::OutputDeviceInfo& info)
{
    int intChannelMode;
    ds >> info.sampleRate >> info.udpAddress >> info.udpPort >> info.copyToUDP >> info.udpUseRTP >> intChannelMode;
    info.udpChannelMode = (AudioOutput::UDPChannelMode) intChannelMode;
    return ds;
}

AudioDeviceManager::AudioDeviceManager()
{
    m_inputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    m_outputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
}

AudioDeviceManager::~AudioDeviceManager()
{
    QMap<int, AudioOutput*>::iterator it = m_audioOutputs.begin();

    for (; it != m_audioOutputs.end(); ++it) {
        delete(*it);
    }
}

bool AudioDeviceManager::getOutputDeviceName(int outputDeviceIndex, QString &deviceName) const
{
    if (outputDeviceIndex < 0)
    {
        deviceName = m_defaultDeviceName;
        return true;
    }
    else
    {
        if (outputDeviceIndex < m_outputDevicesInfo.size())
        {
            deviceName = m_outputDevicesInfo[outputDeviceIndex].deviceName();
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool AudioDeviceManager::getInputDeviceName(int inputDeviceIndex, QString &deviceName) const
{
    if (inputDeviceIndex < 0)
    {
        deviceName = m_defaultDeviceName;
        return true;
    }
    else
    {
        if (inputDeviceIndex < m_inputDevicesInfo.size())
        {
            deviceName = m_inputDevicesInfo[inputDeviceIndex].deviceName();
            return true;
        }
        else
        {
            return false;
        }
    }
}

int AudioDeviceManager::getOutputDeviceIndex(const QString &deviceName) const
{
    for (int i = 0; i < m_outputDevicesInfo.size(); i++)
    {
        //qDebug("AudioDeviceManager::getOutputDeviceIndex: %d: %s|%s", i, qPrintable(deviceName), qPrintable(m_outputDevicesInfo[i].deviceName()));
        if (deviceName == m_outputDevicesInfo[i].deviceName()) {
            return i;
        }
    }

    return -1; // system default
}

int AudioDeviceManager::getInputDeviceIndex(const QString &deviceName) const
{
    for (int i = 0; i < m_inputDevicesInfo.size(); i++)
    {
        //qDebug("AudioDeviceManager::getInputDeviceIndex: %d: %s|%s", i, qPrintable(deviceName), qPrintable(m_inputDevicesInfo[i].deviceName()));
        if (deviceName == m_inputDevicesInfo[i].deviceName()) {
            return i;
        }
    }

    return -1; // system default
}


void AudioDeviceManager::resetToDefaults()
{
}

QByteArray AudioDeviceManager::serialize() const
{
    qDebug("AudioDeviceManager::serialize");
    debugAudioInputInfos();
    debugAudioOutputInfos();

    SimpleSerializer s(1);
    QByteArray data;

    serializeInputMap(data);
    s.writeBlob(1, data);
    serializeOutputMap(data);
    s.writeBlob(2, data);

    return s.final();
}

void AudioDeviceManager::serializeInputMap(QByteArray& data) const
{
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    *stream << m_audioInputInfos;
    delete stream;
}

void AudioDeviceManager::serializeOutputMap(QByteArray& data) const
{
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    *stream << m_audioOutputInfos;
    delete stream;
}

bool AudioDeviceManager::deserialize(const QByteArray& data)
{
    qDebug("AudioDeviceManager::deserialize");

    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray data;

        d.readBlob(1, &data);
        deserializeInputMap(data);
        d.readBlob(2, &data);
        deserializeOutputMap(data);

        debugAudioInputInfos();
        debugAudioOutputInfos();

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AudioDeviceManager::deserializeInputMap(QByteArray& data)
{
    QDataStream readStream(&data, QIODevice::ReadOnly);
    readStream >> m_audioInputInfos;
}

void AudioDeviceManager::deserializeOutputMap(QByteArray& data)
{
    QDataStream readStream(&data, QIODevice::ReadOnly);
    readStream >> m_audioOutputInfos;
}

void AudioDeviceManager::addAudioSink(AudioFifo* audioFifo, MessageQueue *sampleSinkMessageQueue, int outputDeviceIndex)
{
    qDebug("AudioDeviceManager::addAudioSink: %d: %p", outputDeviceIndex, audioFifo);

    if (m_audioOutputs.find(outputDeviceIndex) == m_audioOutputs.end()) {
        m_audioOutputs[outputDeviceIndex] = new AudioOutput();
    }

    if (m_audioOutputs[outputDeviceIndex]->getNbFifos() == 0) {
        startAudioOutput(outputDeviceIndex);
    }

    if (m_audioSinkFifos.find(audioFifo) == m_audioSinkFifos.end()) // new FIFO
    {
        m_audioOutputs[outputDeviceIndex]->addFifo(audioFifo);
        m_audioSinkFifos[audioFifo] = outputDeviceIndex; // register audio FIFO
        m_audioFifoToSinkMessageQueues[audioFifo] = sampleSinkMessageQueue;
        m_outputDeviceSinkMessageQueues[outputDeviceIndex].append(sampleSinkMessageQueue);
    }
    else
    {
        int audioOutputDeviceIndex = m_audioSinkFifos[audioFifo];

        if (audioOutputDeviceIndex != outputDeviceIndex) // change of audio device
        {
            removeAudioSink(audioFifo); // remove from current
            m_audioOutputs[outputDeviceIndex]->addFifo(audioFifo); // add to new
            m_audioSinkFifos[audioFifo] = outputDeviceIndex; // new index
            m_outputDeviceSinkMessageQueues[audioOutputDeviceIndex].removeOne(sampleSinkMessageQueue);
            m_outputDeviceSinkMessageQueues[outputDeviceIndex].append(sampleSinkMessageQueue);
        }
    }
}

void AudioDeviceManager::removeAudioSink(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceManager::removeAudioSink: %p", audioFifo);

    if (m_audioSinkFifos.find(audioFifo) == m_audioSinkFifos.end())
    {
        qWarning("AudioDeviceManager::removeAudioSink: audio FIFO %p not found", audioFifo);
        return;
    }

    int audioOutputDeviceIndex = m_audioSinkFifos[audioFifo];
    m_audioOutputs[audioOutputDeviceIndex]->removeFifo(audioFifo);

    if (m_audioOutputs[audioOutputDeviceIndex]->getNbFifos() == 0) {
        stopAudioOutput(audioOutputDeviceIndex);
    }

    m_audioSinkFifos.remove(audioFifo); // unregister audio FIFO
    m_outputDeviceSinkMessageQueues[audioOutputDeviceIndex].removeOne(m_audioFifoToSinkMessageQueues[audioFifo]);
    m_audioFifoToSinkMessageQueues.remove(audioFifo);
}

void AudioDeviceManager::addAudioSource(AudioFifo* audioFifo, MessageQueue *sampleSourceMessageQueue, int inputDeviceIndex)
{
    qDebug("AudioDeviceManager::addAudioSource: %d: %p", inputDeviceIndex, audioFifo);

    if (m_audioInputs.find(inputDeviceIndex) == m_audioInputs.end()) {
        m_audioInputs[inputDeviceIndex] = new AudioInput();
    }

    if (m_audioInputs[inputDeviceIndex]->getNbFifos() == 0) {
        startAudioInput(inputDeviceIndex);
    }

    if (m_audioSourceFifos.find(audioFifo) == m_audioSourceFifos.end()) // new FIFO
    {
        m_audioInputs[inputDeviceIndex]->addFifo(audioFifo);
    }
    else
    {
        int audioInputDeviceIndex = m_audioSourceFifos[audioFifo];

        if (audioInputDeviceIndex != inputDeviceIndex) // change of audio device
        {
            removeAudioSource(audioFifo); // remove from current
            m_audioInputs[inputDeviceIndex]->addFifo(audioFifo); // add to new
        }
    }

    m_audioSourceFifos[audioFifo] = inputDeviceIndex; // register audio FIFO
    m_audioFifoToSourceMessageQueues[audioFifo] = sampleSourceMessageQueue;
    m_outputDeviceSinkMessageQueues[inputDeviceIndex].append(sampleSourceMessageQueue);
}

void AudioDeviceManager::removeAudioSource(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceManager::removeAudioSource: %p", audioFifo);

    if (m_audioSourceFifos.find(audioFifo) == m_audioSourceFifos.end())
    {
        qWarning("AudioDeviceManager::removeAudioSource: audio FIFO %p not found", audioFifo);
        return;
    }

    int audioInputDeviceIndex = m_audioSourceFifos[audioFifo];
    m_audioInputs[audioInputDeviceIndex]->removeFifo(audioFifo);

    if (m_audioInputs[audioInputDeviceIndex]->getNbFifos() == 0) {
        stopAudioInput(audioInputDeviceIndex);
    }

    m_audioSourceFifos.remove(audioFifo); // unregister audio FIFO
    m_inputDeviceSourceMessageQueues[audioInputDeviceIndex].removeOne(m_audioFifoToSourceMessageQueues[audioFifo]);
    m_audioFifoToSourceMessageQueues.remove(audioFifo);
}

void AudioDeviceManager::startAudioOutput(int outputDeviceIndex)
{
    unsigned int sampleRate;
    QString udpAddress;
    quint16 udpPort;
    bool copyAudioToUDP;
    bool udpUseRTP;
    AudioOutput::UDPChannelMode udpChannelMode;
    QString deviceName;

    if (getOutputDeviceName(outputDeviceIndex, deviceName))
    {
        if (m_audioOutputInfos.find(deviceName) == m_audioOutputInfos.end())
        {
            sampleRate = m_defaultAudioSampleRate;
            udpAddress = m_defaultUDPAddress;
            udpPort = m_defaultUDPPort;
            copyAudioToUDP = false;
            udpUseRTP = false;
            udpChannelMode = AudioOutput::UDPChannelLeft;
        }
        else
        {
            sampleRate = m_audioOutputInfos[deviceName].sampleRate;
            udpAddress = m_audioOutputInfos[deviceName].udpAddress;
            udpPort = m_audioOutputInfos[deviceName].udpPort;
            copyAudioToUDP = m_audioOutputInfos[deviceName].copyToUDP;
            udpUseRTP = m_audioOutputInfos[deviceName].udpUseRTP;
            udpChannelMode = m_audioOutputInfos[deviceName].udpChannelMode;
        }

        m_audioOutputs[outputDeviceIndex]->start(outputDeviceIndex, sampleRate);
        m_audioOutputInfos[deviceName].sampleRate = m_audioOutputs[outputDeviceIndex]->getRate(); // update with actual rate
        m_audioOutputInfos[deviceName].udpAddress = udpAddress;
        m_audioOutputInfos[deviceName].udpPort = udpPort;
        m_audioOutputInfos[deviceName].copyToUDP = copyAudioToUDP;
        m_audioOutputInfos[deviceName].udpUseRTP = udpUseRTP;
        m_audioOutputInfos[deviceName].udpChannelMode = udpChannelMode;
    }
    else
    {
        qWarning("AudioDeviceManager::startAudioOutput: unknown device index %d", outputDeviceIndex);
    }
}

void AudioDeviceManager::stopAudioOutput(int outputDeviceIndex)
{
    m_audioOutputs[outputDeviceIndex]->stop();
}

void AudioDeviceManager::startAudioInput(int inputDeviceIndex)
{
    unsigned int sampleRate;
    float volume;
    QString deviceName;

    if (getInputDeviceName(inputDeviceIndex, deviceName))
    {
        if (m_audioInputInfos.find(deviceName) == m_audioInputInfos.end())
        {
            sampleRate = m_defaultAudioSampleRate;
            volume = m_defaultAudioInputVolume;
        }
        else
        {
            sampleRate = m_audioInputInfos[deviceName].sampleRate;
            volume = m_audioInputInfos[deviceName].volume;
        }

        m_audioInputs[inputDeviceIndex]->start(inputDeviceIndex, sampleRate);
        m_audioInputs[inputDeviceIndex]->setVolume(volume);
        m_audioInputInfos[deviceName].sampleRate = m_audioInputs[inputDeviceIndex]->getRate();
        m_audioInputInfos[deviceName].volume = volume;
    }
    else
    {
        qWarning("AudioDeviceManager::startAudioInput: unknown device index %d", inputDeviceIndex);
    }
}

void AudioDeviceManager::stopAudioInput(int inputDeviceIndex)
{
    m_audioInputs[inputDeviceIndex]->stop();
}

bool AudioDeviceManager::getInputDeviceInfo(const QString& deviceName, InputDeviceInfo& deviceInfo) const
{
    if (m_audioInputInfos.find(deviceName) == m_audioInputInfos.end())
    {
        return false;
    }
    else
    {
        deviceInfo = m_audioInputInfos[deviceName];
        return true;
    }
}

bool AudioDeviceManager::getOutputDeviceInfo(const QString& deviceName, OutputDeviceInfo& deviceInfo) const
{
    if (m_audioOutputInfos.find(deviceName) == m_audioOutputInfos.end())
    {
        return false;
    }
    else
    {
        deviceInfo = m_audioOutputInfos[deviceName];
        return true;
    }
}

int AudioDeviceManager::getInputSampleRate(int inputDeviceIndex)
{
    QString deviceName;

    if (!getInputDeviceName(inputDeviceIndex, deviceName))
    {
        qDebug("AudioDeviceManager::getInputSampleRate: unknown device index %d", inputDeviceIndex);
        return m_defaultAudioSampleRate;
    }

    InputDeviceInfo deviceInfo;

    if (!getInputDeviceInfo(deviceName, deviceInfo))
    {
        qDebug("AudioDeviceManager::getInputSampleRate: unknown device %s", qPrintable(deviceName));
        return m_defaultAudioSampleRate;
    }
    else
    {
        return deviceInfo.sampleRate;
    }
}

int AudioDeviceManager::getOutputSampleRate(int outputDeviceIndex)
{
    QString deviceName;

    if (!getOutputDeviceName(outputDeviceIndex, deviceName))
    {
        qDebug("AudioDeviceManager::getOutputSampleRate: unknown device index %d", outputDeviceIndex);
        return m_defaultAudioSampleRate;
    }

    OutputDeviceInfo deviceInfo;

    if (!getOutputDeviceInfo(deviceName, deviceInfo))
    {
        qDebug("AudioDeviceManager::getOutputSampleRate: unknown device %s", qPrintable(deviceName));
        return m_defaultAudioSampleRate;
    }
    else
    {
        return deviceInfo.sampleRate;
    }
}


void AudioDeviceManager::setInputDeviceInfo(int inputDeviceIndex, const InputDeviceInfo& deviceInfo)
{
    QString deviceName;

    if (!getInputDeviceName(inputDeviceIndex, deviceName))
    {
        qWarning("AudioDeviceManager::setInputDeviceInfo: unknown device index %d", inputDeviceIndex);
        return;
    }

    InputDeviceInfo oldDeviceInfo;

    if (!getInputDeviceInfo(deviceName, oldDeviceInfo))
    {
        qDebug("AudioDeviceManager::setInputDeviceInfo: unknown device %s", qPrintable(deviceName));
    }

    m_audioInputInfos[deviceName] = deviceInfo;

    if (m_audioInputs.find(inputDeviceIndex) == m_audioInputs.end()) { // no FIFO registered yet hence no audio input has been allocated yet
        return;
    }

    AudioInput *audioInput = m_audioInputs[inputDeviceIndex];

    if (oldDeviceInfo.sampleRate != deviceInfo.sampleRate)
    {
        audioInput->stop();
        audioInput->start(inputDeviceIndex, deviceInfo.sampleRate);
        m_audioInputInfos[deviceName].sampleRate = audioInput->getRate(); // store actual sample rate

        // send message to attached channels
        QList<MessageQueue *>::const_iterator it = m_inputDeviceSourceMessageQueues[inputDeviceIndex].begin();

        for (; it != m_inputDeviceSourceMessageQueues[inputDeviceIndex].end(); ++it)
        {
            DSPConfigureAudio *msg = new DSPConfigureAudio(m_audioInputInfos[deviceName].sampleRate);
            (*it)->push(msg);
        }
    }

    audioInput->setVolume(deviceInfo.volume);
}

void AudioDeviceManager::setOutputDeviceInfo(int outputDeviceIndex, const OutputDeviceInfo& deviceInfo)
{
    QString deviceName;

    if (!getOutputDeviceName(outputDeviceIndex, deviceName))
    {
        qWarning("AudioDeviceManager::setOutputDeviceInfo: unknown device index %d", outputDeviceIndex);
        return;
    }

    OutputDeviceInfo oldDeviceInfo;

    if (!getOutputDeviceInfo(deviceName, oldDeviceInfo))
    {
        qInfo("AudioDeviceManager::setOutputDeviceInfo: unknown device %s", qPrintable(deviceName));
    }

    m_audioOutputInfos[deviceName] = deviceInfo;

    if (m_audioOutputs.find(outputDeviceIndex) == m_audioOutputs.end())
    {
        qWarning("AudioDeviceManager::setOutputDeviceInfo: index: %d device: %s no FIFO registered yet hence no audio output has been allocated yet",
                outputDeviceIndex, qPrintable(deviceName));
        return;
    }

    AudioOutput *audioOutput = m_audioOutputs[outputDeviceIndex];

    if (oldDeviceInfo.sampleRate != deviceInfo.sampleRate)
    {
        audioOutput->stop();
        audioOutput->start(outputDeviceIndex, deviceInfo.sampleRate);
        m_audioOutputInfos[deviceName].sampleRate = audioOutput->getRate(); // store actual sample rate

        // send message to attached channels
        QList<MessageQueue *>::const_iterator it = m_outputDeviceSinkMessageQueues[outputDeviceIndex].begin();

        for (; it != m_outputDeviceSinkMessageQueues[outputDeviceIndex].end(); ++it)
        {
            DSPConfigureAudio *msg = new DSPConfigureAudio(m_audioOutputInfos[deviceName].sampleRate);
            (*it)->push(msg);
        }
    }

    audioOutput->setUdpCopyToUDP(deviceInfo.copyToUDP);
    audioOutput->setUdpDestination(deviceInfo.udpAddress, deviceInfo.udpPort);
    audioOutput->setUdpUseRTP(deviceInfo.udpUseRTP);
    audioOutput->setUdpChannelMode(deviceInfo.udpChannelMode);
    audioOutput->setUdpChannelFormat(deviceInfo.udpChannelMode == AudioOutput::UDPChannelStereo, deviceInfo.sampleRate);

    qDebug("AudioDeviceManager::setOutputDeviceInfo: index: %d device: %s updated",
            outputDeviceIndex, qPrintable(deviceName));
}

void AudioDeviceManager::unsetOutputDeviceInfo(int outputDeviceIndex)
{
    QString deviceName;

    if (!getOutputDeviceName(outputDeviceIndex, deviceName))
    {
        qWarning("AudioDeviceManager::unsetOutputDeviceInfo: unknown device index %d", outputDeviceIndex);
        return;
    }

    OutputDeviceInfo oldDeviceInfo;

    if (!getOutputDeviceInfo(deviceName, oldDeviceInfo))
    {
        qDebug("AudioDeviceManager::unsetOutputDeviceInfo: unregistered device %s", qPrintable(deviceName));
        return;
    }

    m_audioOutputInfos.remove(deviceName);

    if (m_audioOutputs.find(outputDeviceIndex) == m_audioOutputs.end()) { // no FIFO registered yet hence no audio output has been allocated yet
        return;
    }

    stopAudioOutput(outputDeviceIndex);
    startAudioOutput(outputDeviceIndex);

    if (oldDeviceInfo.sampleRate != m_audioOutputInfos[deviceName].sampleRate)
    {
        // send message to attached channels
        QList<MessageQueue *>::const_iterator it = m_outputDeviceSinkMessageQueues[outputDeviceIndex].begin();

        for (; it != m_outputDeviceSinkMessageQueues[outputDeviceIndex].end(); ++it)
        {
            DSPConfigureAudio *msg = new DSPConfigureAudio(m_audioOutputInfos[deviceName].sampleRate);
            (*it)->push(msg);
        }
    }
}

void AudioDeviceManager::unsetInputDeviceInfo(int inputDeviceIndex)
{
    QString deviceName;

    if (!getInputDeviceName(inputDeviceIndex, deviceName))
    {
        qWarning("AudioDeviceManager::unsetInputDeviceInfo: unknown device index %d", inputDeviceIndex);
        return;
    }

    InputDeviceInfo oldDeviceInfo;

    if (!getInputDeviceInfo(deviceName, oldDeviceInfo))
    {
        qDebug("AudioDeviceManager::unsetInputDeviceInfo: unregistered device %s", qPrintable(deviceName));
        return;
    }

    m_audioInputInfos.remove(deviceName);

    if (m_audioInputs.find(inputDeviceIndex) == m_audioInputs.end()) { // no FIFO registered yet hence no audio input has been allocated yet
        return;
    }

    stopAudioInput(inputDeviceIndex);
    startAudioInput(inputDeviceIndex);

    if (oldDeviceInfo.sampleRate != m_audioInputInfos[deviceName].sampleRate)
    {
        // send message to attached channels
        QList<MessageQueue *>::const_iterator it = m_inputDeviceSourceMessageQueues[inputDeviceIndex].begin();

        for (; it != m_inputDeviceSourceMessageQueues[inputDeviceIndex].end(); ++it)
        {
            DSPConfigureAudio *msg = new DSPConfigureAudio(m_audioInputInfos[deviceName].sampleRate);
            (*it)->push(msg);
        }
    }
}

void AudioDeviceManager::inputInfosCleanup()
{
    QSet<QString> deviceNames;
    deviceNames.insert(m_defaultDeviceName);
    QList<QAudioDeviceInfo>::const_iterator itd = m_inputDevicesInfo.begin();

    for (; itd != m_inputDevicesInfo.end(); ++itd) {
        deviceNames.insert(itd->deviceName());
    }

    QMap<QString, InputDeviceInfo>::iterator itm = m_audioInputInfos.begin();

    for (; itm != m_audioInputInfos.end(); ++itm)
    {
        if (!deviceNames.contains(itm.key()))
        {
            qDebug("AudioDeviceManager::inputInfosCleanup: removing key: %s", qPrintable(itm.key()));
            m_audioInputInfos.remove(itm.key());
        }
    }
}

void AudioDeviceManager::outputInfosCleanup()
{
    QSet<QString> deviceNames;
    deviceNames.insert(m_defaultDeviceName);
    QList<QAudioDeviceInfo>::const_iterator itd = m_outputDevicesInfo.begin();

    for (; itd != m_outputDevicesInfo.end(); ++itd) {
        deviceNames.insert(itd->deviceName());
    }

    QMap<QString, OutputDeviceInfo>::iterator itm = m_audioOutputInfos.begin();

    for (; itm != m_audioOutputInfos.end(); ++itm)
    {
        if (!deviceNames.contains(itm.key()))
        {
            qDebug("AudioDeviceManager::outputInfosCleanup: removing key: %s", qPrintable(itm.key()));
            m_audioOutputInfos.remove(itm.key());
        }
    }
}

void AudioDeviceManager::debugAudioInputInfos() const
{
    QMap<QString, InputDeviceInfo>::const_iterator it = m_audioInputInfos.begin();

    for (; it != m_audioInputInfos.end(); ++it)
    {
        qDebug() << "AudioDeviceManager::debugAudioInputInfos:"
                << " name: " << it.key()
                << " sampleRate: " << it.value().sampleRate
                << " volume: " << it.value().volume;
    }
}

void AudioDeviceManager::debugAudioOutputInfos() const
{
    QMap<QString, OutputDeviceInfo>::const_iterator it = m_audioOutputInfos.begin();

    for (; it != m_audioOutputInfos.end(); ++it)
    {
        qDebug() << "AudioDeviceManager::debugAudioOutputInfos:"
                << " name: " << it.key()
                << " sampleRate: " << it.value().sampleRate
                << " udpAddress: " << it.value().udpAddress
                << " udpPort: " << it.value().udpPort
                << " copyToUDP: " << it.value().copyToUDP
                << " udpUseRTP: " << it.value().udpUseRTP
                << " udpChannelMode: " << (int) it.value().udpChannelMode;
    }
}
