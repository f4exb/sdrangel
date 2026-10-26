///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DTVVIDEOPLAYER_H
#define INCLUDE_DTVVIDEOPLAYER_H

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <initializer_list>
#include <map>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include <QList>
#include <QMutex>
#include <QString>
#include <QThread>

struct AVBufferRef;
struct AVBufferPool;
struct AVCodec;
struct AVFormatContext;
struct AVStream;
struct AVIOContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct SwrContext;
struct AVFilterGraph;
struct AVFilterContext;
struct AVHWFramesContext;
struct AVSubtitle;
struct AVFrameSideData;

#include "gui/glvideoframe.h"

#include "dtv/dtvpsi.h"
#include "export.h"

class AudioFifo;
class DTVTSFifo;

// Demultiplexes the transport stream with FFmpeg, decodes the selected service and displays it. The player's
// thread demultiplexes and decodes the audio. Video is decoded and shown in another thread, as frames wait there
// until they are due, and broadcasters send video well ahead of the audio it goes with. Frames are given to the
// display as GLVideoFrames: decoded by the GPU, they stay there (deinterlaced by FFmpeg's GPU filters, or the
// display's for D3D11) if the display can show them, else are copied to memory (after FFmpeg's GPU filter has
// deinterlaced them, if there is one for them, sparing the CPU bwdif)
class SDRGUI_API DTVVideoPlayer : public QThread
{
    Q_OBJECT

public:
    struct Service
    {
        int m_id;           //!< MPEG program number
        QString m_name;     //!< From the SDT
        QString m_provider;
        bool m_hasVideo;
        int m_channel;      //!< Logical channel number, from the NIT, or -1
    };

    struct Info
    {
        int m_serviceId;
        QString m_videoCodec;       //!< FFmpeg's long name
        QString m_videoDecoder;     //!< The GPU's (e.g. CUDA, D3D11VA), or empty if decoded in software
        QString m_audioCodec;
        int m_width = 0;
        int m_height = 0;
        uint64_t m_videoFrames;
        uint64_t m_audioFrames = 0;
        uint64_t m_decodeErrors;
        uint64_t m_lateFrames;      //!< Not shown, as they were too late
        bool m_audioDescription = false; //!< Audio description is being mixed in
        uint64_t m_adFrames;        //!< Audio description frames decoded
        int m_adFade;               //!< Latest fade of the main audio, from the audio description (0 to 255)
        bool m_subtitles;           //!< A subtitle stream is being decoded
        uint64_t m_subtitlePages;   //!< Subtitle pages decoded (including those clearing the screen)
        uint64_t m_audioConcealed;  //!< Audio frames muted, as from packets with uncorrected errors
        uint64_t m_audioGaps;       //!< Gaps in the audio (frames missing) filled with silence
        uint64_t m_videoDropped = 0; //!< Packets with uncorrected errors not given to the GPU's decoder
        uint64_t m_audioUnderruns = 0; //!< Times the audio FIFO ran dry while playing: the device played silence
        uint64_t m_audioOverflows = 0; //!< Times the audio FIFO was too full, and its oldest audio dropped
        uint64_t m_clockRestarts = 0; //!< Times the video lost sync with the audio (or its timestamps jumped)
        uint64_t m_softwareGOPs = 0; //!< GOPs with damage decoded in software, rather than by the GPU
        bool m_scrambled = false;   //!< The service's video (or audio, if it has none) is scrambled: encrypted, so can't be played
        double m_avSync = std::nan(""); //!< Seconds the video is shown after its audio (negative: before), averaged; NaN if not known
        double m_maxFrameInterval = 0.0; //!< Seconds, the longest between frames shown, in the last 5 to 10 s
    };

    //! An audio or subtitle stream of the service
    struct Track
    {
        enum Type
        {
            Main,                   //!< Audio
            AudioDescription,       //!< Audio description to mix with the main audio (receiver mix)
            AudioDescriptionMixed,  //!< Audio with description already mixed in (broadcast mix)
            CleanAudio,             //!< Audio for the hard of hearing
            Subtitles,
            SubtitlesHardOfHearing
        };

        int m_pid;                  //!< m_captionPid for closed captions in the video (ATSC's)
        QString m_language;         //!< ISO 639-2 code, e.g. "eng", or empty
        Type m_type;
    };

    //! The service's tracks, and those being played (PID, or -1 for none)
    struct Tracks
    {
        QList<Track> m_audio;
        QList<Track> m_subtitles;
        int m_audioPid = -1;
        int m_adPid = -1;
        int m_subtitlePid = -1;
    };

    //! A frame to show, when it's due
    typedef std::function<void(GLVideoFramePtr frame)> FrameCallback;
    //! If the display can show GPU frames of a type (GLVideoFrame::Type)
    typedef std::function<bool(int type)> CanShowCallback;

    //! audioFifo may be null, for no audio
    DTVVideoPlayer(DTVTSFifo *tsFifo, AudioFifo *audioFifo, QObject *parent = nullptr);
    ~DTVVideoPlayer();

    void stopPlayer();
    void setService(int id) { m_requestedService = id; }
    void setAudioMute(bool mute) { m_audioMute = mute; }
    void setAudioVolume(int volume); //!< 0..100
    void setDeinterlace(bool deinterlace) { m_deinterlace = deinterlace; }
    //! This build can deinterlace (FFmpeg's libavfilter was found)
    static bool canDeinterlace();
    //! Decode video with the GPU, if FFmpeg can for the codec on this platform. Takes effect straight away
    void setHardwareDecoding(bool hardware);
    void setAVOffset(int ms) { m_avOffset = ms / 1000.0; } //!< Delay of the picture relative to the sound
    //! The audio device's sample rate, and how long it buffers audio before it's heard
    void setAudioOutput(int sampleRate, double latency);
    //! Which tracks to play: audio of the type (Main, CleanAudio, AudioDescription mixed here, or AudioDescriptionMixed
    //! by the broadcaster) in the language (or the first, if empty or not found), falling back to the main audio (and
    //! for a description, to the other kind); and subtitles of the type (Subtitles or SubtitlesHardOfHearing, the other
    //! if not there) in the language if subtitles. Takes effect straight away
    void setTrackPreferences(const QString& audioLanguage, Track::Type audioType, bool subtitles, const QString& subtitleLanguage,
        Track::Type subtitleType);
    //! The tracks those preferences choose of a service's: indexes in tracks.m_audio (audio, and a description to mix
    //! in) and tracks.m_subtitles, -1 for none
    static void chooseTracks(const Tracks& tracks, const QString& audioLanguage, Track::Type audioType, bool subtitles,
        const QString& subtitleLanguage, Track::Type subtitleType, int& audio, int& description, int& subtitle);
    void setFrameCallback(FrameCallback callback) { m_frameCallback = callback; } //!< Called from the video thread
    void setCanShowCallback(CanShowCallback callback) { m_canShowCallback = callback; } //!< Called from the video thread
    Info getInfo();

signals:
    void servicesChanged(const QList<DTVVideoPlayer::Service>& services);
    void serviceSelected(int id, bool hasVideo, bool hasAudio); //!< Whether the service has streams that can be decoded
    void tracksChanged(const DTVVideoPlayer::Tracks& tracks);

protected:
    void run() override;

private:
    static constexpr double m_maxAudioGap = 0.02; //!< Seconds of audio missing, above which silence is inserted
    static constexpr int m_ioBufferSize = 188 * 348;
    static constexpr double m_clockJump = 0.5;    //!< Seconds between the video's clock and the audio's, beyond which it jumps to it
    static constexpr double m_audioClockStale = 1.0; //!< Seconds since audio was written (or a description decoded), after which it's stale
    static constexpr double m_maxVideoLead = 2.0; //!< Seconds a frame may wait without the audio clock, beyond which the timestamps jumped
    static constexpr double m_maxVideoLag = 1.0;  //!< Seconds a frame may be late, beyond which the clock restarts
    static constexpr double m_servicesInterval = 1.0; //!< Seconds between checks of the services, and the multiplex
    static constexpr int m_maxAwaitPackets = 100; //!< About 4 s, of a GPU's decoder waiting for a keyframe: in case they aren't marked
    static constexpr size_t m_maxGOPBytes = 32 * 1024 * 1024; //!< Kept for replaying a GOP into the software decoder
    static constexpr double m_replayWindow = 10.0; //!< Seconds before the last frame shown, within which replayed frames aren't shown again
    static constexpr double m_adControlWindow = 10.0; //!< Seconds ahead of the audio, beyond which a description's control was a jump
    static constexpr double m_subtitleWindow = 30.0; //!< Seconds ahead or behind the video, beyond which subtitles' timestamps jumped
    static constexpr double m_lateScoreDecay = 0.98; //!< Of the late frame score, each frame: about the last 50 frames
    static constexpr double m_shedLateScore = 3.0; //!< Late frames lately, from which the decoders skip pictures nothing refers to
    static constexpr int m_restoreOnTimeFrames = 250; //!< Frames on time (some seconds), after which they decode them all again

    static std::chrono::steady_clock::duration seconds(double s);
    static int64_t pesPTS(const uint8_t *h);
    static bool continuityLost(const uint8_t *p, bool discontinuity, int& lastCC, int& errorPackets);
    static bool signalledDiscontinuity(const uint8_t *p);
    static AVCodecContext *openDecoder(AVStream *stream);
    static const AVHWFramesContext *framesContext(const AVFrame *frame);
    static bool hasGPUDeinterlacer(int format);
    static SwsContext *createConverter(int width, int height, int format);
    static SwrContext *createResampler(const AVFrame *frame, int rate, bool mono);
    static GLVideoSubtitlePtr makeSubtitle(const AVSubtitle& sub, int canvasWidth, int canvasHeight);
    //! Of a Track of closed captions, which aren't in a stream of their own
    static constexpr int m_captionPid = 0x10000;
    static bool getTrack(const AVStream *stream, Track& track);
    static int findTrack(const QList<Track>& tracks, std::initializer_list<Track::Type> types, const QString& language);
    static int findTrackInLanguage(const QList<Track>& tracks, Track::Type type, const QString& language);

    static int readPacket(void *opaque, uint8_t *buf, int size);
    bool open();
    void close();
    QList<Service> getServices() const;
    void updateServices();
    //! reselect: to change how it's decoded, so done even if the service asked for isn't here
    void selectService(bool reselect = false);
    void closeDecoders();
    void selectTracks();
    void closeTrackDecoders();
    //! A frame's closed captions (A/53, CEA-608), decoded into a page shown from its timestamp. In the video thread
    void decodeCaptions(const AVFrameSideData *captions, int64_t timestamp);
    void closeCaptions();
    //! Caption text (lines apart by newlines) as a page, white on black boxes at the bottom of the picture
    static GLVideoSubtitlePtr makeCaptions(const QString& text);
    //! The text of an ASS event, as FFmpeg's text subtitle decoders give it: its fields and override codes removed
    static QString assText(const char *ass);
    //! Captions were found in the video: listed as a track, and played if they're preferred
    void addCaptionTrack();
    void decodeAudioDescription();
    void mixAudioDescription(int16_t *samples, int count, double pts, float gain, bool mute);
    void parseADDescriptors(const uint8_t *buf, int size);
    void trackAudioPES(const uint8_t *buf, int size);
    void trackScrambling(const uint8_t *buf, int size);
    void unwrapTimestamps(AVPacket *packet);
    bool audioFrameDamaged(const AVPacket *packet, bool flagged);
    void trackVideoPES(const uint8_t *buf, int size);
    bool videoFrameDamaged(const AVPacket *packet);
    //! The TS read, whole packets, a read split mid packet carried over (FFmpeg can ask for sizes not a multiple)
    void parseTS(const uint8_t *buf, int size);
    //! Audio into the FIFO, the oldest dropped to make room past 3/4 full, so bursts fit: samples written
    uint32_t writeAudio(const int16_t *samples, int nbSamples);
    void decodeSubtitle();
    GLVideoSubtitlePtr currentSubtitle(double pts);
    bool readAndDecode();
    void queueVideoPacket();
    void startVideoThread();
    void stopVideoThread();
    void videoThread();
    void decodeVideo(AVPacket *packet);
    void receiveVideoFrames(AVCodecContext *ctx);
    void startSoftwareGOP();
    void startSoftwareFallback();
    void replayGOP();
    void decodeVideoSoftware(AVPacket *packet);
    void endSoftwareGOP();
    void clearGOP();
    void videoDiscontinuity();
    void setVideoDiscard(int discard);
    void updateShedding(bool late);
    void flushDeinterlacer();
    bool transferFromGPU(AVFrame *frame);
    void decodeAudio();
    void setupResampler();
    void renderFrame(AVFrame *frame);
    GLVideoFramePtr makeFrame(AVFrame *frame);
    int gpuFrameType(const AVFrame *frame) const;
    bool setupDeinterlacer(const AVFrame *frame);
    AVCodecContext *openVideoDecoder(AVStream *stream);
    bool setupHardwareDecoding(const AVCodec *codec, AVCodecContext *ctx);
    //! late: the frame was too late to show, or the clock restarted as it was
    bool waitForPresentation(const AVFrame *frame, bool& late);
    bool getAudioClock(double& clock);
    void closeDeinterlacer();

    DTVTSFifo *m_tsFifo;
    FrameCallback m_frameCallback;
    CanShowCallback m_canShowCallback;
    AudioFifo *m_audioFifo;
    std::atomic<bool> m_stop;
    std::atomic<int> m_requestedService;
    std::atomic<bool> m_audioMute;
    std::atomic<float> m_audioGain;
    std::atomic<bool> m_deinterlace;
    std::atomic<bool> m_hardwareDecoding;
    std::atomic<bool> m_reselect;           //!< Select the service again, to change how it's decoded
    int m_selectedRequest;          //!< m_requestedService when the current service was selected
    QList<Service> m_services;      //!< As last signalled
    std::chrono::steady_clock::time_point m_servicesChecked;
    DTVPSI m_psi;
    int m_transportStreamId = -1;   //!< Of the stream opened, to see when the multiplex changes, -1 until known

    AVFormatContext *m_formatCtx;
    AVIOContext *m_ioCtx;
    AVCodecContext *m_videoCtx;
    AVCodecContext *m_audioCtx;
    AVCodecContext *m_adCtx;        //!< Audio description, mixed with the main audio
    AVCodecContext *m_subtitleCtx;
    AVFrame *m_frame;               //!< Audio, in the player's thread
    AVFrame *m_videoFrame;          //!< In the video thread
    AVFrame *m_transferFrame;       //!< From the GPU, in the video thread
    AVBufferRef *m_hwDeviceCtx;     //!< For hardware decoding, kept between services, so the display keeps its interop
    int m_hwDeviceType;             //!< AVHWDeviceType of m_hwDeviceCtx
    int m_hwPixelFormat;            //!< Of hardware decoded frames, AV_PIX_FMT_NONE if not decoding with the GPU
    QString m_hwName;
    //! In the video thread: 1 if the last frame was decoded by the GPU, 0 if in software (FFmpeg falls back to it for
    //! profiles the GPU can't decode), -1 before the first frame
    int m_videoGPU;
    //! In the video thread: after damage, a GPU decoder is given nothing until a keyframe, counting those skipped
    bool m_videoAwaitKey;
    int m_videoAwaitPackets;
    //! With a GPU decoder: FFmpeg's software decoder for the GOPs with damage, the packets since the last keyframe (to
    //! replay into it), and whether it's decoding. In the video thread, but for opening it
    AVCodecContext *m_videoSoftwareCtx = nullptr;
    std::vector<AVPacket*> m_gopPackets;
    size_t m_gopBytes = 0;
    bool m_gopValid = false;        //!< m_gopPackets start at a keyframe
    bool m_videoSoftware = false;
    double m_lastVideoPts = -1e300; //!< Seconds, of the last frame decoded
    double m_replayShownPts = -1e300; //!< After a replay, frames to here were shown, so aren't again (-1e300 for none)
    double m_handoverPts = -1e300;  //!< Back on the GPU from this keyframe: software's frames before it, the GPU's after
    //! The GPU's decoder fell back to software (for a profile the GPU can't decode), single threaded: m_videoSoftwareCtx,
    //! multithreaded, decodes instead, for the rest of the service
    bool m_videoFallback = false;
    // Shedding work when frames are consistently late: the decoders skip pictures nothing refers to (AVDiscard) until
    // they're on time again. A score of the frames late lately, and the frames on time since the last late
    int m_videoDiscard = 0;         //!< AVDISCARD_DEFAULT
    double m_lateScore = 0.0;
    int m_onTimeFrames = 0;
    AVPacket *m_packet;
    int m_videoStream;
    // Of the video stream, copied when it's selected, as the video thread can't read m_formatCtx->streams, which
    // the demuxer reallocates when it adds a stream
    int m_videoTimeBaseNum;
    int m_videoTimeBaseDen;
    int m_videoSARNum;
    int m_videoSARDen;
    int m_audioStream;
    int m_adStream;
    int m_subtitleStream;
    int m_programIndex;             //!< In m_formatCtx->programs of the service selected, -1 if none

    // Track preferences, set from other threads
    QMutex m_tracksMutex;
    QString m_audioLanguage;
    Track::Type m_audioType;
    bool m_subtitles;
    QString m_subtitleLanguage;
    Track::Type m_subtitleType;
    std::atomic<bool> m_tracksChanged;

    // Audio description, mixed with the main audio by timestamp: decoded and resampled to mono at the output rate.
    // The main audio is faded and the description panned as the broadcaster says, in the PES headers of MPEG audio
    // descriptions (ETSI TS 101 154 annex E), which are read from the TS before FFmpeg demultiplexes it
    struct ADControl
    {
        double m_pts;               //!< Seconds
        int m_fade;                 //!< 0 none to 255 silent
        int m_pan;                  //!< 0 centre, in 1/256ths of a circle, clockwise
    };
    // What a resampler was set up for. A frame that differs, as one decoded from damaged data can (mono, in a stereo
    // service), would have it read channel planes that aren't there
    struct ResamplerInput
    {
        int m_format = -1;
        int m_rate = 0;
        int m_channels = 0;
        uint64_t m_layout = 0;      //!< Mask, if known
        bool operator==(const ResamplerInput& other) const {
            return (m_format == other.m_format) && (m_rate == other.m_rate) && (m_channels == other.m_channels) && (m_layout == other.m_layout);
        }
    };
    //! The resampler for frame, made again if it's for another format, or nullptr if frame can't be resampled
    //! (damaged and of another format, or missing planes)
    SwrContext *resamplerFor(SwrContext *&swr, ResamplerInput& input, int rate, bool mono, bool damaged);
    SwrContext *m_adSwrCtx;
    ResamplerInput m_adSwrInput;
    int m_adSwrRate;
    std::deque<int16_t> m_adSamples;    //!< Waiting to be mixed (a deque, as they're taken from the front each frame)
    std::vector<int16_t> m_adBuffer;    //!< Resampled into, before they're added
    double m_adStartPts;            //!< Of m_adSamples[0], seconds
    std::chrono::steady_clock::time_point m_adTime;    //!< Of the last audio description decoded
    int m_adPid;                    //!< Being mixed, -1 for none
    //! By PID, as they're read before the description is selected, while FFmpeg reads ahead to find the streams
    std::map<int, std::deque<ADControl>> m_adControls;
    float m_fadeGain;               //!< Applied to the main audio at the end of the last block
    // Audio from packets the receiver couldn't correct (flagged corrupt by the demuxer) would click or squawk,
    // so it's muted, fading out and in. Missing audio is replaced by silence, so the audio stays in time
    float m_muteGain;               //!< 0 muted to 1, at the end of the last block
    double m_audioNextPts;          //!< Expected timestamp of the next audio frame, seconds, NaN if not known
    int m_audioFillAfterWrite;      //!< Samples in the FIFO after the last frame was written (at m_audioWriteTime)
    std::vector<int16_t> m_silence;
    // The demuxer flags a whole PES (several audio frames) as corrupt if one of its TS packets is. Which bytes
    // of each PES came in TS packets with errors, or were lost, is noted as the TS is read, so only the frames
    // they're in are muted. The frames of a PES come in order, so where each is in it is known from their sizes
    struct AudioPES
    {
        int64_t m_pts;              //!< 90 kHz, 33 bits, -1 if its header had errors
        int m_length;               //!< Payload bytes, -1 until the next PES starts
        std::vector<std::pair<int, int>> m_damaged;     //!< Payload bytes [first, last) with errors or lost
    };
    // Likewise for video, whether each PES had errors or lost packets: FFmpeg's parser, only knowing a picture has
    // ended when the next starts, flags the next picture corrupt rather than the damaged one
    struct VideoPES
    {
        int64_t m_pts;              //!< 90 kHz, 33 bits, -1 if its header had errors (or none), -2 once matched
        bool m_damaged;
        uint64_t m_sequence;
    };
    //! Each video PID's, from the stream's opening: the selected service's isn't known until FFmpeg's probed it, and
    //! the packets read meanwhile are returned after
    struct VideoTrack
    {
        int m_cc = -1;
        int m_errorPackets = 0;
        uint64_t m_sequence = 0;    //!< Of the next PES
        uint64_t m_matched = 0;     //!< After the last matched to a packet
        std::deque<VideoPES> m_pes;
    };
    int m_videoPid;                 //!< In the demultiplexing thread, -1 for none
    std::map<int, VideoTrack> m_videoTracks;
    std::vector<uint8_t> m_tsCarry; //!< Of a read split mid packet
    std::vector<uint8_t> m_tsAligned;
    std::vector<int> m_selectedStreams; //!< The selected programme's stream indexes, when selected
    //! And what each was then (type, codec, disposition, language, extradata): a PMT update can change a stream in place
    std::vector<std::string> m_selectedSignatures;
    std::vector<std::string> signaturesOf(const std::vector<int>& streams) const;
    int m_selectedProgram = -1;     //!< Its id
    int m_audioPid;                 //!< Of the main audio, -1 for none
    bool m_scrambled = false;       //!< The service's packets, as last seen
    //! While FFmpeg probes the streams (avformat_find_stream_info()), when it's to stop (once data's come), and if it
    //! was stopped
    bool m_probing = false;
    bool m_probeTimed = false;
    std::chrono::steady_clock::time_point m_probeDeadline;
    bool m_probeStopped = false;
    static constexpr double maxProbeSeconds = 4.0;
    std::deque<AudioPES> m_audioPES;    //!< Recent PES of it, the last being received
    int m_audioPESBytes;            //!< Payload bytes of the last so far
    int m_audioCC;                  //!< Last good continuity counter, -1 if none
    int m_audioErrorPackets;        //!< Packets with errors since it
    int64_t m_framePESPts;          //!< PES the last audio frame checked was in, 90 kHz, -1 if none
    int m_framePESOffset;           //!< Where in it the next frame starts
    int m_audioFrameSize;           //!< Of the last good audio frame, bytes
    bool m_audioDamagedLast;        //!< The last audio frame was damaged
    int m_audioPeak;                //!< Of the last good audio frame, as decoded
    ADControl m_adControl;          //!< In use

    // Subtitles, decoded in the player's thread and shown with the video frames they are due with
    struct SubtitlePage
    {
        double m_start;             //!< Seconds
        double m_end;
        GLVideoSubtitlePtr m_subtitle;  //!< Null to clear the screen
    };
    std::mutex m_subtitleMutex;
    std::deque<SubtitlePage> m_subtitlePages;
    SubtitlePage m_subtitleShown;   //!< In the video thread

    // Closed captions, in the video's user data (A/53: CEA-608 in CEA-708's cc_data), as ATSC sends them: listed as a
    // subtitle track once seen, decoded in the video thread (as FFmpeg gives them with the video frames)
    std::atomic<bool> m_captionsSeen{false};    //!< Found in the video of the service, since it was selected
    bool m_captionsListed = false;              //!< In m_tracks
    std::atomic<bool> m_captionsEnabled{false}; //!< Chosen as the subtitles
    AVCodecContext *m_captionCtx = nullptr;     //!< FFmpeg's CEA-608 decoder, in the video thread
    Tracks m_tracks;                            //!< As last signalled

    // Video packets waiting for the video thread
    std::thread m_videoThread;
    std::mutex m_videoMutex;
    std::condition_variable m_videoCondition;
    std::deque<AVPacket *> m_videoPackets;  //!< Null for a discontinuity: those before dropped, the next a keyframe
    size_t m_videoBytes;
    std::atomic<bool> m_videoStop;  //!< Waited on with m_videoMutex, read without in the frame timing loop
    static constexpr size_t m_maxVideoBytes = 32 * 1024 * 1024; //!< Many seconds, even of HD
    //! In the demultiplexing thread: the video thread fell that far behind, so its packets were dropped, and those
    //! after are until a keyframe
    bool m_videoResync = false;

    // Buffers for frames in memory the player makes (copied from the GPU, or converted), reused rather than allocated
    // for each frame: megabytes for HD, which the system would map and fault in afresh each time. In the video thread
    struct FramePool
    {
        AVBufferPool *m_pool = nullptr;
        int m_format = -1;
        int m_width = 0;
        int m_height = 0;
        int m_linesize[4] = {};
        size_t m_offset[4] = {};    //!< Of each plane in the buffer
    };
    //! A buffer for frame, of its format and size, from the pool (set up again if it's for another format or size)
    static bool getPooledBuffer(FramePool& pool, AVFrame *frame);
    static void freePool(FramePool& pool);
    FramePool m_transferPool;
    FramePool m_convertPool;

    // Frames in formats the display doesn't take, converted to YUV 4:2:0
    SwsContext *m_swsCtx;
    int m_swsWidth;
    int m_swsHeight;
    int m_swsFormat;
    int m_frameWidth;               //!< Of the last frame, to report a change of size
    int m_frameHeight;

    // Deinterlacer: buffer -> bwdif -> buffersink
    AVFilterGraph *m_filterGraph;
    AVFilterContext *m_bufferSrc;
    AVFilterContext *m_bufferSink;
    AVFrame *m_filtered;
    int m_filterWidth;
    int m_filterHeight;
    int m_filterFormat;
    const void *m_filterFrames;     //!< hw_frames_ctx data of the GPU frames the deinterlacer is for
    bool m_gpuDeinterlaceFailed;    //!< The GPU's couldn't be set up (as bwdif_cuda isn't built): bwdif in memory

    // Video is shown when the audio being played reaches its timestamp. Without audio, it is paced against
    // wall clock time, with some latency. The audio clock is set in the player's thread and read in the video thread
    std::mutex m_audioClockMutex;
    bool m_audioClockValid;
    double m_audioEndPts;   //!< Seconds, of the end of the last audio written
    int m_audioClockRate;   //!< Sample rate of the audio written
    std::chrono::steady_clock::time_point m_audioWriteTime;
    bool m_clockValid;
    int64_t m_timestampRef;         //!< 90 kHz, the last of the service's packets, its timestamps unwrapped from it
    bool m_timestampRefValid;
    double m_clockPts;      //!< Seconds
    std::chrono::steady_clock::time_point m_clockTime;
#ifdef ENABLE_PROFILER
    std::chrono::steady_clock::time_point m_profileLastFrame;
#endif
    // For the measurements: the last frame shown, the longest interval between frames in this 5 s window and the
    // last, the A/V sync averaged, and when audio was last written (to tell the FIFO running dry while playing)
    std::chrono::steady_clock::time_point m_lastShown;
    std::chrono::steady_clock::time_point m_intervalWindowStart;
    double m_intervalMax = 0.0;
    double m_intervalLastMax = 0.0;
    double m_avSyncAverage = std::nan("");
    std::chrono::steady_clock::time_point m_lastAudioWrite;
    static constexpr double m_latency = 0.3; //!< Seconds
    static constexpr double clockGain = 0.05; //!< Of the audio clock error, applied each frame
    static constexpr double avSyncGain = 0.1; //!< Of the A/V sync measured, averaged over about 10 frames
    static constexpr double maxLateness = 0.1; //!< Seconds, beyond which frames aren't shown
    //! Seconds a frame may wait for the audio clock: video is sent ahead of its audio, which is then queued in the
    //! FIFO and the device, and the picture may be delayed by the A/V offset. Beyond that, the timestamps jumped
    static constexpr double maxAudioLead = 10.0;

    SwrContext *m_swrCtx;
    ResamplerInput m_swrInput;
    int m_swrRate;                          //!< Output sample rate m_swrCtx is set up for
    std::vector<int16_t> m_audioBuffer;
    std::atomic<int> m_audioSampleRate;     //!< Of the audio device
    std::atomic<double> m_audioLatency;     //!< Seconds the audio device buffers
    std::atomic<double> m_avOffset;         //!< Seconds the picture is delayed relative to the sound

    QMutex m_infoMutex;
    Info m_info;
};

Q_DECLARE_METATYPE(DTVVideoPlayer::Service)
Q_DECLARE_METATYPE(DTVVideoPlayer::Tracks)

#endif // INCLUDE_DTVVIDEOPLAYER_H
