///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_ATVDEMODSINK_H
#define INCLUDE_ATVDEMODSINK_H

#include <QElapsedTimer>
#include <vector>
#include <memory>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/agc.h"
#include "dsp/phaselock.h"
#include "dsp/recursivefilters.h"
#include "dsp/phasediscri.h"
#include "util/movingaverage.h"
#include "gui/tvscreenanalog.h"

#include "atvdemodsettings.h"

class ScopeVis;

class ATVDemodSink : public ChannelSampleSink {
public:
    ATVDemodSink();
	~ATVDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

  	void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void setTVScreen(TVScreenAnalog *tvScreen) //!< set by the GUI
    {
        m_registeredTVScreen = tvScreen;
        m_tvScreenBuffer = m_registeredTVScreen->getBackBuffer();
    }
    double getMagSq() const { return m_magSqAverage; } //!< Beware this is scaled to 2^30
    bool getBFOLocked();
    void setVideoTabIndex(int videoTabIndex) { m_videoTabIndex = videoTabIndex; }

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const ATVDemodSettings& settings, bool force = false);

private:
    struct ATVConfigPrivate
    {
        int m_intTVSampleRate;
        int m_intNumberSamplePerLine;

        ATVConfigPrivate() :
            m_intTVSampleRate(0),
            m_intNumberSamplePerLine(0)
        {}
    };

    /**
     * Exponential average using integers and alpha as the inverse of a power of two
     */
    class AvgExpInt
    {
    public:
        AvgExpInt(int log2Alpha) : m_log2Alpha(log2Alpha), m_m1(0), m_start(true) {}
        void reset() { m_start = true; }

        int run(int m0)
        {
            if (m_start)
            {
                m_m1 = m0;
                m_start = false;
                return m0;
            }
            else
            {
                m_m1 = m0 + m_m1 - (m_m1>>m_log2Alpha);
                return m_m1>>m_log2Alpha;
            }
        }

    private:
        int m_log2Alpha;
        int m_m1;
        bool m_start;
    };

    int m_channelSampleRate;
	int m_channelFrequencyOffset;
    int m_samplesPerLine;       //!< number of samples per complete line (includes sync signals) - adusted value
	float m_samplesPerLineFrac; //!< number of samples per complete line (includes sync signals), fractional part
	ATVDemodSettings m_settings;
    int m_videoTabIndex;

    //*************** SCOPE  ***************

    ScopeVis* m_scopeSink;
    SampleVector m_scopeSampleBuffer;

    //*************** ATV PARAMETERS  ***************
    TVScreenAnalog *m_registeredTVScreen;
	TVScreenAnalogBuffer *m_tvScreenBuffer;

    //int m_intNumberSamplePerLine;
    int m_numberSamplesPerHTop;        //!< number of samples per horizontal synchronization pulse (pulse in ultra-black) - integer value
    int m_numberOfBlackLines;          //!< this is the total number of lines not part of the image and is used for vertical screen size
    int m_firstVisibleLine;

    int m_fieldDetectStartPos;
    int m_fieldDetectEndPos;
    int m_vSyncDetectStartPos;
    int m_vSyncDetectEndPos;

    int m_vSyncDetectThreshold;
    int m_fieldDetectThreshold1;
    int m_fieldDetectThreshold2;

    int m_numberOfVSyncLines;
    int m_numberSamplesPerLineSignals; //!< number of samples in the non image part of the line (signals = front porch + pulse + back porch)
    int m_numberSamplesPerHSync;       //!< number of samples per horizontal synchronization pattern (pulse + back porch)
    int m_numberSamplesHSyncCrop;      //!< number of samples to crop from start of horizontal synchronization
    bool m_interleaved;                //!< interleaved image

    //*************** PROCESSING  ***************

    int m_fieldIndex;
    int m_synchroSamples;

    int m_fieldDetectSampleCount;
    int m_vSyncDetectSampleCount;

    float m_effMin;
    float m_effMax;

    float m_ampMin;
    float m_ampMax;
    float m_ampDelta; //!< calculated amplitude of HSync pulse (should be ~0.3f)

    float m_fltBufferI[6];
    float m_fltBufferQ[6];

	int m_amSampleIndex;

    int m_sampleOffset;         // assumed (averaged) sample offset from the start of horizontal sync pulse
	float m_sampleOffsetFrac;   // sample offset, fractional part
    int m_sampleOffsetDetected; // detected sample offset from the start of horizontal sync pulse
    int m_lineIndex;

	float m_hSyncShift;
    int m_hSyncErrorCount;

    float prevSample;
    int m_avgColIndex;

    SampleVector m_sampleBuffer;

    float m_sampleRangeCorrection;

    //*************** RF  ***************

    MovingAverageUtil<double, double, 32> m_magSqAverage;
    MovingAverageUtilVar<double, double> m_ampAverage;

    NCO m_nco;
    SimplePhaseLock m_bfoPLL;
    SecondOrderRecursiveFilter m_bfoFilter;

    // Used for vestigial SSB with asymmetrical filtering (needs double sideband scheme)
    fftfilt* m_DSBFilter;
    Complex* m_DSBFilterBuffer;
    int m_DSBFilterBufferIndex;
    static const int m_ssbFftLen;

    // Used for FM
    PhaseDiscriminators m_objPhaseDiscri;

    void demod(Complex& c);
    void applyStandard(int sampleRate, ATVDemodSettings::ATVStd atvStd, float lineDuration);

    inline void processSample(float& sample, int& sampleVideo)
    {
        // Filling pixel on the current line - reference index 0 at start of sync pulse
		m_tvScreenBuffer->setSampleValue(m_sampleOffset - m_numberSamplesPerHSync, sampleVideo);

        if (m_settings.m_hSync)
        {
            // Horizontal Synchro detection
            if ((prevSample >= m_settings.m_levelSynchroTop &&
                sample < m_settings.m_levelSynchroTop) // horizontal synchro detected
                && (m_sampleOffsetDetected > m_samplesPerLine - m_numberSamplesPerHTop))
            {
                float sampleOffsetDetectedFrac =
                    (sample - m_settings.m_levelSynchroTop) / (prevSample - sample);
                float hSyncShift = -m_sampleOffset - m_sampleOffsetFrac - sampleOffsetDetectedFrac;
                if (hSyncShift > m_samplesPerLine / 2)
					hSyncShift -= m_samplesPerLine;
                else if (hSyncShift < -m_samplesPerLine / 2)
					hSyncShift += m_samplesPerLine;

                if (fabs(hSyncShift) > m_numberSamplesPerHTop)
                {
                    m_hSyncErrorCount++;
                    if (m_hSyncErrorCount >= 4)
                    {
                        // Fast sync: shift is too large, needs to be fixed ASAP
						m_hSyncShift = hSyncShift;
                        m_hSyncErrorCount = 0;
                    }
                }
                else
                {
					// Slow sync: slight adjustment is needed
					m_hSyncShift = hSyncShift * 0.2f;
                    m_hSyncErrorCount = 0;
                }
				m_sampleOffsetDetected = 0;
            }
            else
				m_sampleOffsetDetected++;
        }
		m_sampleOffset++;

        if (m_settings.m_vSync)
        {
            if (m_sampleOffset > m_fieldDetectStartPos && m_sampleOffset < m_fieldDetectEndPos)
                m_fieldDetectSampleCount += sample < m_settings.m_levelSynchroTop;
            if (m_sampleOffset > m_vSyncDetectStartPos && m_sampleOffset < m_vSyncDetectEndPos)
                m_vSyncDetectSampleCount += sample < m_settings.m_levelSynchroTop;
        }

        // end of line
        if (m_sampleOffset >= m_samplesPerLine)
        {
            float sampleOffsetFloat = m_hSyncShift + m_sampleOffsetFrac - m_samplesPerLineFrac;
            m_sampleOffset = sampleOffsetFloat;
            m_sampleOffsetFrac = sampleOffsetFloat - m_sampleOffset;
			m_hSyncShift = 0.0f;

			m_lineIndex++;
			if (m_settings.m_atvStd == ATVDemodSettings::ATVStdHSkip) {
                processEOLHSkip();
            } else {
                processEOLClassic();
            }
        }

        prevSample = sample;
    }

    // Standard vertical sync
    inline void processEOLClassic()
    {
        if (m_lineIndex == m_numberOfVSyncLines + 3 && m_fieldIndex == 0)
        {
			m_tvScreenBuffer = m_registeredTVScreen->swapBuffers();
        }

        if (m_vSyncDetectSampleCount > m_vSyncDetectThreshold &&
            (m_lineIndex < 3 || m_lineIndex > m_numberOfVSyncLines + 1) && m_settings.m_vSync)
        {
            if (m_interleaved)
            {
                if (m_fieldDetectSampleCount > m_fieldDetectThreshold1)
                    m_fieldIndex = 0;
                else if (m_fieldDetectSampleCount < m_fieldDetectThreshold2)
                    m_fieldIndex = 1;
            }
            m_lineIndex = 2;
        }
        m_fieldDetectSampleCount = 0;
        m_vSyncDetectSampleCount = 0;

        if (m_lineIndex > m_settings.m_nbLines / 2 + m_fieldIndex && m_interleaved)
        {
            m_lineIndex = 1;
            m_fieldIndex = 1 - m_fieldIndex;
        }
        else if (m_lineIndex > m_settings.m_nbLines && !m_interleaved)
        {
            m_lineIndex = 1;
            m_fieldIndex = 0;
        }

        int rowIndex = m_lineIndex - m_firstVisibleLine;
        if (m_interleaved)
            rowIndex = rowIndex * 2 - m_fieldIndex;

		m_tvScreenBuffer->selectRow(rowIndex, m_sampleOffsetFrac);
	}

    // Vertical sync is obtained by skipping horizontal sync on the line that triggers vertical sync (new frame)
    inline void processEOLHSkip()
    {
		if ((m_sampleOffsetDetected > (3 * m_samplesPerLine) / 2) // Vertical sync is first horizontal sync after skip (count at least 1.5 line length)
            || (!m_settings.m_vSync && (m_lineIndex >= m_settings.m_nbLines))) // Vsync ignored and reached nominal number of lines per frame
        {
			m_tvScreenBuffer = m_registeredTVScreen->swapBuffers();
			m_lineIndex = 0;
        }

		m_tvScreenBuffer->selectRow(m_lineIndex, m_sampleOffsetFrac);
    }
};

#endif // INCLUDE_ATVDEMODSINK_H
