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

#include <QMutex>
#include <QHostAddress>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
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

#include "udpsrcsettings.h"

class QUdpSocket;
class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class UDPSrc : public BasebandSampleSink, public ChannelSinkAPI {
	Q_OBJECT

public:
    class MsgConfigureUDPSrc : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPSrcSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSrc* create(const UDPSrcSettings& settings, bool force)
        {
            return new MsgConfigureUDPSrc(settings, force);
        }

    private:
        UDPSrcSettings m_settings;
        bool m_force;

        MsgConfigureUDPSrc(const UDPSrcSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        {
        }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgConfigureChannelizer* create(int sampleRate, int centerFrequency)
        {
            return new MsgConfigureChannelizer(sampleRate, centerFrequency);
        }

    private:
        int m_sampleRate;
        int  m_centerFrequency;

        MsgConfigureChannelizer(int sampleRate, int centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

	UDPSrc(DeviceSourceAPI *deviceAPI);
	virtual ~UDPSrc();
	virtual void destroy() { delete this; }
	void setSpectrum(BasebandSampleSink* spectrum) { m_spectrum = spectrum; }

	void setSpectrum(MessageQueue* messageQueue, bool enabled);
	double getMagSq() const { return m_magsq; }
	double getInMagSq() const { return m_inMagsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    static const QString m_channelIdURI;
    static const QString m_channelId;
	static const int udpBlockSize = 512; // UDP block size in number of bytes

public slots:
    void audioReadyRead();

protected:
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

	struct Sample16
	{
	    Sample16() : m_r(0), m_i(0) {}
	    Sample16(int16_t r, int16_t i) : m_r(r), m_i(i) {}
	    int16_t m_r;
	    int16_t m_i;
	};

    struct Sample24
    {
	    Sample24() : m_r(0), m_i(0) {}
	    Sample24(int32_t r, int32_t i) : m_r(r), m_i(i) {}
        int32_t m_r;
        int32_t m_i;
    };

    DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    int m_inputSampleRate;
    int m_inputFrequencyOffset;
    UDPSrcSettings m_settings;

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
	UDPSink<Sample16> *m_udpBuffer16;
	UDPSink<int16_t> *m_udpBufferMono16;
    UDPSink<Sample24> *m_udpBuffer24;
    UDPSink<int32_t> *m_udpBufferMono24;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;

	BasebandSampleSink* m_spectrum;
	bool m_spectrumEnabled;

	quint32 m_nextSSBId;
	quint32 m_nextS16leId;

	char *m_udpAudioBuf;
	static const int m_udpAudioPayloadSize = 8192; //!< UDP audio samples buffer. No UDP block on Earth is larger than this
    static const Real m_agcTarget;

    PhaseDiscriminators m_phaseDiscri;

    double m_squelch;
    bool m_squelchOpen;
    int  m_squelchOpenCount;
    int  m_squelchCloseCount;
    int  m_squelchGate; //!< number of samples computed from given gate
    int  m_squelchRelease;

    MagAGC m_agc;
    Bandpass<double> m_bandpass;

	QMutex m_settingsMutex;

    void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = true);
    void applySettings(const UDPSrcSettings& settings, bool force = false);

    inline void calculateSquelch(double value)
    {
        if ((!m_settings.m_squelchEnabled) || (value > m_squelch))
        {
            if (m_squelchGate == 0)
            {
                m_squelchOpen = true;
            }
            else
            {
                if (m_squelchOpenCount < m_squelchGate)
                {
                    m_squelchOpenCount++;
                }
                else
                {
                    m_squelchCloseCount = m_squelchRelease;
                    m_squelchOpen = true;
                }
            }
        }
        else
        {
            if (m_squelchGate == 0)
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
            m_squelchOpenCount = m_squelchGate;
            m_squelchCloseCount = m_squelchRelease;
        }
        else
        {
            m_squelchOpen = false;
            m_squelchOpenCount = 0;
            m_squelchCloseCount = 0;
        }
    }

    void udpWrite(FixReal real, FixReal imag)
    {
        if (SDR_RX_SAMP_SZ == 16)
        {
            if (m_settings.m_sampleSize == UDPSrcSettings::Size16bits) {
                m_udpBuffer16->write(Sample16(real, imag));
            } else if (m_settings.m_sampleSize == UDPSrcSettings::Size24bits) {
                m_udpBuffer24->write(Sample24(real<<8, imag<<8));
            }
        }
        else if (SDR_RX_SAMP_SZ == 24)
        {
            if (m_settings.m_sampleSize == UDPSrcSettings::Size16bits) {
                m_udpBuffer16->write(Sample16(real>>8, imag>>8));
            } else if (m_settings.m_sampleSize == UDPSrcSettings::Size24bits) {
                m_udpBuffer24->write(Sample24(real, imag));
            }
        }
    }

    void udpWriteMono(FixReal sample)
    {
        if (SDR_RX_SAMP_SZ == 16)
        {
            if (m_settings.m_sampleSize == UDPSrcSettings::Size16bits) {
                m_udpBufferMono16->write(sample);
            } else if (m_settings.m_sampleSize == UDPSrcSettings::Size24bits) {
                m_udpBufferMono24->write(sample<<8);
            }
        }
        else if (SDR_RX_SAMP_SZ == 24)
        {
            if (m_settings.m_sampleSize == UDPSrcSettings::Size16bits) {
                m_udpBufferMono16->write(sample>>8);
            } else if (m_settings.m_sampleSize == UDPSrcSettings::Size24bits) {
                m_udpBufferMono24->write(sample);
            }
        }
    }

    void udpWriteNorm(Real real, Real imag)
    {
        if (m_settings.m_sampleSize == UDPSrcSettings::Size16bits) {
            m_udpBuffer16->write(Sample16(real*32768.0, imag*32768.0));
        } else if (m_settings.m_sampleSize == UDPSrcSettings::Size24bits) {
            m_udpBuffer24->write(Sample24(real*8388608.0, imag*8388608.0));
        }
    }

    void udpWriteNormMono(Real sample)
    {
        if (m_settings.m_sampleSize == UDPSrcSettings::Size16bits) {
            m_udpBufferMono16->write(sample*32768.0);
        } else if (m_settings.m_sampleSize == UDPSrcSettings::Size24bits) {
            m_udpBufferMono24->write(sample*8388608.0);
        }
    }

};

#endif // INCLUDE_UDPSRC_H
