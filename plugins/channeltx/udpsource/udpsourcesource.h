///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSOURCESOURCE_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSOURCESOURCE_H_

#include <QObject>
#include <QNetworkRequest>

#include "dsp/channelsamplesource.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"

#include "udpsourcesettings.h"
#include "udpsourceudphandler.h"

class BasebandSampleSink;
class MessageQueue;

class UDPSourceSource : public ChannelSampleSource {
public:
    UDPSourceSource();
    virtual ~UDPSourceSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; };

    void setUDPFeedbackMessageQueue(MessageQueue *messageQueue);
    void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_spectrumSink = spectrumSink; }
    double getMagSq() const { return m_magsq; }
    double getInMagSq() const { return m_inMagsq; }
    int32_t getBufferGauge() const { return m_udpHandler.getBufferGauge(); }
    bool getSquelchOpen() const { return m_squelchOpen; }

    void resetReadIndex();

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const UDPSourceSettings& settings, bool force = false);

    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }

    void sampleRateCorrection(float rawDeltaRatio, float correctionFactor);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    UDPSourceSettings m_settings;

    Real m_squelch;

    NCO m_carrierNco;
    Complex m_modSample;

    BasebandSampleSink* m_spectrumSink;
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
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
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

    static const int m_sampleRateAverageItems = 17;
    static const int m_ssbFftLen = 1024;

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

#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSOURCESOURCE_H_ */
