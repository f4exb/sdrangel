///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
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

#ifndef INCLUDE_UDPSRC_H
#define INCLUDE_UDPSRC_H

#include <dsp/basebandsamplesink.h>
#include <QMutex>
#include <QHostAddress>
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "dsp/phasediscri.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/bandpass.h"
#include "util/udpsink.h"
#include "util/message.h"
#include "audio/audiofifo.h"


class QUdpSocket;
class UDPSrcGUI;

class UDPSrc : public BasebandSampleSink {
	Q_OBJECT

public:
	enum SampleFormat {
		FormatS16LE,
		FormatNFM,
		FormatNFMMono,
		FormatLSB,
		FormatUSB,
		FormatLSBMono,
		FormatUSBMono,
		FormatAMMono,
		FormatAMNoDCMono,
		FormatAMBPFMono,
		FormatNone
	};

	struct AudioSample {
		qint16 l;
		qint16 r;
	};

	typedef std::vector<AudioSample> AudioVector;

	UDPSrc(MessageQueue* uiMessageQueue, UDPSrcGUI* udpSrcGUI, BasebandSampleSink* spectrum);
	virtual ~UDPSrc();

	void configure(MessageQueue* messageQueue,
			SampleFormat sampleFormat,
			Real outputSampleRate,
			Real rfBandwidth,
			int fmDeviation,
			QString& udpAddress,
			int udpPort,
			int audioPort,
			bool force);
	void configureImmediate(MessageQueue* messageQueue,
			bool audioActive,
			bool audioStereo,
			Real gain,
			int volume,
			Real squelchDB,
			Real squelchGate,
			bool squelchEnabled,
			bool agc,
			bool force);
	void setSpectrum(MessageQueue* messageQueue, bool enabled);
	double getMagSq() const { return m_magsq; }
	double getInMagSq() const { return m_inMagsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	static const int udpBlockSize = 512; // UDP block size in number of bytes

public slots:
    void audioReadyRead();

protected:
	class MsgUDPSrcConfigure : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		SampleFormat getSampleFormat() const { return m_sampleFormat; }
		Real getOutputSampleRate() const { return m_outputSampleRate; }
		Real getRFBandwidth() const { return m_rfBandwidth; }
		int getFMDeviation() const { return m_fmDeviation; }
		const QString& getUDPAddress() const { return m_udpAddress; }
		int getUDPPort() const { return m_udpPort; }
		int getAudioPort() const { return m_audioPort; }
		bool getForce() const { return m_force; }

		static MsgUDPSrcConfigure* create(SampleFormat
				sampleFormat,
				Real sampleRate,
				Real rfBandwidth,
				int fmDeviation,
				QString& udpAddress,
				int udpPort,
				int audioPort,
				bool force)
		{
			return new MsgUDPSrcConfigure(sampleFormat,
					sampleRate,
					rfBandwidth,
					fmDeviation,
					udpAddress,
					udpPort,
					audioPort,
					force);
		}

	private:
		SampleFormat m_sampleFormat;
		Real m_outputSampleRate;
		Real m_rfBandwidth;
		int m_fmDeviation;
		QString m_udpAddress;
		int m_udpPort;
		int m_audioPort;
		bool m_force;

		MsgUDPSrcConfigure(SampleFormat sampleFormat,
				Real outputSampleRate,
				Real rfBandwidth,
				int fmDeviation,
				QString& udpAddress,
				int udpPort,
				int audioPort,
				bool force) :
			Message(),
			m_sampleFormat(sampleFormat),
			m_outputSampleRate(outputSampleRate),
			m_rfBandwidth(rfBandwidth),
			m_fmDeviation(fmDeviation),
			m_udpAddress(udpAddress),
			m_udpPort(udpPort),
			m_audioPort(audioPort),
			m_force(force)
		{ }
	};

	class MsgUDPSrcConfigureImmediate : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getGain() const { return m_gain; }
		int getVolume() const { return m_volume; }
		bool getAudioActive() const { return m_audioActive; }
		bool getAudioStereo() const { return m_audioStereo; }
		Real getSquelchDB() const { return m_squelchDB; }
		Real getSquelchGate() const { return m_squelchGate; }
		bool getSquelchEnabled() const { return m_squelchEnabled; }
		bool getAGC() const { return m_agc; }
		bool getForce() const { return m_force; }

		static MsgUDPSrcConfigureImmediate* create(
				bool audioActive,
				bool audioStereo,
				int gain,
				int volume,
				Real squelchDB,
				Real squelchGate,
				bool squelchEnabled,
				bool agc,
				bool force)
		{
			return new MsgUDPSrcConfigureImmediate(
					audioActive,
					audioStereo,
					gain,
					volume,
					squelchDB,
					squelchGate,
					squelchEnabled,
					agc,
					force);
		}

	private:
		Real m_gain;
		int  m_volume;
		bool m_audioActive;
		bool m_audioStereo;
		Real m_squelchDB;
        Real m_squelchGate; // seconds
		bool m_squelchEnabled;
		bool m_agc;
		bool m_force;

		MsgUDPSrcConfigureImmediate(
				bool audioActive,
				bool audioStereo,
				Real gain,
				int volume,
                Real squelchDB,
                Real squelchGate,
                bool squelchEnabled,
                bool agc,
                bool force) :
			Message(),
			m_gain(gain),
            m_volume(volume),
			m_audioActive(audioActive),
			m_audioStereo(audioStereo),
			m_squelchDB(squelchDB),
            m_squelchGate(squelchGate),
			m_squelchEnabled(squelchEnabled),
			m_agc(agc),
			m_force(force)
		{ }
	};

	class MsgUDPSrcSpectrum : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getEnabled() const { return m_enabled; }

		static MsgUDPSrcSpectrum* create(bool enabled)
		{
			return new MsgUDPSrcSpectrum(enabled);
		}

	private:
		bool m_enabled;

		MsgUDPSrcSpectrum(bool enabled) :
			Message(),
			m_enabled(enabled)
		{ }
	};

    struct Config {
        Real m_outputSampleRate;
        SampleFormat m_sampleFormat;
        Real m_inputSampleRate;
        qint64 m_inputFrequencyOffset;
        Real m_rfBandwidth;
        int m_fmDeviation;
        bool m_channelMute;
        Real m_gain;
        Real m_squelch; //!< squared magnitude
        Real m_squelchGate; //!< seconds
        bool m_squelchEnabled;
        bool m_agc;
        bool m_audioActive;
        bool m_audioStereo;
        int m_volume;

        QString m_udpAddressStr;
        quint16 m_udpPort;
        quint16 m_audioPort;

        Config() :
            m_outputSampleRate(48000),
            m_sampleFormat(FormatS16LE),
            m_inputSampleRate(48000),
            m_inputFrequencyOffset(0),
            m_rfBandwidth(12500),
            m_fmDeviation(2500),
            m_channelMute(false),
            m_gain(1.0),
            m_squelch(1e-6),
            m_squelchGate(0.0),
            m_squelchEnabled(true),
            m_agc(false),
            m_audioActive(false),
            m_audioStereo(false),
            m_volume(20),
            m_udpAddressStr("127.0.0.1"),
            m_udpPort(9999),
            m_audioPort(9998)
        {}
    };

    Config m_config;
    Config m_running;

	MessageQueue* m_uiMessageQueue;
	UDPSrcGUI* m_udpSrcGUI;
	QUdpSocket *m_audioSocket;

	double m_magsq;
    double m_inMagsq;
    MovingAverage<double> m_outMovingAverage;
    MovingAverage<double> m_inMovingAverage;
    MovingAverage<double> m_amMovingAverage;

	Real m_scale;
	Complex m_last, m_this;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* UDPFilter;

	SampleVector m_sampleBuffer;
	UDPSink<Sample> *m_udpBuffer;
	UDPSink<FixReal> *m_udpBufferMono;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;

	BasebandSampleSink* m_spectrum;
	bool m_spectrumEnabled;

	quint32 m_nextSSBId;
	quint32 m_nextS16leId;

	char *m_udpAudioBuf;
	static const int m_udpAudioPayloadSize = 8192; //!< UDP audio samples buffer. No UDP block on Earth is larger than this
	static const Real m_agcTarget = 16384.0f;

    PhaseDiscriminators m_phaseDiscri;

    bool m_squelchOpen;
    int  m_squelchOpenCount;
    int  m_squelchCloseCount;
    int m_squelchThreshold; //!< number of samples computed from given gate

    MagAGC m_agc;
    Bandpass<double> m_bandpass;

	QMutex m_settingsMutex;

	void apply(bool force);

    inline void calculateSquelch(double value)
    {
        if ((!m_running.m_squelchEnabled) || (value > m_running.m_squelch))
        {
            if (m_squelchThreshold == 0)
            {
                m_squelchOpen = true;
            }
            else
            {
                if (m_squelchOpenCount < m_squelchThreshold)
                {
                    m_squelchOpenCount++;
                }
                else
                {
                    m_squelchCloseCount = m_squelchThreshold;
                    m_squelchOpen = true;
                }
            }
        }
        else
        {
            if (m_squelchThreshold == 0)
            {
                m_squelchOpen = false;
            }
            else
            {
                if (m_squelchCloseCount > 0)
                {
                    m_squelchCloseCount--;
                }
                else
                {
                    m_squelchOpenCount = 0;
                    m_squelchOpen = false;
                }
            }
        }
    }

    inline void initSquelch(bool open)
    {
        if (open)
        {
            m_squelchOpen = true;
            m_squelchOpenCount = m_squelchThreshold;
            m_squelchCloseCount = m_squelchThreshold;
        }
        else
        {
            m_squelchOpen = false;
            m_squelchOpenCount = 0;
            m_squelchCloseCount = 0;
        }
    }

};

#endif // INCLUDE_UDPSRC_H
