///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel Contributors                                      //
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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMODBASEBAND_H
#define PLUGINS_CHANNELTX_MODCW_CWMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "cwmodsource.h"

class UpChannelizer;
class ChannelAPI;

/**
 * Baseband processing thread for the CW modulator.
 *
 * Owns the UpChannelizer and CWModSource, feeding the sample FIFO with
 * upsampled, frequency-shifted CW I/Q samples.
 */
class CWModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureCWModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const CWModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureCWModBaseband* create(const QStringList& settingsKeys, const CWModSettings& settings, bool force)
        {
            return new MsgConfigureCWModBaseband(settingsKeys, settings, force);
        }

    private:
        CWModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureCWModBaseband(const QStringList& settingsKeys, const CWModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    CWModBaseband();
    ~CWModBaseband();
    void reset();
    void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue* messageQueue) { m_source.setMessageQueueToGUI(messageQueue); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    void setSpectrumSampleSink(BasebandSampleSink* sampleSink) { m_source.setSpectrumSink(sampleSink); }
    void setChannel(ChannelAPI *channel);
    int getSourceChannelSampleRate() const { return m_source.getChannelSampleRate(); }

signals:
    /**
     * Level changed signal.
     * \param rmsLevel  RMS level in the range [0.0, 1.0]
     * \param peakLevel Peak level in the range [0.0, 1.0]
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    CWModSource m_source;
    MessageQueue m_inputMessageQueue;
    CWModSettings m_settings;
    QRecursiveMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMODBASEBAND_H
