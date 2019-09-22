///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_UDPSRC_H
#define INCLUDE_UDPSRC_H

#include <QMutex>
#include <QHostAddress>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "dsp/phasediscri.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/bandpass.h"
#include "util/udpsinkutil.h"
#include "util/message.h"
#include "audio/audiofifo.h"

#include "udpsinksettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QUdpSocket;
class DeviceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class UDPSink : public BasebandSampleSink, public ChannelAPI {
	Q_OBJECT

public:
    class MsgConfigureUDPSource : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSource* create(const UDPSinkSettings& settings, bool force)
        {
            return new MsgConfigureUDPSource(settings, force);
        }

    private:
        UDPSinkSettings m_settings;
        bool m_force;

        MsgConfigureUDPSource(const UDPSinkSettings& settings, bool force) :
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

	UDPSink(DeviceAPI *deviceAPI);
	virtual ~UDPSink();
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

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const UDPSinkSettings& settings);

    static void webapiUpdateChannelSettings(
            UDPSinkSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;

    static const QString m_channelIdURI;
    static const QString m_channelId;
	static const int udpBlockSize = 512; // UDP block size in number of bytes

public slots:
    void audioReadyRead();

private slots:
    void networkManagerFinished(QNetworkReply *reply);

protected:
	class MsgUDPSinkSpectrum : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getEnabled() const { return m_enabled; }

		static MsgUDPSinkSpectrum* create(bool enabled)
		{
			return new MsgUDPSinkSpectrum(enabled);
		}

	private:
		bool m_enabled;

		MsgUDPSinkSpectrum(bool enabled) :
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

    DeviceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    int m_inputSampleRate;
    int m_inputFrequencyOffset;
    UDPSinkSettings m_settings;

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
	UDPSinkUtil<Sample16> *m_udpBuffer16;
	UDPSinkUtil<int16_t> *m_udpBufferMono16;
    UDPSinkUtil<Sample24> *m_udpBuffer24;

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

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	QMutex m_settingsMutex;

    void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = true);
    void applySettings(const UDPSinkSettings& settings, bool force = false);

    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSinkSettings& settings, bool force);

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
            if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ16) {
                m_udpBuffer16->write(Sample16(real, imag));
            } else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ24) {
                m_udpBuffer24->write(Sample24(real<<8, imag<<8));
            } else {
                m_udpBuffer16->write(Sample16(real, imag));
            }
        }
        else if (SDR_RX_SAMP_SZ == 24)
        {
            if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ16) {
                m_udpBuffer16->write(Sample16(real>>8, imag>>8));
            } else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ24) {
                m_udpBuffer24->write(Sample24(real, imag));
            } else {
                m_udpBuffer16->write(Sample16(real>>8, imag>>8));
            }
        }
    }

    void udpWriteMono(FixReal sample)
    {
        if (SDR_RX_SAMP_SZ == 16)
        {
            m_udpBufferMono16->write(sample);
        }
        else if (SDR_RX_SAMP_SZ == 24)
        {
            m_udpBufferMono16->write(sample>>8);
        }
    }

    void udpWriteNorm(Real real, Real imag)
    {
        m_udpBuffer16->write(Sample16(real*32768.0, imag*32768.0));
    }

    void udpWriteNormMono(Real sample)
    {
        m_udpBufferMono16->write(sample*32768.0);
    }

};

#endif // INCLUDE_UDPSRC_H
