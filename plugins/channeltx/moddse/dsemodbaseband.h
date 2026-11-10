///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODBASEBAND_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODBASEBAND_H_

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "dsemodsource.h"

class UpChannelizer;
class ChannelAPI;
class BasebandSampleSink;
class Feature;

class DSEModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureDSEModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DSEModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDSEModBaseband* create(const QStringList& settingsKeys, const DSEModSettings& settings, bool force) {
            return new MsgConfigureDSEModBaseband(settingsKeys, settings, force);
        }

    private:
        DSEModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDSEModBaseband(const QStringList& settingsKeys, const DSEModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    DSEModBaseband();
    ~DSEModBaseband();
    void reset();
    void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    CWKeyer& getCWKeyer() { return m_source.getCWKeyer(); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getAudioSampleRate() const { return m_source.getAudioSampleRate(); }
    int getPcmSampleRate() const { return m_source.getPcmSampleRate(); }
    int getChannelSampleRate() const;
    void setInputFile(DSEModAudioFile *file, QRecursiveMutex *mutex) { m_source.setInputFile(file, mutex); }
    AudioFifo *getAudioFifo() { return m_source.getAudioFifo(); }
    void setSpectrumSampleSink(BasebandSampleSink* sampleSink) { m_source.setSpectrumSink(sampleSink); }
    void setChannel(ChannelAPI *channel);
    void setAMBEFeature(Feature *feature, std::shared_ptr<MbeEncoderPort> port) { m_source.setAMBEFeature(feature, port); }
    DSEModProcessor *getProcessor() { return m_source.getProcessor(); }
    uint32_t getAudioUnderrunCount() const { return m_source.getAudioUnderrunCount(); }
    uint32_t getAudioOverrunCount() const { return m_source.getAudioOverrunCount(); }

signals:
    /**
     * Level changed
     * \param rmsLevel RMS level in range 0.0 - 1.0
     * \param peakLevel Peak level in range 0.0 - 1.0
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    DSEModSource m_source;
    MessageQueue m_inputMessageQueue;
    DSEModSettings m_settings;
    QRecursiveMutex m_mutex;
    std::atomic<quint64> m_apiProduced{0}, m_apiPulled{0}, m_apiDrainAt{0};

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODBASEBAND_H_
