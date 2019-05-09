///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSOURCE_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSOURCE_H_

#include <QObject>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "util/message.h"

#include "udpsourcesettings.h"
#include "udpsourceudphandler.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class ThreadedBasebandSampleSource;
class UpChannelizer;

class UDPSource : public BasebandSampleSource, public ChannelAPI {
    Q_OBJECT

public:
    class MsgConfigureUDPSource : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSource* create(const UDPSourceSettings& settings, bool force)
        {
            return new MsgConfigureUDPSource(settings, force);
        }

    private:
        UDPSourceSettings m_settings;
        bool m_force;

        MsgConfigureUDPSource(const UDPSourceSettings& settings, bool force) :
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

    UDPSource(DeviceAPI *deviceAPI);
    virtual ~UDPSource();
    virtual void destroy() { delete this; }

    void setSpectrumSink(BasebandSampleSink* spectrum) { m_spectrum = spectrum; }

    virtual void start();
    virtual void stop();
    virtual void pull(Sample& sample);
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

    double getMagSq() const { return m_magsq; }
    double getInMagSq() const { return m_inMagsq; }
    int32_t getBufferGauge() const { return m_udpHandler.getBufferGauge(); }
    bool getSquelchOpen() const { return m_squelchOpen; }

    void setSpectrum(bool enabled);
    void resetReadIndex();

    static const QString m_channelIdURI;
    static const QString m_channelId;

signals:
    /**
     * Level changed
     * \param rmsLevel RMS level in range 0.0 - 1.0
     * \param peakLevel Peak level in range 0.0 - 1.0
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private slots:
    void networkManagerFinished(QNetworkReply *reply);

private:
    class MsgUDPSourceSpectrum : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getEnabled() const { return m_enabled; }

        static MsgUDPSourceSpectrum* create(bool enabled)
        {
            return new MsgUDPSourceSpectrum(enabled);
        }

    private:
        bool m_enabled;

        MsgUDPSourceSpectrum(bool enabled) :
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

    DeviceAPI* m_deviceAPI;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;

    int m_basebandSampleRate;
    Real m_outputSampleRate;
    int m_inputFrequencyOffset;
    UDPSourceSettings m_settings;

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

    UDPSourceUDPHandler m_udpHandler;
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

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    QMutex m_settingsMutex;

    static const int m_sampleRateAverageItems = 17;
    static const int m_ssbFftLen = 1024;

    void applyChannelSettings(int basebandSampleRate, int outputSampleRate, int inputFrequencyOffset, bool force = false);
    void applySettings(const UDPSourceSettings& settings, bool force = false);
    void modulateSample();
    void calculateLevel(Real sample);
    void calculateLevel(Complex sample);

    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const UDPSourceSettings& settings);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSourceSettings& settings, bool force);

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

    inline void readMonoSample(qint16& t)
    {

        if (m_settings.m_stereoInput)
        {
            AudioSample a;
            m_udpHandler.readSample(a);
            t = ((a.l + a.r) * m_settings.m_gainIn) / 2;
        }
        else
        {
            m_udpHandler.readSample(t);
            t *= m_settings.m_gainIn;
        }
    }
};




#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSOURCE_H_ */
