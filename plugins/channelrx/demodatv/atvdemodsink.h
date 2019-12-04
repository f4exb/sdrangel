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

#include "dsp/channelsamplesink.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/agc.h"
#include "dsp/phaselock.h"
#include "dsp/recursivefilters.h"
#include "dsp/phasediscri.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "gui/tvscreen.h"

#include "atvdemodsettings.h"

class ATVDemodSink : public ChannelSampleSink {
public:
    ATVDemodSink();
	~ATVDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

  	void setScopeSink(BasebandSampleSink* scopeSink) { m_scopeSink = scopeSink; }
    void setTVScreen(TVScreen *tvScreen) { m_registeredTVScreen = tvScreen; } //!< set by the GUI
    double getMagSq() const { return m_objMagSqAverage; } //!< Beware this is scaled to 2^30
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
    int m_tvSampleRate;
    unsigned int m_samplesPerLineNom; //!< number of samples per complete line (includes sync signals) - nominal value
    unsigned int m_samplesPerLine;    //!< number of samples per complete line (includes sync signals) - adusted value
    ATVDemodSettings m_settings;
    int m_videoTabIndex;

    //*************** SCOPE  ***************

    BasebandSampleSink* m_scopeSink;
    SampleVector m_scopeSampleBuffer;

    //*************** ATV PARAMETERS  ***************
    TVScreen *m_registeredTVScreen;

    //int m_intNumberSamplePerLine;
    int m_numberSamplesPerHTopNom;     //!< number of samples per horizontal synchronization pulse (pulse in ultra-black) - nominal value
    int m_numberSamplesPerHTop;        //!< number of samples per horizontal synchronization pulse (pulse in ultra-black) - adusted value
    int m_numberOfSyncLines;           //!< this is the number of non displayable lines at the start of a frame. First displayable row comes next.
    int m_numberOfBlackLines;          //!< this is the total number of lines not part of the image and is used for vertical screen size
    int m_numberOfEqLines;             //!< number of equalizing lines both whole and partial
    int m_numberSamplesPerLineSignals; //!< number of samples in the non image part of the line (signals = front porch + pulse + back porch)
    int m_numberSamplesPerHSync;       //!< number of samples per horizontal synchronization pattern (pulse + back porch)
    int m_numberSamplesHSyncCrop;      //!< number of samples to crop from start of horizontal synchronization
    bool m_interleaved;                //!< interleaved image
    int m_firstRowIndexEven;           //!< index of the first row of an even image
    int m_firstRowIndexOdd;            //!< index of the first row of an even image

    //*************** PROCESSING  ***************

    int m_imageIndex;
    int m_synchroSamples;

    bool m_horizontalSynchroDetected;
    bool m_verticalSynchroDetected;

    float m_ampLineSum;
    float m_ampLineAvg;

    float m_effMin;
    float m_effMax;

    float m_ampMin;
    float m_ampMax;
    float m_ampDelta; //!< calculated amplitude of HSync pulse (should be ~0.3f)
    float m_ampSample;

    float m_fltBufferI[6];
    float m_fltBufferQ[6];

    int m_colIndex;
    int m_sampleIndex;
    unsigned int m_amSampleIndex;
    int m_rowIndex;
    int m_lineIndex;

    AvgExpInt m_objAvgColIndex;
    int m_avgColIndex;

    SampleVector m_sampleBuffer;

    //*************** RF  ***************

    MovingAverageUtil<double, double, 32> m_objMagSqAverage;

    NCO m_nco;
    SimplePhaseLock m_bfoPLL;
    SecondOrderRecursiveFilter m_bfoFilter;

    // Interpolator group for decimation and/or double sideband RF filtering
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    // Used for vestigial SSB with asymmetrical filtering (needs double sideband scheme)
    fftfilt* m_DSBFilter;
    Complex* m_DSBFilterBuffer;
    int m_DSBFilterBufferIndex;
    static const int m_ssbFftLen;

    // Used for FM
    PhaseDiscriminators m_objPhaseDiscri;

    //QElapsedTimer m_objTimer;

    void demod(Complex& c);
    void applyStandard(int sampleRate, const ATVDemodSettings& settings, float lineDuration);

    // Vertical sync is obtained by skipping horizontal sync on the line that triggers vertical sync (new frame)
    inline void processHSkip(float& sample, int& sampleVideo)
    {
        // Fill pixel on the current line - column index 0 is reference at start of sync remove only sync length empirically
        m_registeredTVScreen->setDataColor(m_colIndex - m_numberSamplesHSyncCrop, sampleVideo, sampleVideo, sampleVideo);

        // Horizontal Synchro detection

        // Floor Detection (0.1 nominal)
        if (sample < m_settings.m_levelSynchroTop)
        {
            if (m_synchroSamples == 0) // AM scale reset on transition if within range
            {
                m_effMin = 2000000.0f;
                m_effMax = -2000000.0f;
                m_amSampleIndex = 0;
            }

            m_synchroSamples++;
        }
        // Black detection (0.3 nominal)
        else if (sample > m_settings.m_levelBlack) {
            m_synchroSamples = 0;
        }

        // Refine AM scale estimation on HSync pulse sequence
        if (m_amSampleIndex == (3*m_numberSamplesPerHTop)/2)
        {
            m_ampMin = m_effMin;
            m_ampMax = m_effMax;
            m_ampDelta = (m_ampMax - m_ampMin);

            if (m_ampDelta <= 0.0) {
                m_ampDelta = 0.3f;
            }
        }

        // H sync pulse
        m_horizontalSynchroDetected = (m_synchroSamples == m_numberSamplesPerHTop);

        if (m_horizontalSynchroDetected)
        {
            // Vertical sync and image rendering
            if ((m_sampleIndex >= (3*m_samplesPerLine) / 2) // Vertical sync is first horizontal sync after skip (count at least 1.5 line length)
             || (!m_settings.m_vSync && (m_lineIndex >= m_settings.m_nbLines))) // Vsync ignored and reached nominal number of lines per frame
            {
                // qDebug("ATVDemodSink::processHSkip: %sVSync: co: %d sa: %d li: %d",
                //     (m_settings.m_vSync ? "" : "no "), m_colIndex, m_sampleIndex, m_lineIndex);
                m_avgColIndex = m_colIndex;
                m_registeredTVScreen->renderImage(0);

                m_imageIndex++;
                m_lineIndex = 0;
                m_rowIndex = 0;
                m_registeredTVScreen->selectRow(m_rowIndex);
            }

            m_sampleIndex = 0; // reset after H sync
        }
        else
        {
            m_sampleIndex++;
        }

        if (m_colIndex < m_samplesPerLine + m_numberSamplesPerHTop - 1) // increment until full line + next horizontal pulse
        {
            m_colIndex++;
        }
        else // full line + next horizontal pulse => start of screen reference line
        {
            // set column index to start a new line
            if (m_settings.m_hSync && (m_lineIndex == 0)) { // start of a new frame - readjust sync position
                m_colIndex = m_numberSamplesPerHTop + (m_samplesPerLine - m_avgColIndex) / 2; // amortizing factor 1/2
            } else { // reset column index at end of sync pulse normally
                m_colIndex = m_numberSamplesPerHTop;
            }

            m_lineIndex++; // new line
            m_rowIndex++;  // new row

            if (m_rowIndex < m_settings.m_nbLines) {
                m_registeredTVScreen->selectRow(m_rowIndex);
            }
        }
    }

    // Vertical sync is obtained when the average level of signal on a line is below a certain threshold. This is obtained by lowering signal to ultra black during at least 3/4th of the line
    // We use directly the sum of line sample values
    inline void processClassic(float& sample, int& sampleVideo)
    {
        // Filling pixel on the current line - reference index 0 at start of sync pulse
        // remove only sync pulse empirically, +4 is to compensate shift due to hsync amortizing factor of 1/4
        m_registeredTVScreen->setDataColor(m_colIndex - m_numberSamplesHSyncCrop, sampleVideo, sampleVideo, sampleVideo);

        int synchroTimeSamples = (3 * m_samplesPerLine) / 4; // count 3/4 line globally
        float synchroTrameLevel =  0.5f * ((float) synchroTimeSamples) * m_settings.m_levelBlack; // threshold is half the black value over 3/4th of line samples

        // Horizontal Synchro detection

        // Floor Detection 0
        if (sample < m_settings.m_levelSynchroTop)
        {
            if ((m_synchroSamples == 0) && (m_ampSample > 0.25f) && (m_ampSample < 0.35f)) // AM scale reset on transition
            {
                m_effMin = 2000000.0f;;
                m_effMax = -2000000.0f;;
                m_amSampleIndex = 0;
            }

            m_synchroSamples++;
        }
        // Black detection 0.3
        else if (sample > m_settings.m_levelBlack) {
            m_synchroSamples = 0;
        }

        // Refine AM scale estimation on HSync pulse sequence
        if ((m_amSampleIndex == (3*m_numberSamplesPerHTop)/2) && (sample > 0.25f) && (sample < 0.35f))
        {
            m_ampSample = sample;
            m_ampMin = m_effMin;
            m_ampMax = m_effMax;
            m_ampDelta = (m_ampMax - m_ampMin);

            if (m_ampDelta <= 0.0) {
                m_ampDelta = 0.3f;
            }
        }

        // H sync pulse
        m_horizontalSynchroDetected = (m_synchroSamples == m_numberSamplesPerHTop) && (m_sampleIndex > (m_samplesPerLine/2) + m_numberSamplesPerLineSignals);

        //Horizontal Synchro processing

        if (m_horizontalSynchroDetected)
        {
            m_avgColIndex = m_sampleIndex - m_colIndex - (m_colIndex < m_samplesPerLine/2 ? 150 : 0);
            //qDebug("HSync: %d %d %d", m_sampleIndex, m_colIndex, m_avgColIndex);
            m_sampleIndex = 0;
        }
        else
        {
            m_sampleIndex++;
        }

        if (m_colIndex < m_samplesPerLine + m_numberSamplesPerHTop - 1) // increment until full line + next horizontal pulse
        {
            m_colIndex++;

            if (m_colIndex < (m_samplesPerLine/2)) { // count on first half of line for better separation between black and ultra black
                m_ampLineSum += sample;
            }
        }
        else // full line + next horizontal pulse => start of screen reference line
        {
            m_ampLineAvg = m_ampLineSum / ((m_samplesPerLine/2) - m_numberSamplesPerHTop); // avg length is half line less horizontal top
            m_ampLineSum = 0.0f;

            // set column index to start a new line
            if (m_settings.m_hSync && (m_lineIndex == 0)) {
                m_colIndex = m_numberSamplesPerHTop + m_avgColIndex/4; // amortizing 1/4
            } else {
                m_colIndex = m_numberSamplesPerHTop;
            }

            // process line
            m_lineIndex++; // new line
            m_rowIndex += m_interleaved ? 2 : 1; // new row considering interleaving

            if (m_rowIndex < m_settings.m_nbLines) {
                m_registeredTVScreen->selectRow(m_rowIndex - m_numberOfSyncLines);
            }
        }

        // Vertical sync and image rendering

        if (m_lineIndex > m_numberOfBlackLines) {
            m_verticalSynchroDetected = false; // reset trigger when detection zone is left
        }

        if ((m_settings.m_vSync) && (m_lineIndex <= m_settings.m_nbLines)) // VSync activated and lines in range
        {
            if (m_colIndex >= synchroTimeSamples)
            {
                if (m_ampLineAvg < 0.15f) // ultra black detection
                {
                    if (!m_verticalSynchroDetected) // not yet
                    {
                        m_verticalSynchroDetected = true; // prevent repetition

                        // Odd frame or not interleaved
                        if ((m_imageIndex % 2 == 1) || !m_interleaved) {
                            m_registeredTVScreen->renderImage(0);
                        }

                        if (m_lineIndex > m_settings.m_nbLines/2) { // long frame done (even)
                            m_imageIndex = m_firstRowIndexOdd;  // next is odd
                        } else {
                            m_imageIndex = m_firstRowIndexEven; // next is even
                        }

                        if (m_interleaved) {
                            m_rowIndex = m_imageIndex;
                        } else {
                            m_rowIndex = 0; // just the first line
                        }

                        // qDebug("ATVDemodSink::processClassic: m_lineIndex: %d m_imageIndex: %d m_rowIndex: %d",
                        //     m_lineIndex, m_imageIndex, m_rowIndex);
                        m_registeredTVScreen->selectRow(m_rowIndex - m_numberOfSyncLines);

                        m_lineIndex = 0;
                        m_imageIndex++;
                    }
                }
            }
        }
        else // no VSync or lines out of range => set new image arbitrarily
        {
            if (m_lineIndex >= m_settings.m_nbLines/2)
            {
                if (m_lineIndex > m_settings.m_nbLines/2) { // long frame done (even)
                    m_imageIndex = m_firstRowIndexOdd;  // next is odd
                } else {
                    m_imageIndex = m_firstRowIndexEven; // next is even
                }

                if (m_interleaved) {
                    m_rowIndex = m_imageIndex;
                } else {
                    m_rowIndex = 0; // just the first line
                }

                m_registeredTVScreen->selectRow(m_rowIndex - m_numberOfSyncLines);

                m_lineIndex = 0;
                m_imageIndex++;
            }
        }
    }
};


#endif // INCLUDE_ATVDEMODSINK_H