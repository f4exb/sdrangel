///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_DVBDEMODBASEBAND_H
#define INCLUDE_DVBDEMODBASEBAND_H

#include <atomic>

#include <QElapsedTimer>
#include <QObject>
#include <QMutex>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "dvbdemodsink.h"

class DownChannelizer;

class DVBDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureDVBDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DVBDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDVBDemodBaseband* create(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureDVBDemodBaseband(settings, settingsKeys, force);
        }

    private:
        DVBDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDVBDemodBaseband(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    //! The device or channel has been retuned: the receivers acquire again, and in Auto search for both standards
    class MsgResetReceivers : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgResetReceivers* create() { return new MsgResetReceivers(); }

    private:
        MsgResetReceivers() : Message() { }
    };

    //! The symbol rate searched for again, in a band (a transponder scan's). Sent by setRateSearchBand()
    class MsgRateSearchBand : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getFrequency() const { return m_frequency; }
        double getWidth() const { return m_width; }
        bool getSpectrumOnly() const { return m_spectrumOnly; }
        uint64_t getGeneration() const { return m_generation; }
        static MsgRateSearchBand* create(double frequency, double width, bool spectrumOnly, uint64_t generation) {
            return new MsgRateSearchBand(frequency, width, spectrumOnly, generation);
        }

    private:
        double m_frequency;
        double m_width;
        bool m_spectrumOnly;
        uint64_t m_generation;
        MsgRateSearchBand(double frequency, double width, bool spectrumOnly, uint64_t generation) :
            Message(), m_frequency(frequency), m_width(width), m_spectrumOnly(spectrumOnly), m_generation(generation) { }
    };

    DVBDemodBaseband();
    ~DVBDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    //! The symbol rate search's band (see DVBDemodSink::setRateSearchBand()), its generation given back in the status
    //! once it's applied. Kept, and set again by reapplyRateSearchBand(), as the message is lost if the queue's cleared
    //! (by reset(), as the device starts) before it's handled. From any thread
    void setRateSearchBand(double frequency, double width, bool spectrumOnly, uint64_t generation);
    //! The band set last again, if one has been (after reset(), as the device starts). From any thread
    void reapplyRateSearchBand();
    int getChannelSampleRate() const;
    void getStatus(DVBDemodSink::Status& status) { m_sink.getStatus(status); }
    DTVTSFifo *getTSFifo() { return m_sink.getTSFifo(); }
    AudioFifo *getAudioFifo() { return m_sink.getAudioFifo(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }
    //! The audio output's sample rate, and how long it buffers audio
    void getAudioOutput(int& sampleRate, double& latency) const;
    DVBDemodEPG *getEPG() { return m_sink.getEPG(); }
    DTVApps *getApps() { return m_sink.getApps(); }
    int startRecording(const QString& fileName, DTVRecorder::Mode mode, int serviceId, QString& error) {
        return m_sink.startRecording(fileName, mode, serviceId, error);
    }
    void stopRecording(int id) { m_sink.stopRecording(id); }
    std::vector<DVBDemodSink::Recording> getRecordings() { return m_sink.getRecordings(); }

private:
    SampleSinkFifo m_sampleFifo;
    uint64_t m_droppedSearching = 0;    //!< Samples dropped behind while searching, since last logged
    QElapsedTimer m_dropLogTimer;
    DownChannelizer *m_channelizer;
    DVBDemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    DVBDemodSettings m_settings;
    bool m_running;
    QRecursiveMutex m_mutex;
    std::atomic<int> m_audioSampleRate;
    //! Of the audio device, read here (as the sample rate is) where the sink's added to it and when it reports its
    //! rate, in this thread: the device manager isn't to be read from another while sinks are added in this one
    std::atomic<double> m_audioLatency;
    std::atomic<int> m_audioDeviceIndex;
    QMutex m_bandMutex;                 //!< For the following
    double m_bandFrequency = 0.0;       //!< The rate search's band, as set last
    double m_bandWidth = 0.0;
    bool m_bandSpectrumOnly = false;
    uint64_t m_bandGeneration = 0;      //!< 0 if none's been set

    bool handleMessage(const Message& cmd);
    void applySettings(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force = false);
    void setChannelization();

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_DVBDEMODBASEBAND_H
