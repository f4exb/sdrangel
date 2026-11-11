///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2018-2019, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#ifndef INCLUDE_DSPCOMMANDS_H
#define INCLUDE_DSPCOMMANDS_H

#include <QString>
#include <QMutex>

#include <algorithm>
#include <functional>
#include <memory>

#include "dsp/dsptypes.h"
#include "util/message.h"
#include "export.h"

class DeviceSampleSource;
class BasebandSampleSink;
class DeviceSampleSink;
class BasebandSampleSource;
class AudioFifo;
class MessageQueue;

class SDRBASE_API DSPAcquisitionInit : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRBASE_API DSPAcquisitionStart : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRBASE_API DSPAcquisitionStop : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRBASE_API DSPAcquisitionError : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    DSPAcquisitionError(const QString& errorMessage) :
        m_errorMessage(errorMessage)
    { }

    const QString& getErrorMessage() const { return m_errorMessage; }

private:
    QString m_errorMessage;
};

class SDRBASE_API DSPGenerationInit : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRBASE_API DSPGenerationStart : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRBASE_API DSPGenerationStop : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRBASE_API DSPGenerationError : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    DSPGenerationError(const QString& errorMessage) :
        m_errorMessage(errorMessage)
    { }

    const QString& getErrorMessage() const { return m_errorMessage; }

private:
    QString m_errorMessage;
};

class SDRBASE_API DSPGetSourceDeviceDescription : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	void setDeviceDescription(const QString& text) { m_deviceDescription = text; }
	const QString& getDeviceDescription() const { return m_deviceDescription; }

private:
	QString m_deviceDescription;
};

class SDRBASE_API DSPGetSinkDeviceDescription : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	void setDeviceDescription(const QString& text) { m_deviceDescription = text; }
	const QString& getDeviceDescription() const { return m_deviceDescription; }

private:
	QString m_deviceDescription;
};

class SDRBASE_API DSPGetErrorMessage : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	void setErrorMessage(const QString& text) { m_errorMessage = text; }
	const QString& getErrorMessage() const { return m_errorMessage; }

private:
	QString m_errorMessage;
};

class SDRBASE_API DSPSetSource : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPSetSource(DeviceSampleSource* sampleSource) : Message(), m_sampleSource(sampleSource) { }

	DeviceSampleSource* getSampleSource() const { return m_sampleSource; }

private:
	DeviceSampleSource* m_sampleSource;
};

class SDRBASE_API DSPSetSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPSetSink(DeviceSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	DeviceSampleSink* getSampleSink() const { return m_sampleSink; }

private:
	DeviceSampleSink* m_sampleSink;
};

class SDRBASE_API DSPAddBasebandSampleSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPAddBasebandSampleSink(BasebandSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	BasebandSampleSink* getSampleSink() const { return m_sampleSink; }

private:
	BasebandSampleSink* m_sampleSink;
};

class SDRBASE_API DSPAddSpectrumSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPAddSpectrumSink(BasebandSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	BasebandSampleSink* getSampleSink() const { return m_sampleSink; }

private:
	BasebandSampleSink* m_sampleSink;
};

class SDRBASE_API DSPAddBasebandSampleSource : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPAddBasebandSampleSource(BasebandSampleSource* sampleSource) : Message(), m_sampleSource(sampleSource) { }

	BasebandSampleSource* getSampleSource() const { return m_sampleSource; }

private:
	BasebandSampleSource* m_sampleSource;
};

class SDRBASE_API DSPRemoveBasebandSampleSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPRemoveBasebandSampleSink(BasebandSampleSink* sampleSink, bool deleting) : Message(), m_sampleSink(sampleSink), m_deleting(deleting) { }

	BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
	bool getDeleting() const { return m_deleting; }

private:
	BasebandSampleSink* m_sampleSink;
	bool m_deleting;
};

class SDRBASE_API DSPRemoveSpectrumSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPRemoveSpectrumSink(BasebandSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	BasebandSampleSink* getSampleSink() const { return m_sampleSink; }

private:
	BasebandSampleSink* m_sampleSink;
};

class SDRBASE_API DSPRemoveBasebandSampleSource : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPRemoveBasebandSampleSource(BasebandSampleSource* sampleSource, bool deleting) : Message(), m_sampleSource(sampleSource), m_deleting(deleting) { }

	BasebandSampleSource* getSampleSource() const { return m_sampleSource; }
	bool getDeleting() const { return m_deleting; }

private:
	BasebandSampleSource* m_sampleSource;
	bool m_deleting;
};

class SDRBASE_API DSPAddAudioSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPAddAudioSink(AudioFifo* audioFifo) : Message(), m_audioFifo(audioFifo) { }

	AudioFifo* getAudioFifo() const { return m_audioFifo; }

private:
	AudioFifo* m_audioFifo;
};

class SDRBASE_API DSPRemoveAudioSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPRemoveAudioSink(AudioFifo* audioFifo) : Message(), m_audioFifo(audioFifo) { }

	AudioFifo* getAudioFifo() const { return m_audioFifo; }

private:
	AudioFifo* m_audioFifo;
};

class SDRBASE_API DSPConfigureCorrection : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPConfigureCorrection(bool dcOffsetCorrection, bool iqImbalanceCorrection) :
		Message(),
		m_dcOffsetCorrection(dcOffsetCorrection),
		m_iqImbalanceCorrection(iqImbalanceCorrection)
	{ }

	bool getDCOffsetCorrection() const { return m_dcOffsetCorrection; }
	bool getIQImbalanceCorrection() const { return m_iqImbalanceCorrection; }

private:
	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;

};

class SDRBASE_API DSPEngineReport : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPEngineReport(int sampleRate, quint64 centerFrequency) :
		Message(),
		m_sampleRate(sampleRate),
		m_centerFrequency(centerFrequency)
	{ }

	int getSampleRate() const { return m_sampleRate; }
	quint64 getCenterFrequency() const { return m_centerFrequency; }

private:
	int m_sampleRate;
	quint64 m_centerFrequency;
};

class SDRBASE_API DSPConfigureScopeVis : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPConfigureScopeVis(int triggerChannel, Real triggerLevelHigh, Real triggerLevelLow) :
		Message(),
		m_triggerChannel(triggerChannel),
		m_triggerLevelHigh(triggerLevelHigh),
		m_triggerLevelLow(triggerLevelLow)
	{ }

	int getTriggerChannel() const { return m_triggerChannel; }
	Real getTriggerLevelHigh() const { return m_triggerLevelHigh; }
	Real getTriggerLevelLow() const { return m_triggerLevelLow; }

private:
	int m_triggerChannel;
	Real m_triggerLevelHigh;
	Real m_triggerLevelLow;
};

class SDRBASE_API DSPSignalNotification : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPSignalNotification(int samplerate, qint64 centerFrequency, bool realElseComplex = false) :
		Message(),
		m_sampleRate(samplerate),
		m_centerFrequency(centerFrequency),
        m_realElseComplex(realElseComplex)
	{ }

	int getSampleRate() const { return m_sampleRate; }
	qint64 getCenterFrequency() const { return m_centerFrequency; }
    bool getRealElseComplex() const { return m_realElseComplex; }

private:
	int m_sampleRate;
	qint64 m_centerFrequency;
    bool m_realElseComplex;
};

class SDRBASE_API DSPMIMOSignalNotification : public Message {
	MESSAGE_CLASS_DECLARATION
public:
	DSPMIMOSignalNotification(int samplerate, qint64 centerFrequency, bool sourceOrSink, unsigned int index, bool realElseComplex = false) :
		Message(),
		m_sampleRate(samplerate),
		m_centerFrequency(centerFrequency),
        m_realElseComplex(realElseComplex),
		m_sourceOrSink(sourceOrSink),
		m_index(index)
	{ }
	int getSampleRate() const { return m_sampleRate; }
	qint64 getCenterFrequency() const { return m_centerFrequency; }
    bool getRealElseComplex() const { return m_realElseComplex; }
	bool getSourceOrSink() const { return m_sourceOrSink; }
	unsigned int getIndex() const { return m_index; }
private:
	int m_sampleRate;
	qint64 m_centerFrequency;
    bool m_realElseComplex;
	bool m_sourceOrSink;
	unsigned int m_index;
};

class SDRBASE_API DSPConfigureChannelizer : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPConfigureChannelizer(int sampleRate, int centerFrequency) :
		Message(),
		m_sampleRate(sampleRate),
		m_centerFrequency(centerFrequency)
	{ }

	int getSampleRate() const { return m_sampleRate; }
	int getCenterFrequency() const { return m_centerFrequency; }

private:
	int m_sampleRate;
	int m_centerFrequency;
};

class SDRBASE_API DSPConfigureAudio : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    enum AudioType
    {
        AudioInput,
        AudioOutput
    };

    DSPConfigureAudio(int sampleRate, AudioType audioType) :
        m_sampleRate(sampleRate),
        m_autioType(audioType)
    { }

    int getSampleRate() const { return m_sampleRate; }
    AudioType getAudioType() const { return m_autioType; }

private:
    int m_sampleRate;
    AudioType m_autioType;
};

class SDRBASE_API DSPPushMbeFrame : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPPushMbeFrame(
        const unsigned char *mbeFrame,
        int mbeRateIndex,
        int mbeVolumeIndex,
        unsigned char channels,
        bool useHP,
        int upsampling,
        AudioFifo *audioFifo
    ) :
		Message(),
		m_mbeFrame(mbeFrame),
		m_mbeRateIndex(mbeRateIndex),
        m_mbeVolumeIndex(mbeVolumeIndex),
        m_channels(channels),
        m_useHP(useHP),
        m_upsampling(upsampling),
        m_audioFifo(audioFifo)
	{ }

	const unsigned char * getMbeFrame() const { return m_mbeFrame; }
	int getMbeRateIndex() const { return m_mbeRateIndex; }
    int getMbeVolumeIndex() const { return m_mbeVolumeIndex; }
    unsigned char getChannels() const { return m_channels; }
    bool getUseHP() const { return m_useHP; }
    int getUpsampling() const { return m_upsampling; }
    AudioFifo *getAudioFifo() const { return m_audioFifo; }

private:
    const unsigned char *m_mbeFrame;
    int m_mbeRateIndex;
    int m_mbeVolumeIndex;
    unsigned char m_channels;
    bool m_useHP;
    int m_upsampling;
    AudioFifo *m_audioFifo;
};


/**
 * Push one 160 sample 16 bit PCM frame (8 kHz) to an AMBE encoder (feature).
 * The receiver copies the audio synchronously. The encoded frame is returned
 * asynchronously as a DSPReportMbeFrame on the reply queue.
 */
class SDRBASE_API DSPPushMbeAudio : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    DSPPushMbeAudio(
        const short *audio,
        int mbeRateIndex,
        int gain,
        unsigned int sequence,
        const void *client,
        MessageQueue *replyQueue
    ) :
        Message(),
        m_audio(audio),
        m_mbeRateIndex(mbeRateIndex),
        m_gain(gain),
        m_sequence(sequence),
        m_client(client),
        m_replyQueue(replyQueue)
    { }

    const short *getAudio() const { return m_audio; }      //!< 160 samples
    int getMbeRateIndex() const { return m_mbeRateIndex; } //!< SerialDV::DVRate compatible
    int getGain() const { return m_gain; }                 //!< encoder input gain (dB)
    unsigned int getSequence() const { return m_sequence; }
    const void *getClient() const { return m_client; }
    MessageQueue *getReplyQueue() const { return m_replyQueue; }

private:
    const short *m_audio;
    int m_mbeRateIndex;
    int m_gain;
    unsigned int m_sequence;
    const void *m_client;
    MessageQueue *m_replyQueue;
};

/**
 * Reply to DSPPushMbeAudio carrying the encoded MBE frame (DVSI order)
 */
class SDRBASE_API DSPReportMbeFrame : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    static const int m_maxFrameBytes = 18;

    DSPReportMbeFrame(
        const unsigned char *mbeFrame,
        int nbBytes,
        unsigned int sequence,
        const void *client,
        bool ok
    ) :
        Message(),
        m_nbBytes(nbBytes),
        m_sequence(sequence),
        m_client(client),
        m_ok(ok)
    {
        std::fill(m_mbeFrame, m_mbeFrame + m_maxFrameBytes, 0);

        if (mbeFrame && (nbBytes > 0)) {
            std::copy(mbeFrame, mbeFrame + std::min(nbBytes, m_maxFrameBytes), m_mbeFrame);
        }
    }

    const unsigned char *getMbeFrame() const { return m_mbeFrame; }
    int getNbBytes() const { return m_nbBytes; }
    unsigned int getSequence() const { return m_sequence; }
    const void *getClient() const { return m_client; }
    bool isOK() const { return m_ok; }

private:
    unsigned char m_mbeFrame[m_maxFrameBytes];
    int m_nbBytes;
    unsigned int m_sequence;
    const void *m_client;
    bool m_ok;
};

/**
 * Tell an AMBE feature that a client is done with it: an encoder's client (its reply queue must
 * not be used any more), or a decoder's AudioFifo (it must not be written any more; the call
 * returns once that is so, and queued frames for it are dropped). Sent through MbeEncoderPort.
 */
class SDRBASE_API DSPReleaseMbeClient : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    DSPReleaseMbeClient(const void *client) :
        Message(),
        m_client(client)
    { }

    const void *getClient() const { return m_client; }

private:
    const void *m_client;
};

/**
 * Thread safe handle on an MBE provider (the AMBE feature) for clients that call it from their
 * own threads: encoders (DSPPushMbeAudio) and decoders (DSPPushMbeFrame, the DSD Demodulator's
 * sink) alike. The provider creates it, hands it out with DSPGetMbeEncoderPort and closes it in
 * its destructor: a call in flight completes before the provider goes away and later calls return
 * false. Clients keep it by shared pointer so it outlives both sides.
 */
class SDRBASE_API MbeEncoderPort
{
public:
    MbeEncoderPort() : m_provider(nullptr) { }

    //! Provider side: set at construction, cleared (blocking on calls in flight) at destruction
    void setProvider(std::function<bool(const Message&)> provider)
    {
        QMutexLocker locker(&m_mutex);
        m_provider = provider;
    }

    //! Client side: forward a message to the provider. Returns false if the provider is gone.
    bool handleMessage(const Message& msg)
    {
        QMutexLocker locker(&m_mutex);
        return m_provider ? m_provider(msg) : false;
    }

    bool isOpen()
    {
        QMutexLocker locker(&m_mutex);
        return (bool) m_provider;
    }

private:
    QMutex m_mutex;
    std::function<bool(const Message&)> m_provider;
};

/**
 * Ask an MBE encoder provider for its port (handled synchronously: the provider fills the port).
 */
class SDRBASE_API DSPGetMbeEncoderPort : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    DSPGetMbeEncoderPort() : Message() { }

    std::shared_ptr<MbeEncoderPort> getPort() const { return m_port; }
    void setPort(std::shared_ptr<MbeEncoderPort> port) { m_port = port; }

private:
    std::shared_ptr<MbeEncoderPort> m_port;
};

#endif // INCLUDE_DSPCOMMANDS_H
