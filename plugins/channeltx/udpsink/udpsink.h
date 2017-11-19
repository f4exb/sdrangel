///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSINK_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSINK_H_

#include <QObject>

#include "dsp/basebandsamplesource.h"
#include "channel/channelsourceapi.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "util/message.h"

#include "udpsinkudphandler.h"
#include "udpsinksettings.h"

class DeviceSinkAPI;
class ThreadedBasebandSampleSource;
class UpChannelizer;

class UDPSink : public BasebandSampleSource, public ChannelSourceAPI {
    Q_OBJECT

public:
    class MsgConfigureUDPSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSink* create(const UDPSinkSettings& settings, bool force)
        {
            return new MsgConfigureUDPSink(settings, force);
        }

    private:
        UDPSinkSettings m_settings;
        bool m_force;

        MsgConfigureUDPSink(const UDPSinkSettings& settings, bool force) :
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

    UDPSink(DeviceSinkAPI *deviceAPI);
    virtual ~UDPSink();

    void setSpectrumSink(BasebandSampleSink* spectrum) { m_spectrum = spectrum; }

    virtual void start();
    virtual void stop();
    virtual void pull(Sample& sample);
    virtual bool handleMessage(const Message& cmd);

    virtual int getDeltaFrequency() const { return m_absoluteFrequencyOffset; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }

    double getMagSq() const { return m_magsq; }
    double getInMagSq() const { return m_inMagsq; }
    int32_t getBufferGauge() const { return m_udpHandler.getBufferGauge(); }
    bool getSquelchOpen() const { return m_squelchOpen; }

    void setSpectrum(bool enabled);
    void resetReadIndex();

    static const QString m_channelID;

signals:
    /**
     * Level changed
     * \param rmsLevel RMS level in range 0.0 - 1.0
     * \param peakLevel Peak level in range 0.0 - 1.0
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
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

    class MsgResetReadIndex : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgResetReadIndex* create()
        {
            return new MsgResetReadIndex();
        }

    private:

        MsgResetReadIndex() :
            Message()
        { }
    };

    DeviceSinkAPI* m_deviceAPI;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;

    UDPSinkSettings m_settings;
    int m_absoluteFrequencyOffset;

    Real m_squelch;

    NCO m_carrierNco;
    Complex m_modSample;

    BasebandSampleSink* m_spectrum;
    bool m_spectrumEnabled;
    SampleVector m_sampleBuffer;
    int m_spectrumChunkSize;
    int m_spectrumChunkCounter;

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    double m_magsq;
    double m_inMagsq;
    MovingAverage<double> m_movingAverage;
    MovingAverage<double> m_inMovingAverage;

    UDPSinkUDPHandler m_udpHandler;
    Real m_actualInputSampleRate; //!< sample rate with UDP buffer skew compensation
    double m_sampleRateSum;
    int m_sampleRateAvgCounter;

    int m_levelCalcCount;
    Real m_peakLevel;
    double m_levelSum;
    int m_levelNbSamples;

    bool m_squelchOpen;
    int  m_squelchOpenCount;
    int  m_squelchCloseCount;
    int m_squelchThreshold;

    float m_modPhasor;    //!< Phasor for FM modulation
    fftfilt* m_SSBFilter; //!< Complex filter for SSB modulation
    Complex* m_SSBFilterBuffer;
    int m_SSBFilterBufferIndex;

    QMutex m_settingsMutex;

    static const int m_sampleRateAverageItems = 17;
    static const int m_ssbFftLen = 1024;

    void applySettings(const UDPSinkSettings& settings, bool force = false);
    void modulateSample();
    void calculateLevel(Real sample);
    void calculateLevel(Complex sample);

    inline void calculateSquelch(double value)
    {
        if ((!m_settings.m_squelchEnabled) || (value > m_squelch))
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

    inline void readMonoSample(FixReal& t)
    {
        Sample s;

        if (m_settings.m_stereoInput)
        {
            m_udpHandler.readSample(s);
            t = ((s.m_real + s.m_imag) * m_settings.m_gainIn) / 2;
        }
        else
        {
            m_udpHandler.readSample(t);
            t *= m_settings.m_gainIn;
        }
    }
};




#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINK_H_ */
