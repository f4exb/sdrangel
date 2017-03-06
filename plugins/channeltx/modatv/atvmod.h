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

#ifndef PLUGINS_CHANNELTX_MODATV_ATVMOD_H_
#define PLUGINS_CHANNELTX_MODATV_ATVMOD_H_

#include <QObject>
#include <QMutex>

#include <stdint.h>

#include "dsp/basebandsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "util/message.h"

class ATVMod : public BasebandSampleSource {
    Q_OBJECT

public:
    typedef enum
    {
        ATVStdPAL625
    } ATVStd;

    typedef enum
    {
        ATVModInputUniform,
        ATVModInputBarChart,
        ATVModInputGradient
    } ATVModInput;

    ATVMod();
    ~ATVMod();

    void configure(MessageQueue* messageQueue,
            Real rfBandwidth,
            ATVStd atvStd,
            ATVModInput atvModInput,
            Real uniformLevel,
            bool channelMute);

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples); // this is used for video signal actually
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    Real getMagSq() const { return m_movingAverage.average(); }

    static int getSampleRateMultiple(ATVStd std);

signals:
    /**
     * Level changed
     * \param rmsLevel RMS level in range 0.0 - 1.0
     * \param peakLevel Peak level in range 0.0 - 1.0
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    class MsgConfigureATVMod : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getRFBandwidth() const { return m_rfBandwidth; }
        ATVStd getATVStd() const { return m_atvStd; }
        ATVModInput getATVModInput() const { return m_atvModInput; }
        Real getUniformLevel() const { return m_uniformLevel; }

        static MsgConfigureATVMod* create(
            Real rfBandwidth,
            ATVStd atvStd,
            ATVModInput atvModInput,
            Real uniformLevel)
        {
            return new MsgConfigureATVMod(rfBandwidth, atvStd, atvModInput, uniformLevel);
        }

    private:
        Real        m_rfBandwidth;
        ATVStd      m_atvStd;
        ATVModInput m_atvModInput;
        Real    m_uniformLevel;

        MsgConfigureATVMod(
                Real rfBandwidth,
                ATVStd atvStd,
                ATVModInput atvModInput,
                Real uniformLevel) :
            Message(),
            m_rfBandwidth(rfBandwidth),
            m_atvStd(atvStd),
            m_atvModInput(atvModInput),
            m_uniformLevel(uniformLevel)
        { }
    };

    struct Config
    {
        int         m_outputSampleRate;     //!< sample rate from channelizer
        qint64      m_inputFrequencyOffset; //!< offset from baseband center frequency
        Real        m_rfBandwidth;          //!< Bandwidth of modulated signal
        ATVStd      m_atvStd;               //!< Standard
        ATVModInput m_atvModInput;          //!< Input source type
        Real        m_uniformLevel;         //!< Percentage between black and white for uniform screen display

        Config() :
            m_outputSampleRate(-1),
            m_inputFrequencyOffset(0),
            m_rfBandwidth(0),
            m_atvStd(ATVStdPAL625),
            m_atvModInput(ATVModInputBarChart),
            m_uniformLevel(0.5f)
        { }
    };

    Config m_config;
    Config m_running;

    NCO m_carrierNco;
    Complex m_modSample;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    int      m_tvSampleRate;     //!< sample rate for generating signal
    uint32_t m_pointsPerSync;    //!< number of line points for the horizontal sync
    uint32_t m_pointsPerBP;      //!< number of line points for the back porch
    uint32_t m_pointsPerLine;    //!< number of line points for the image line
    uint32_t m_pointsPerFP;      //!< number of line points for the front porch
    uint32_t m_pointsPerFSync;   //!< number of line points for the field first sync
    uint32_t m_pointsPerBar;     //!< number of line points for a bar of the bar chart
    uint32_t m_pointsPerTU;      //!< number of line points per time unit
    uint32_t m_nbLines;          //!< number of lines per complete frame
    uint32_t m_nbHorizPoints;    //!< number of line points per horizontal line
    bool     m_interlaced;       //!< true if image is interlaced (2 half frames per frame)
    bool     m_evenImage;
    QMutex   m_settingsMutex;
    int      m_horizontalCount;  //!< current point index on line
    int      m_lineCount;        //!< current line index in frame

    MovingAverage<Real> m_movingAverage;
    quint32 m_levelCalcCount;
    Real m_peakLevel;
    Real m_levelSum;

    static const float m_blackLevel;
    static const float m_spanLevel;
    static const int m_levelNbSamples;

    void apply(bool force = false);
    void pullFinalize(Complex& ci, Sample& sample);
    void pullVideo(Real& sample);
    void calculateLevel(Real& sample);
    void modulateSample();
    void applyStandard();

    inline void pullImageLine(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerSync) // sync pulse
        {
            sample = 0.0f; // ultra-black
        }
        else if (m_horizontalCount < m_pointsPerSync + m_pointsPerBP) // back porch
        {
            sample = 0.3f; // black
        }
        else if (m_horizontalCount < m_pointsPerSync + m_pointsPerBP + m_pointsPerLine)
        {
            int pointIndex = m_horizontalCount - (m_pointsPerSync + m_pointsPerBP);

            switch(m_running.m_atvModInput)
            {
            case ATVModInputBarChart:
                sample = (pointIndex / m_pointsPerBar) * (m_spanLevel/5.0f) + m_blackLevel;
                break;
            case ATVModInputGradient:
                sample = (pointIndex / m_pointsPerLine) * m_spanLevel + m_blackLevel;
                break;
            case ATVModInputUniform:
            default:
                sample = m_spanLevel * m_running.m_uniformLevel + m_blackLevel;
            }
        }
        else // front porch
        {
            sample = m_blackLevel; // black
        }
    }

    inline void pullVSyncLine(int pointIndex, Real& sample)
    {
        switch (m_lineCount)
        {
        case 0:   // __|__|
        case 1:
        case 313:
        case 314:
        {
            int halfIndex = m_horizontalCount % (m_nbHorizPoints/2);

            if (halfIndex < (m_nbHorizPoints/2) - m_pointsPerSync) // ultra-black
            {
                sample = 0.0f;
            }
            else // black
            {
                sample = m_blackLevel;
            }
        }
            break;
        case 2:   // __||XX
            if (m_horizontalCount < (m_nbHorizPoints/2) - m_pointsPerSync)
            {
                sample = 0.0f;
            }
            else if (m_horizontalCount < (m_nbHorizPoints/2))
            {
                sample = m_blackLevel;
            }
            else if (m_horizontalCount < (m_nbHorizPoints/2) + m_pointsPerFSync)
            {
                sample = 0.0f;
            }
            else
            {
                sample = m_blackLevel;
            }
            break;
        case 3:   // |XX|XX
        case 4:
        case 310:
        case 311:
        case 315:
        case 316:
        case 622:
        case 623:
        case 624:
        {
            int halfIndex = m_horizontalCount % (m_nbHorizPoints/2);

            if (halfIndex < m_pointsPerFSync) // ultra-black
            {
                sample = 0.0f;
            }
            else // black
            {
                sample = m_blackLevel;
            }
        }
            break;
        case 312: // |XX__|
            if (m_horizontalCount < m_pointsPerFSync)
            {
                sample = 0.0f;
            }
            else if (m_horizontalCount < (m_nbHorizPoints/2))
            {
                sample = m_blackLevel;
            }
            else if (m_horizontalCount < m_nbHorizPoints - m_pointsPerSync)
            {
                sample = 0.0f;
            }
            else
            {
                sample = m_blackLevel;
            }
            break;
        default: // black images
            if (m_horizontalCount < m_pointsPerSync)
            {
                sample = 0.0f;
            }
            else
            {
                sample = m_blackLevel;
            }
            break;
        }
    }
};


#endif /* PLUGINS_CHANNELTX_MODATV_ATVMOD_H_ */
