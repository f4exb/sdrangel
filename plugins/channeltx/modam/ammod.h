///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODAM_AMMOD_H_
#define PLUGINS_CHANNELTX_MODAM_AMMOD_H_

#include <QMutex>
#include <vector>
#include "dsp/basebandsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/message.h"

class AMMod : public BasebandSampleSource {
    Q_OBJECT

public:
    AMMod();
    ~AMMod();

    void configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, int modPercent, bool audioMute);

    virtual void pull(Sample& sample);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    Real getMagSq() const { return m_magsq; }

private:
    class MsgConfigureAMMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getRFBandwidth() const { return m_rfBandwidth; }
        Real getAFBandwidth() const { return m_afBandwidth; }
        float getModFactor() const { return m_modFactor; }
        bool getAudioMute() const { return m_audioMute; }

        static MsgConfigureAMMod* create(Real rfBandwidth, Real afBandwidth, int modPercent, bool audioMute)
        {
            return new MsgConfigureAMMod(rfBandwidth, afBandwidth, modPercent, audioMute);
        }

    private:
        Real m_rfBandwidth;
        Real m_afBandwidth;
        float m_modFactor;
        bool m_audioMute;

        MsgConfigureAMMod(Real rfBandwidth, Real afBandwidth, float modFactor, bool audioMute) :
            Message(),
            m_rfBandwidth(rfBandwidth),
            m_afBandwidth(afBandwidth),
            m_modFactor(modFactor),
            m_audioMute(audioMute)
        { }
    };

    struct AudioSample {
        qint16 l;
        qint16 r;
    };
    typedef std::vector<AudioSample> AudioVector;

    enum RateState {
        RSInitialFill,
        RSRunning
    };

    struct Config {
        int m_outputSampleRate;
        qint64 m_inputFrequencyOffset;
        Real m_rfBandwidth;
        Real m_afBandwidth;
        float m_modFactor;
        quint32 m_audioSampleRate;
        bool m_audioMute;

        Config() :
            m_outputSampleRate(-1),
            m_inputFrequencyOffset(0),
            m_rfBandwidth(-1),
            m_afBandwidth(-1),
            m_modFactor(0.2f),
            m_audioSampleRate(0),
            m_audioMute(false)
        { }
    };

    Config m_config;
    Config m_running;

    NCO m_carrierNco;
    NCO m_toneNco;
    Complex m_modSample;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    Lowpass<Real> m_lowpass;

    Real m_magsq;
    MovingAverage<Real> m_movingAverage;
    SimpleAGC m_volumeAGC;

    AudioVector m_audioBuffer;
    uint m_audioBufferFill;

    AudioFifo m_audioFifo;
    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;

    void apply();
};


#endif /* PLUGINS_CHANNELTX_MODAM_AMMOD_H_ */
