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

#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <stdint.h>

#include "dsp/basebandsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/fftfilt.h"
#include "util/message.h"

class ATVMod : public BasebandSampleSource {
    Q_OBJECT

public:
    typedef enum
    {
        ATVStdPAL625,
		ATVStdPAL525,
		ATVStd525L20F,
		ATVStd405
    } ATVStd;

    typedef enum
    {
        ATVModInputUniform,
        ATVModInputHBars,
        ATVModInputVBars,
        ATVModInputChessboard,
        ATVModInputHGradient,
        ATVModInputVGradient,
        ATVModInputImage,
        ATVModInputVideo,
		ATVModInputCamera
    } ATVModInput;

    typedef enum
    {
    	ATVModulationAM,
		ATVModulationFM,
		ATVModulationUSB,
		ATVModulationLSB,
        ATVModulationVestigialUSB,
        ATVModulationVestigialLSB
    } ATVModulation;

    class MsgConfigureImageFileName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureImageFileName* create(const QString& fileName)
        {
            return new MsgConfigureImageFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureImageFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureVideoFileName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureVideoFileName* create(const QString& fileName)
        {
            return new MsgConfigureVideoFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureVideoFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureVideoFileSourceSeek : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureVideoFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureVideoFileSourceSeek(seekPercentage);
        }

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureVideoFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
        { }
    };

    class MsgConfigureVideoFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgConfigureVideoFileSourceStreamTiming* create()
        {
            return new MsgConfigureVideoFileSourceStreamTiming();
        }

    private:

        MsgConfigureVideoFileSourceStreamTiming() :
            Message()
        { }
    };

    class MsgReportVideoFileSourceStreamTiming : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFrameCount() const { return m_frameCount; }

        static MsgReportVideoFileSourceStreamTiming* create(int frameCount)
        {
            return new MsgReportVideoFileSourceStreamTiming(frameCount);
        }

    protected:
        int m_frameCount;

        MsgReportVideoFileSourceStreamTiming(int frameCount) :
            Message(),
            m_frameCount(frameCount)
        { }
    };

    class MsgReportVideoFileSourceStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFrameRate() const { return m_frameRate; }
        quint32 getVideoLength() const { return m_videoLength; }

        static MsgReportVideoFileSourceStreamData* create(int frameRate,
                quint32 recordLength)
        {
            return new MsgReportVideoFileSourceStreamData(frameRate, recordLength);
        }

    protected:
        int m_frameRate;
        int m_videoLength; //!< Video length in frames

        MsgReportVideoFileSourceStreamData(int frameRate,
                int videoLength) :
            Message(),
            m_frameRate(frameRate),
            m_videoLength(videoLength)
        { }
    };

    class MsgConfigureCameraIndex : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }

        static MsgConfigureCameraIndex* create(int index)
        {
            return new MsgConfigureCameraIndex(index);
        }

    private:
        int m_index;

        MsgConfigureCameraIndex(int index) :
            Message(),
			m_index(index)
        { }
    };

    class MsgConfigureCameraData : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }
        float getManualFPS() const { return m_manualFPS; }
        bool getManualFPSEnable() const { return m_manualFPSEnable; }

        static MsgConfigureCameraData* create(
        		int index,
				float manualFPS,
				bool manualFPSEnable)
        {
            return new MsgConfigureCameraData(index, manualFPS, manualFPSEnable);
        }

    private:
        int m_index;
        float m_manualFPS;
        bool m_manualFPSEnable;

        MsgConfigureCameraData(int index, float manualFPS, bool manualFPSEnable) :
            Message(),
			m_index(index),
			m_manualFPS(manualFPS),
			m_manualFPSEnable(manualFPSEnable)
        { }
    };

    class MsgReportCameraData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getdeviceNumber() const { return m_deviceNumber; }
        float getFPS() const { return m_fps; }
        float getFPSManual() const { return m_fpsManual; }
        bool getFPSManualEnable() const { return m_fpsManualEnable; }
        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }
        int getStatus() const { return m_status; }

        static MsgReportCameraData* create(
        		int deviceNumber,
				float fps,
				float fpsManual,
				bool fpsManualEnable,
				int width,
				int height,
				int status)
        {
            return new MsgReportCameraData(
            		deviceNumber,
					fps,
					fpsManual,
					fpsManualEnable,
					width,
					height,
					status);
        }

    protected:
        int m_deviceNumber;
        float m_fps;
        float m_fpsManual;
        bool m_fpsManualEnable;
        int m_width;
        int m_height;
        int m_status;

        MsgReportCameraData(
        		int deviceNumber,
        		float fps,
				float fpsManual,
				bool fpsManualEnable,
				int width,
				int height,
				int status) :
            Message(),
			m_deviceNumber(deviceNumber),
			m_fps(fps),
			m_fpsManual(fpsManual),
			m_fpsManualEnable(fpsManualEnable),
			m_width(width),
			m_height(height),
			m_status(status)
        { }
    };

    class MsgConfigureOverlayText : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getOverlayText() const { return m_overlayText; }

        static MsgConfigureOverlayText* create(const QString& overlayText)
        {
            return new MsgConfigureOverlayText(overlayText);
        }

    private:
        QString m_overlayText;

        MsgConfigureOverlayText(const QString& overlayText) :
            Message(),
            m_overlayText(overlayText)
        { }
    };

    class MsgConfigureShowOverlayText : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getShowOverlayText() const { return m_showOverlayText; }

        static MsgConfigureShowOverlayText* create(bool showOverlayText)
        {
            return new MsgConfigureShowOverlayText(showOverlayText);
        }

    private:
        bool m_showOverlayText;

        MsgConfigureShowOverlayText(bool showOverlayText) :
            Message(),
            m_showOverlayText(showOverlayText)
        { }
    };

    class MsgReportEffectiveSampleRate : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        uint32_t gatNbPointsPerLine() const { return m_nbPointsPerLine; }

        static MsgReportEffectiveSampleRate* create(int sampleRate, uint32_t nbPointsPerLine)
        {
            return new MsgReportEffectiveSampleRate(sampleRate, nbPointsPerLine);
        }

    protected:
        int m_sampleRate;
        uint32_t m_nbPointsPerLine;

        MsgReportEffectiveSampleRate(
                int sampleRate,
                uint32_t nbPointsPerLine) :
            Message(),
            m_sampleRate(sampleRate),
            m_nbPointsPerLine(nbPointsPerLine)
        { }
    };

    ATVMod();
    ~ATVMod();

    void configure(MessageQueue* messageQueue,
            Real rfBandwidth,
            Real rfOppBandwidth,
            ATVStd atvStd,
            int nbLines,
            int fps,
            ATVModInput atvModInput,
            Real uniformLevel,
			ATVModulation atvModulation,
			bool videoPlayLoop,
			bool videoPlay,
			bool cameraPLay,
            bool channelMute,
            bool invertedVideo,
            float rfScaling,
            float fmExcursion,
            bool forceDecimator);

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples); // this is used for video signal actually
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    int getEffectiveSampleRate() const { return m_tvSampleRate; };
    Real getMagSq() const { return m_movingAverage.average(); }
    void getCameraNumbers(std::vector<int>& numbers);

    static void getBaseValues(int outputSampleRate, int linesPerSecond, int& sampleRateUnits, uint32_t& nbPointsPerRateUnit);
    static float getRFBandwidthDivisor(ATVModulation modulation);

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
        Real getRFOppBandwidth() const { return m_rfOppBandwidth; }
        ATVStd getATVStd() const { return m_atvStd; }
        ATVModInput getATVModInput() const { return m_atvModInput; }
        int getNbLines() const { return m_nbLines; }
        int getFPS() const { return m_fps; }
        Real getUniformLevel() const { return m_uniformLevel; }
        ATVModulation getModulation() const { return m_atvModulation; }
        bool getVideoPlayLoop() const { return m_videoPlayLoop; }
        bool getVideoPlay() const { return m_videoPlay; }
        bool getCameraPlay() const { return m_cameraPlay; }
        bool getChannelMute() const { return m_channelMute; }
        bool getInvertedVideo() const { return m_invertedVideo; }
        float getRFScaling() const { return m_rfScaling; }
        float getFMExcursion() const { return m_fmExcursion; }
        bool getForceDecimator() const { return m_forceDecimator; }

        static MsgConfigureATVMod* create(
            Real rfBandwidth,
            Real rfOppBandwidth,
            ATVStd atvStd,
            int nbLines,
            int fps,
            ATVModInput atvModInput,
            Real uniformLevel,
			ATVModulation atvModulation,
			bool videoPlayLoop,
			bool videoPlay,
			bool cameraPlay,
			bool channelMute,
			bool invertedVideo,
			float rfScaling,
			float fmExcursion,
			bool forceDecimator)
        {
            return new MsgConfigureATVMod(
                    rfBandwidth,
                    rfOppBandwidth,
                    atvStd,
                    nbLines,
                    fps,
                    atvModInput,
                    uniformLevel,
                    atvModulation,
                    videoPlayLoop,
                    videoPlay,
                    cameraPlay,
					channelMute,
					invertedVideo,
					rfScaling,
					fmExcursion,
					forceDecimator);
        }

    private:
        Real          m_rfBandwidth;
        Real          m_rfOppBandwidth;
        ATVStd        m_atvStd;
        int           m_nbLines;
        int           m_fps;
        ATVModInput   m_atvModInput;
        Real          m_uniformLevel;
        ATVModulation m_atvModulation;
        bool          m_videoPlayLoop;
        bool          m_videoPlay;
        bool          m_cameraPlay;
        bool          m_channelMute;
        bool          m_invertedVideo;
        float         m_rfScaling;
        float         m_fmExcursion;
        bool          m_forceDecimator;

        MsgConfigureATVMod(
                Real rfBandwidth,
                Real rfOppBandwidth,
                ATVStd atvStd,
                int nbLines,
                int fps,
                ATVModInput atvModInput,
                Real uniformLevel,
				ATVModulation atvModulation,
				bool videoPlayLoop,
				bool videoPlay,
				bool cameraPlay,
				bool channelMute,
				bool invertedVideo,
				float rfScaling,
				float fmExcursion,
				bool forceDecimator) :
            Message(),
            m_rfBandwidth(rfBandwidth),
            m_rfOppBandwidth(rfOppBandwidth),
            m_atvStd(atvStd),
            m_nbLines(nbLines),
            m_fps(fps),
            m_atvModInput(atvModInput),
            m_uniformLevel(uniformLevel),
			m_atvModulation(atvModulation),
			m_videoPlayLoop(videoPlayLoop),
			m_videoPlay(videoPlay),
			m_cameraPlay(cameraPlay),
			m_channelMute(channelMute),
			m_invertedVideo(invertedVideo),
			m_rfScaling(rfScaling),
			m_fmExcursion(fmExcursion),
			m_forceDecimator(forceDecimator)
        { }
    };

    struct ATVCamera
    {
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
    };

    struct Config
    {
        int           m_outputSampleRate;     //!< sample rate from channelizer
        qint64        m_inputFrequencyOffset; //!< offset from baseband center frequency
        Real          m_rfBandwidth;          //!< Bandwidth of modulated signal or direct sideband for SSB / vestigial SSB
        Real          m_rfOppBandwidth;       //!< Bandwidth of opposite sideband for vestigial SSB
        ATVStd        m_atvStd;               //!< Standard
        int           m_nbLines;              //!< Number of lines per full frame
        int           m_fps;                  //!< Number of frames per second
        ATVModInput   m_atvModInput;          //!< Input source type
        Real          m_uniformLevel;         //!< Percentage between black and white for uniform screen display
        ATVModulation m_atvModulation;        //!< RF modulation type
        bool          m_videoPlayLoop;        //!< Play video in a loop
        bool          m_videoPlay;            //!< True to play video and false to pause
        bool          m_cameraPlay;           //!< True to play camera video and false to pause
        bool          m_channelMute;          //!< Mute channel baseband output
        bool          m_invertedVideo;        //!< True if video signal is inverted before modulation
        float         m_rfScalingFactor;      //!< Scaling factor from +/-1 to +/-2^15
        float         m_fmExcursion;          //!< FM excursion factor relative to full bandwidth
        bool          m_forceDecimator;       //!< Forces decimator even when channel and source sample rates are equal

        Config() :
            m_outputSampleRate(-1),
            m_inputFrequencyOffset(0),
            m_rfBandwidth(0),
            m_rfOppBandwidth(0),
            m_atvStd(ATVStdPAL625),
            m_nbLines(625),
            m_fps(25),
            m_atvModInput(ATVModInputHBars),
            m_uniformLevel(0.5f),
			m_atvModulation(ATVModulationAM),
			m_videoPlayLoop(false),
			m_videoPlay(false),
			m_cameraPlay(false),
			m_channelMute(false),
			m_invertedVideo(false),
			m_rfScalingFactor(29204.0f), // -1dB
            m_fmExcursion(0.5f),         // half bandwidth
            m_forceDecimator(false)
        { }
    };

    Config m_config;
    Config m_running;

    NCO m_carrierNco;
    Complex m_modSample;
    float m_modPhasor; //!< For FM modulation
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    int      m_tvSampleRate;     //!< sample rate for generating signal
    uint32_t m_pointsPerLine;    //!< Number of points per full line
    uint32_t m_pointsPerSync;    //!< number of line points for the horizontal sync
    uint32_t m_pointsPerBP;      //!< number of line points for the back porch
    uint32_t m_pointsPerImgLine; //!< number of line points for the image line
    uint32_t m_pointsPerFP;      //!< number of line points for the front porch
    uint32_t m_pointsPerFSync;   //!< number of line points for the field first sync
    uint32_t m_pointsPerHBar;    //!< number of line points for a bar of the bar chart
    uint32_t m_linesPerVBar;     //!< number of lines for a bar of the bar chart
    uint32_t m_pointsPerTU;      //!< number of line points per time unit
    uint32_t m_nbLines;          //!< number of lines per complete frame
    uint32_t m_nbLines2;         //!< same number as above (non interlaced) or half the number above (interlaced)
    uint32_t m_nbImageLines;     //!< number of image lines excluding synchronization lines
    uint32_t m_nbImageLines2;    //!< same number as above (non interlaced) or half the number above (interlaced)
    uint32_t m_nbHorizPoints;    //!< number of line points per horizontal line
    uint32_t m_nbSyncLinesHead;  //!< number of header sync lines
    uint32_t m_nbBlankLines;     //!< number of lines in a frame (full or half) that are blanked (black) at the top of the image
    float    m_hBarIncrement;    //!< video level increment at each horizontal bar increment
    float    m_vBarIncrement;    //!< video level increment at each vertical bar increment
    bool     m_interlaced;       //!< true if image is interlaced (2 half frames per frame)
    bool     m_evenImage;        //!< in interlaced mode true if this is an even image
    QMutex   m_settingsMutex;
    int      m_horizontalCount;  //!< current point index on line
    int      m_lineCount;        //!< current line index in frame
    float    m_fps;              //!< resulting frames per second

    MovingAverage<Real> m_movingAverage;
    quint32 m_levelCalcCount;
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
    int m_cameraIndex;           //!< curent camera index in list of available cameras

    std::string m_overlayText;
    bool m_showOverlayText;

    // Used for standard SSB
    fftfilt* m_SSBFilter;
    Complex* m_SSBFilterBuffer;
    int m_SSBFilterBufferIndex;

    // Used for vestigial SSB with asymmetrical filtering (needs double sideband scheme)
    fftfilt* m_DSBFilter;
    Complex* m_DSBFilterBuffer;
    int m_DSBFilterBufferIndex;

    static const int m_ssbFftLen;

    static const float m_blackLevel;
    static const float m_spanLevel;
    static const int m_levelNbSamples;
    static const int m_nbBars; //!< number of bars in bar or chessboard patterns
    static const int m_cameraFPSTestNbFrames; //!< number of frames for camera FPS test

    void apply(bool force = false);
    void pullFinalize(Complex& ci, Sample& sample);
    void pullVideo(Real& sample);
    void calculateLevel(Real& sample);
    void modulateSample();
    Complex& modulateSSB(Real& sample);
    Complex& modulateVestigialSSB(Real& sample);
    void applyStandard(int rateUnits, int nbPointsPerRateUnit);
    void openImage(const QString& fileName);
    void openVideo(const QString& fileName);
    void resizeImage();
    void calculateVideoSizes();
    void resizeVideo();
    void seekVideoFileStream(int seekPercentage);
    void scanCameras();
    void releaseCameras();
    void calculateCamerasSizes();
    void resizeCameras();
    void resizeCamera();
    void mixImageAndText(cv::Mat& image);

    inline void pullImageLine(Real& sample)
    {
        if (m_horizontalCount < m_pointsPerSync) // sync pulse
        {
            sample = 0.0f; // ultra-black
        }
        else if (m_horizontalCount < m_pointsPerSync + m_pointsPerBP) // back porch
        {
            sample = m_blackLevel; // black
        }
        else if (m_horizontalCount < m_pointsPerSync + m_pointsPerBP + m_pointsPerImgLine)
        {
            int pointIndex = m_horizontalCount - (m_pointsPerSync + m_pointsPerBP);
            int iLine = m_lineCount % m_nbLines2;
            int oddity = m_lineCount < m_nbLines2 ? 0 : 1;
            int iLineImage = iLine - m_nbSyncLinesHead - m_nbBlankLines;

            switch(m_running.m_atvModInput)
            {
            case ATVModInputHBars:
                sample = (pointIndex / m_pointsPerHBar) * m_hBarIncrement + m_blackLevel;
                break;
            case ATVModInputVBars:
                sample = (iLine / m_linesPerVBar) * m_vBarIncrement + m_blackLevel;
                break;
            case ATVModInputChessboard:
                sample = (((iLine / m_linesPerVBar)*5 + (pointIndex / m_pointsPerHBar)) % 2) * m_spanLevel * m_running.m_uniformLevel + m_blackLevel;
                break;
            case ATVModInputHGradient:
                sample = (pointIndex / (float) m_pointsPerImgLine) * m_spanLevel + m_blackLevel;
                break;
            case ATVModInputVGradient:
                sample = ((iLine -5) / (float) m_nbImageLines2) * m_spanLevel + m_blackLevel;
                break;
            case ATVModInputImage:
                if (!m_imageOK || (iLineImage < -oddity) || m_image.empty())
                {
                    sample = m_spanLevel * m_running.m_uniformLevel + m_blackLevel;
                }
                else
                {
                	unsigned char pixv;

                	if (m_interlaced) {
                        pixv = m_image.at<unsigned char>(2*iLineImage + oddity, pointIndex); // row (y), col (x)
                	} else {
                        pixv = m_image.at<unsigned char>(iLineImage, pointIndex); // row (y), col (x)
                	}

                    sample = (pixv / 256.0f) * m_spanLevel + m_blackLevel;
                }
                break;
            case ATVModInputVideo:
                if (!m_videoOK || (iLineImage < -oddity) || m_videoFrame.empty())
                {
                    sample = m_spanLevel * m_running.m_uniformLevel + m_blackLevel;
                }
                else
                {
                	unsigned char pixv;

                	if (m_interlaced) {
                        pixv = m_videoFrame.at<unsigned char>(2*iLineImage + oddity, pointIndex); // row (y), col (x)
                	} else {
                        pixv = m_videoFrame.at<unsigned char>(iLineImage, pointIndex); // row (y), col (x)
                	}

                    sample = (pixv / 256.0f) * m_spanLevel + m_blackLevel;
                }
            	break;
            case ATVModInputCamera:
                if ((iLineImage < -oddity) || (m_cameraIndex < 0))
                {
                    sample = m_spanLevel * m_running.m_uniformLevel + m_blackLevel;
                }
                else
                {
                    ATVCamera& camera = m_cameras[m_cameraIndex];

                    if (camera.m_videoFrame.empty())
                    {
                        sample = m_spanLevel * m_running.m_uniformLevel + m_blackLevel;
                    }
                    else
                    {
                        unsigned char pixv;

                        if (m_interlaced) {
                            pixv = camera.m_videoFrame.at<unsigned char>(2*iLineImage + oddity, pointIndex); // row (y), col (x)
                        } else {
                            pixv = camera.m_videoFrame.at<unsigned char>(iLineImage, pointIndex); // row (y), col (x)
                        }

                        sample = (pixv / 256.0f) * m_spanLevel + m_blackLevel;
                    }
                }
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

    inline void pullVSyncLine(Real& sample)
    {
        int fieldLine = m_lineCount % m_nbLines2;

        if (m_lineCount < m_nbLines2) // even
        {
            if (fieldLine < 2) // 0,1: Whole line "long" pulses
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
            else if (fieldLine == 2) // long pulse then equalizing pulse
            {
                if (m_horizontalCount < (m_nbHorizPoints/2) - m_pointsPerSync)
                {
                    sample = 0.0f; // ultra-black
                }
                else if (m_horizontalCount < (m_nbHorizPoints/2))
                {
                    sample = m_blackLevel; // black
                }
                else if (m_horizontalCount < (m_nbHorizPoints/2) + m_pointsPerFSync)
                {
                    sample = 0.0f; // ultra-black
                }
                else
                {
                    sample = m_blackLevel; // black
                }
            }
            else if ((fieldLine < 5) || (fieldLine > m_nbLines2 - 3)) // Whole line equalizing pulses
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
            else // black images
            {
                if (m_horizontalCount < m_pointsPerSync)
                {
                    sample = 0.0f;
                }
                else
                {
                    sample = m_blackLevel;
                }
            }
        }
        else // odd
        {
            if (fieldLine < 1) // equalizing pulse then long pulse
            {
                if (m_horizontalCount < m_pointsPerFSync)
                {
                    sample = 0.0f; // ultra-black
                }
                else if (m_horizontalCount < (m_nbHorizPoints/2))
                {
                    sample = m_blackLevel; // black
                }
                else if (m_horizontalCount < m_nbHorizPoints - m_pointsPerSync)
                {
                    sample = 0.0f; // ultra-black
                }
                else
                {
                    sample = m_blackLevel; // black
                }
            }
            else if (fieldLine < 3) // Whole line "long" pulses
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
            else if ((fieldLine < 5) || (fieldLine > m_nbLines2 - 5)) // Whole line equalizing pulses
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
            else // black images
            {
                if (m_horizontalCount < m_pointsPerSync)
                {
                    sample = 0.0f;
                }
                else
                {
                    sample = m_blackLevel;
                }
            }
        }
    }
};


#endif /* PLUGINS_CHANNELTX_MODATV_ATVMOD_H_ */
