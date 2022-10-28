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

#ifndef PLUGINS_CHANNELTX_MODATV_ATVMODSOURCE_H_
#define PLUGINS_CHANNELTX_MODATV_ATVMODSOURCE_H_

#include <vector>

#include <QObject>
#include <QMutex>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/videoio.hpp>

#include <stdint.h>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "dsp/fftfilt.h"
#include "util/message.h"

#include "atvmodsettings.h"

class MessageQueue;

class ATVModSource : public ChannelSampleSource
{
public:
    ATVModSource();
    ~ATVModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples);

    int getEffectiveSampleRate() const { return m_tvSampleRate; };
    double getMagSq() const { return m_movingAverage.asDouble(); }
    void getCameraNumbers(std::vector<int>& numbers);

    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const ATVModSettings& settings, bool force = false);
    void openImage(const QString& fileName);
    void openVideo(const QString& fileName);
    void seekVideoFileStream(int seekPercentage);
    void reportVideoFileSourceStreamTiming();
    void configureCameraIndex(unsigned int index);
    void configureCameraData(uint32_t index, float mnaualFPS, bool manualFPSEnable);

    static void getBaseValues(int outputSampleRate, int linesPerSecond, int& sampleRateUnits, uint32_t& nbPointsPerRateUnit);
    static float getRFBandwidthDivisor(ATVModSettings::ATVModulation modulation);

private:
    class ATVCamera
    {
    public:
    	cv::VideoCapture m_camera;    //!< camera object
        cv::Mat m_videoframeOriginal; //!< camera non resized image
        cv::Mat m_videoFrame;         //!< displayable camera frame
    	int m_cameraNumber;           //!< camera device number
        float m_videoFPS;             //!< camera FPS rate
        float m_videoFPSManual;       //!< camera FPS rate manually set
        bool m_videoFPSManualEnable;  //!< Enable camera FPS rate manual set value
        int m_videoWidth;             //!< camera frame width
        int m_videoHeight;            //!< camera frame height
        float m_videoFx;              //!< camera horizontal scaling factor
        float m_videoFy;              //!< camera vertictal scaling factor
        float m_videoFPSq;            //!< camera FPS sacaling factor
        float m_videoFPSqManual;      //!< camera FPS sacaling factor manually set
        float m_videoFPSCount;        //!< camera FPS fractional counter
        int m_videoPrevFPSCount;      //!< camera FPS previous integer counter

        ATVCamera() :
        	m_cameraNumber(-1),
			m_videoFPS(25.0f),
			m_videoFPSManual(20.0f),
			m_videoFPSManualEnable(false),
        	m_videoWidth(1),
			m_videoHeight(1),
			m_videoFx(1.0f),
			m_videoFy(1.0f),
			m_videoFPSq(1.0f),
			m_videoFPSqManual(1.0f),
		    m_videoFPSCount(0.0f),
		    m_videoPrevFPSCount(0)
        {}

        ATVCamera(const ATVCamera& camera) :
            m_camera(camera.m_camera),
            m_videoframeOriginal(camera.m_videoframeOriginal),
            m_videoFrame(camera.m_videoFrame),
            m_cameraNumber(camera.m_cameraNumber),
            m_videoFPS(camera.m_videoFPS),
            m_videoFPSManual(camera.m_videoFPSManual),
            m_videoFPSManualEnable(camera.m_videoFPSManualEnable),
            m_videoWidth(camera.m_videoWidth),
            m_videoHeight(camera.m_videoHeight),
            m_videoFx(camera.m_videoFx),
            m_videoFy(camera.m_videoFy),
            m_videoFPSq(camera.m_videoFPSq),
            m_videoFPSqManual(camera.m_videoFPSqManual),
            m_videoFPSCount(camera.m_videoFPSCount),
            m_videoPrevFPSCount(camera.m_videoPrevFPSCount)
        {}
    };

    enum LineType
    {
        LineImage,            //!< Full image line
        LineImageHalf1Short,  //!< Half image line first then short pulse
        LineImageHalf1Broad,  //!< Half image line first then broad pulse
        LineImageHalf2,       //!< Half image line first black then image
        LineShortPulses,      //!< VSync short pulses a.k.a equalizing
        LineBroadPulses,      //!< VSync broad pulses a.k.a field sync
        LineShortBroadPulses, //!< VSync short then broad pulses
        LineBroadShortPulses, //!< VSync broad then short pulses
        LineShortBlackPulses, //!< VSync short pulse then black
        LineBlack,            //!< Full black line
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    ATVModSettings m_settings;

    NCO m_carrierNco;
    Complex m_modSample;
    float m_modPhasor; //!< For FM modulation
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    int      m_tvSampleRate;     //!< sample rate for generating signal
    uint32_t m_pointsPerLine;    //!< Number of points per full line
    int      m_pointsPerSync;    //!< number of line points for the horizontal sync
    int      m_pointsPerBP;      //!< number of line points for the back porch
    int      m_pointsPerImgLine; //!< number of line points for the image line
    uint32_t m_pointsPerFP;      //!< number of line points for the front porch
    int      m_pointsPerVEqu;    //!< number of line points for the equalizing (short) pulse
    int      m_pointsPerVSync;   //!< number of line points for the vertical sync (broad) pulse
    int      m_nbLines;          //!< number of lines per complete frame
    int      m_nbLines2;         //!< same number as above (non interlaced) or Euclidean half the number above (interlaced)
    int      m_nbLinesField1;    //!< In interlaced mode: number of lines in field1 transition included
    uint32_t m_nbImageLines2;    //!< half the number of effective image lines
    int      m_nbImageLines;     //!< number of effective image lines
    uint32_t m_imageLineStart1;  //!< start index of line for field 1
    uint32_t m_imageLineStart2;  //!< start index of line for field 2
    int      m_nbHorizPoints;    //!< number of line points per horizontal line
    float    m_blankLineLvel;    //!< video level of blank lines
    uint32_t m_pointsPerHBar;    //!< number of line points for a bar of the bar chart
    float    m_hBarIncrement;    //!< video level increment at each horizontal bar increment
    uint32_t m_linesPerVBar;     //!< number of lines for a bar of the bar chart
    float    m_vBarIncrement;    //!< video level increment at each vertical bar increment
    bool     m_interlaced;       //!< true if image is interlaced (2 half frames per frame)
    int      m_horizontalCount;  //!< current point index on line
    int      m_lineCount;        //!< current line index in frame
    int      m_imageLine;        //!< current line index in image
    float    m_fps;              //!< resulting frames per second
    LineType m_lineType;         //!< current line type

    MovingAverageUtil<double, double, 16> m_movingAverage;
    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    cv::Mat m_imageFromFile;     //!< original image not resized not overlaid by text
    cv::Mat m_imageOriginal;     //!< original not resized image
    cv::Mat m_image;             //!< resized image for transmission at given rate
    bool m_imageOK;

    cv::VideoCapture m_video;    //!< current video capture
    cv::Mat m_videoframeOriginal; //!< current frame from video
    cv::Mat m_videoFrame;        //!< current displayable video frame
    float m_videoFPS;            //!< current video FPS rate
    int m_videoWidth;            //!< current video frame width
    int m_videoHeight;           //!< current video frame height
    float m_videoFx;             //!< current video horizontal scaling factor
    float m_videoFy;             //!< current video vertictal scaling factor
    float m_videoFPSq;           //!< current video FPS sacaling factor
    float m_videoFPSCount;       //!< current video FPS fractional counter
    int m_videoPrevFPSCount;     //!< current video FPS previous integer counter
    int m_videoLength;           //!< current video length in frames
    bool m_videoEOF;             //!< current video has reached end of file
    bool m_videoOK;

    std::vector<ATVCamera> m_cameras; //!< vector of available cameras
    int m_cameraIndex;           //!< current camera index in list of available cameras

    std::string m_overlayText;
    QString m_imageFileName;
    QString m_videoFileName;

    // Used for standard SSB
    fftfilt* m_SSBFilter;
    Complex* m_SSBFilterBuffer;
    int m_SSBFilterBufferIndex;

    // Used for vestigial SSB with asymmetrical filtering (needs double sideband scheme)
    fftfilt* m_DSBFilter;
    Complex* m_DSBFilterBuffer;
    int m_DSBFilterBufferIndex;

    MessageQueue *m_messageQueueToGUI;

    static const int m_ssbFftLen;
    static const float m_blackLevel;
    static const float m_spanLevel;
    static const int m_levelNbSamples;
    static const int m_nbBars; //!< number of bars in bar or chessboard patterns
    static const int m_cameraFPSTestNbFrames; //!< number of frames for camera FPS test

    static const LineType StdPAL625_F1Start[];
    static const LineType StdPAL625_F2Start[];
    static const LineType StdPAL525_F1Start[];
    static const LineType StdPAL525_F2Start[];
    static const LineType Std819_F1Start[];
    static const LineType Std819_F2Start[];
    static const LineType StdShort_F1Start[];
    static const LineType StdShort_F2Start[];

    void pullFinalize(Complex& ci, Sample& sample);
    void pullVideo(Real& sample);
    void calculateLevel(Real& sample);
    void modulateSample();
    Complex& modulateSSB(Real& sample);
    Complex& modulateVestigialSSB(Real& sample);
    void applyStandard(const ATVModSettings& settings);
    void resizeImage();
    void calculateVideoSizes();
    void resizeVideo();
    void scanCameras();
    void releaseCameras();
    void calculateCamerasSizes();
    void resizeCameras();
    void resizeCamera();
    void mixImageAndText(cv::Mat& image);

    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }

    inline LineType getLineType(ATVModSettings::ATVStd standard, int lineNumber)
    {
        if (standard == ATVModSettings::ATVStdHSkip)
        {
            return LineImage; // all lines are image lines
        }
        else if (standard == ATVModSettings::ATVStdPAL625) // m_nbLines2 = 312 - fieldLine 0 = 313
        {
            if (lineNumber < m_nbLines2)
            {
                if (lineNumber < 5) { // field 1 start (0..4 in line index = line number - 1)
                    return StdPAL625_F1Start[lineNumber];
                } else if (lineNumber < 22) { // field 1 black lines (5..21)
                    return LineBlack;
                } else if (lineNumber == 22) { // field 1 half image line (22)
                    return LineImageHalf2;
                } else if (lineNumber < m_nbLines2 - 2) { // field 1 full image (23..309)
                    return LineImage;
                } else if (lineNumber < m_nbLines2) { // field 1 bottom (310..311)
                    return LineShortPulses;
                }
            }
            else if (lineNumber == m_nbLines2)  // field transition 1 -> 2 (312)
            {
                return LineShortBroadPulses;
            }
            else
            {
                int fieldLine = lineNumber - m_nbLines2 - 1;

                if (fieldLine < 5) { // field 2 start (313..(313+5-1=317))
                    return StdPAL625_F2Start[fieldLine];
                } else if (fieldLine < 22) { // field 2 black lines (318..(313+22-1=334))
                    return LineBlack;
                } else if (fieldLine < m_nbLines2 - 3) { // field 2 full image (335..(313+309-1=621))
                    return LineImage;
                } else if (fieldLine == m_nbLines2 - 3) { // field 2 half image line (313+309=622)
                    return LineImageHalf1Short;
                } else { // field 2 bottom (623..624..)
                    return LineShortPulses;
                }
            }
        }
        else if (standard == ATVModSettings::ATVStdPAL525) // m_nbLines2 = 262 - fieldLine 0 = 263
        {
            if (lineNumber < m_nbLines2)
            {

                if (lineNumber < 9) { // field 1 start (0..8 in line index)
                    return StdPAL525_F1Start[lineNumber];
                } else if (lineNumber < 20) { // field 1 black lines (9..19)
                    return LineBlack;
                } else if (lineNumber < m_nbLines2) { // field 1 full image (20..261)
                    return LineImage;
                }
            }
            else if (lineNumber == m_nbLines2) // field transition 1 -> 2 or field 1 half image line (262)
            {
                return LineImageHalf1Short;
            }
            else
            {
                int fieldLine = lineNumber - m_nbLines2 - 1;

                if (fieldLine < 9) { // field 2 start (263..(263+9-1=271))
                    return StdPAL525_F2Start[fieldLine];
                } else if (fieldLine < 19) { // field 2 black lines (272..(263+19-1=281))
                    return LineBlack;
                } else if (fieldLine == 19) { // field 2 half image line (263+19=282)
                    return LineImageHalf2;
                } else if (fieldLine < m_nbLines2) { // field 2 full line (283..(263+262-1=524))
                    return LineImage;
                } else { // failsafe: should not get there - same as field 1 start first line
                    return LineShortPulses;
                }
            }
        }
        else if (standard == ATVModSettings::ATVStd819) // Standard F (Belgian) - m_nbLines2 = 409 - fieldLine 0 = 409
        {
            if (lineNumber < m_nbLines2)
            {
                if (lineNumber < 7) { // field 1 start (0..6 in line index)
                    return Std819_F1Start[lineNumber];
                } else if (lineNumber < 26) { // field 1 black lines (7..25)
                    return LineBlack;
                } else if (lineNumber == 26) { // field 1 half image line (26)
                    return LineImageHalf2;
                } else if (lineNumber < m_nbLines2 - 3) { // field 1 full image (27..405) - 379 lines
                    return LineImage;
                } else if (lineNumber < m_nbLines2) { // field 1 bottom (405..408)
                    return LineShortPulses;
                }
            }
            else if (lineNumber == m_nbLines2) // transition field 1 -> 2 (409)
            {
                return LineShortBroadPulses;
            }
            else
            {
                int fieldLine = lineNumber - m_nbLines2 - 1;

                if (fieldLine < 6) { // field 2 start (410..(410+6-1=415))
                    return Std819_F2Start[fieldLine];
                } else if (fieldLine < 26) { // field 2 black lines (416..(410+26-1=435))
                    return LineBlack;
                } else if (fieldLine < m_nbLines2 - 4) { // field 2 full image (436..(410+405-1=814)) - 379 lines
                    return LineImage;
                } else if (fieldLine == m_nbLines2 - 4) { // field 2 half image line (410+405=815)
                    return LineImageHalf1Short;
                } else { // field 2 bottom (816..818..)
                    return LineShortPulses;
                }
            }
        }
        else if (standard == ATVModSettings::ATVStdShortInterlaced)
        {
            if (lineNumber < m_nbLines2)
            {
                if (lineNumber < 2) {
                    return StdShort_F1Start[lineNumber];
                } else {
                    return LineImage;
                }
            }
            else
            {
                int fieldLine = lineNumber - m_nbLines2;

                if (fieldLine < 2) {
                    return StdShort_F2Start[fieldLine];
                } else if (fieldLine < m_nbLines2) {
                    return LineImage;
                } else { // failsafe - will add a black line for odd number of lines
                    return LineBlack;
                }
            }
        }
        else if (standard == ATVModSettings::ATVStdShort)
        {
            if (lineNumber < 2) {
                return StdShort_F2Start[lineNumber];
            } else if (lineNumber < m_nbLines) {
                return LineImage;
            } else {
                return LineBlack;
            }
        }

        return LineBlack;
    }

    inline void pullImageLastHalfSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerSync + m_pointsPerBP) { // sync
            pullImageSample(sample);
        } else if (m_horizontalCount < (m_nbHorizPoints/2)) {
            sample = m_blackLevel;
        } else {
            pullImageSample(sample);
        }
    }

    inline void pullImageFirstHalfShortSample(Real& sample)
    {
        if (m_horizontalCount < (m_nbHorizPoints/2)) {
            pullImageSample(sample);
        } else {
            pullVSyncLineEquPulsesSample(sample);
        }
    }

    inline void pullImageFirstHalfBroadSample(Real& sample)
    {
        if (m_horizontalCount < (m_nbHorizPoints/2)) {
            pullImageSample(sample);
        } else {
            pullVSyncLineLongPulsesSample(sample);
        }
    }

    inline void pullImageSample(Real& sample, bool noHSync = false)
    {
        if (m_horizontalCount < m_pointsPerSync) // sync pulse
        {
            sample = noHSync ? m_blackLevel : 0.0f; // ultra-black
        }
        else if (m_horizontalCount < m_pointsPerSync + m_pointsPerBP) // back porch
        {
            sample = m_blackLevel; // black
        }
        else if (m_horizontalCount < m_pointsPerSync + m_pointsPerBP + m_pointsPerImgLine)
        {
            if (m_imageLine >= m_nbImageLines) // out of image zone
            {
                sample = m_spanLevel * m_settings.m_uniformLevel + m_blackLevel; // uniform line
                return;
            }

            int pointIndex = m_horizontalCount - (m_pointsPerSync + m_pointsPerBP);

            switch(m_settings.m_atvModInput)
            {
            case ATVModSettings::ATVModInputHBars:
                sample = (pointIndex / m_pointsPerHBar) * m_hBarIncrement + m_blackLevel;
                break;
            case ATVModSettings::ATVModInputVBars:
                sample = (m_imageLine / m_linesPerVBar) * m_vBarIncrement + m_blackLevel;
                break;
            case ATVModSettings::ATVModInputChessboard:
                sample = (((m_imageLine / m_linesPerVBar)*5 + (pointIndex / m_pointsPerHBar)) % 2) * m_spanLevel * m_settings.m_uniformLevel + m_blackLevel;
                break;
            case ATVModSettings::ATVModInputHGradient:
                sample = (pointIndex / (float) m_pointsPerImgLine) * m_spanLevel + m_blackLevel;
                break;
            case ATVModSettings::ATVModInputVGradient:
                sample = (m_imageLine / (float) m_nbImageLines) * m_spanLevel + m_blackLevel;
                break;
            case ATVModSettings::ATVModInputDiagonal:
                sample = pointIndex < (m_imageLine * m_pointsPerImgLine) / m_nbImageLines ? m_blackLevel : m_settings.m_uniformLevel + m_blackLevel;
                break;
            case ATVModSettings::ATVModInputImage:
                if (!m_imageOK || m_image.empty())
                {
                    sample = m_spanLevel * m_settings.m_uniformLevel + m_blackLevel;
                }
                else
                {
                	unsigned char pixv;
                    pixv = m_image.at<unsigned char>(m_imageLine, pointIndex); // row (y), col (x)
                    sample = (pixv / 256.0f) * m_spanLevel + m_blackLevel;
                }
                break;
            case ATVModSettings::ATVModInputVideo:
                if (!m_videoOK || m_videoFrame.empty())
                {
                    sample = m_spanLevel * m_settings.m_uniformLevel + m_blackLevel;
                }
                else
                {
                	unsigned char pixv;
                    pixv = m_videoFrame.at<unsigned char>(m_imageLine, pointIndex); // row (y), col (x)
                    sample = (pixv / 256.0f) * m_spanLevel + m_blackLevel;
                }
            	break;
            case ATVModSettings::ATVModInputCamera:
                if (m_cameraIndex < 0)
                {
                    sample = m_spanLevel * m_settings.m_uniformLevel + m_blackLevel;
                }
                else
                {
                    ATVCamera& camera = m_cameras[m_cameraIndex];

                    if (camera.m_videoFrame.empty())
                    {
                        sample = m_spanLevel * m_settings.m_uniformLevel + m_blackLevel;
                    }
                    else
                    {
                        unsigned char pixv;
                        pixv = camera.m_videoFrame.at<unsigned char>(m_imageLine, pointIndex); // row (y), col (x)
                        sample = (pixv / 256.0f) * m_spanLevel + m_blackLevel;
                    }
                }
                break;
            case ATVModSettings::ATVModInputUniform:
            default:
                sample = m_spanLevel * m_settings.m_uniformLevel + m_blackLevel; // uniform line
            }
        }
        else // front porch
        {
            sample = m_blackLevel; // black
        }
    }

    inline void pullVSyncLineEquPulsesSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerVEqu) {
            sample = 0.0f; // ultra-black
        } else if (m_horizontalCount < (m_nbHorizPoints/2)) {
            sample = m_blackLevel; // black
        } else if (m_horizontalCount < (m_nbHorizPoints/2) + m_pointsPerVEqu) {
            sample = 0.0f; // ultra-black
        } else {
            sample = m_blackLevel; // black
        }
    }

    inline void pullVSyncLineLongPulsesSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerVSync) {
            sample = 0.0f; // ultra-black
        } else if (m_horizontalCount < (m_nbHorizPoints/2)) {
            sample = m_blackLevel; // black
        } else if (m_horizontalCount < (m_nbHorizPoints/2) + m_pointsPerVSync) {
            sample = 0.0f; // ultra-black
        } else {
            sample = m_blackLevel; // black
        }
    }

    inline void pullVSyncLineEquLongPulsesSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerVEqu) {
            sample = 0.0f; // ultra-black
        } else if (m_horizontalCount < (m_nbHorizPoints/2)) {
            sample = m_blackLevel; // black
        } else if (m_horizontalCount < (m_nbHorizPoints/2) + m_pointsPerVSync) {
            sample = 0.0f; // ultra-black
        } else {
            sample = m_blackLevel; // black
        }
    }

    inline void pullVSyncLineEquBlackPulsesSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerVEqu) {
            sample = 0.0f; // ultra-black
        } else {
            sample = m_blackLevel; // black
        }
    }

    inline void pullVSyncLineLongEquPulsesSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerVSync) {
            sample = 0.0f; // ultra-black
        } else if (m_horizontalCount < (m_nbHorizPoints/2)) {
            sample = m_blackLevel; // black
        } else if (m_horizontalCount < (m_nbHorizPoints/2) + m_pointsPerVEqu) {
            sample = 0.0f; // ultra-black
        } else {
            sample = m_blackLevel; // black
        }
    }

    inline void pullVSyncLineLongBlackPulsesSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerVSync) {
            sample = 0.0f; // ultra-black
        } else {
            sample = m_blackLevel; // black
        }
    }

    inline void pullBlackLineSample(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerSync) {
            sample = 0.0f; // ultra-black
        } else {
            sample = m_blackLevel; // black
        }
    }
};


#endif /* PLUGINS_CHANNELTX_MODATV_ATVMOD_H_ */
