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

#ifndef INCLUDE_DVBDEMODSINK_H
#define INCLUDE_DVBDEMODSINK_H

#include <atomic>
#include <complex>
#include <deque>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <QElapsedTimer>
#include <QMutex>

#include "dsp/channelsamplesink.h"
#include "audio/audiofifo.h"
#include "modemdvb/dvbssharedfrontend.h"
#include "modemdvb/dvbtreceiver.h"
#include "modemdvb/dvbt2receiver.h"
#include "modemdvb/dvbsreceiver.h"
#include "modemdvb/dvbssymbolrate.h"
#include "modemdvb/dvbs2receiver.h"

#include "dvbdemodepg.h"
#include "dvbdemodrecorder.h"
#include "dvbdemodsettings.h"
#include "dvbdemodtsfifo.h"
#include "dvbdemodudp.h"

class DVBDemodSink : public ChannelSampleSink {
public:
    struct Status
    {
        dvbt::Receiver::Status m_receiver;
        dvbt2::Receiver::Status m_t2;
        dvbs::Receiver::Status m_s;
        dvbs2::Receiver::Status m_s2;
        //! Being received (DVBDemodSettings::Standard): StandardAutoTerrestrial none yet (searching for DVB-T and T2),
        //! StandardDVBT, StandardDVBT2 (either profile), StandardAutoSatellite none yet (searching for DVB-S and S2),
        //! StandardDVBS, StandardDVBS2
        int m_standard;
        int m_channelSampleRate;
        bool m_sampleRateOK;        //!< Channel sample rate is close enough to what the bandwidth (or symbol rate) needs
        double m_requiredRate;      //!< The least channel sample rate that will do
        double m_autoSymbolRate;    //!< DVB-S and S2 with the symbol rate automatic: as found, 0 while searching
        //! DVB-S and S2 with the symbol rate set, nothing locked for a while: the signal's, measured, if it's another
        //! (as after a retune to a transponder at another rate), else 0
        double m_signalSymbolRate = 0.0;
        //! The signals in the channel's spectrum the symbol rate search saw last (for a transponder scan), and how many
        //! searches have ended (each 2^18 samples, while searching)
        std::vector<dvbs::SymbolRateSearch::Signal> m_signals;
        std::vector<float> m_spectrum;  //!< That search's power spectrum (see SymbolRateSearch::Result)
        double m_spectrumBinHz = 0.0;
        double m_minSignalWidth = 0.0;
        uint64_t m_rateSearches = 0;    //!< Since the band was last set
        uint64_t m_bandGeneration = 0;  //!< Of the band last set (by setRateSearchBand())
        double m_power;             //!< dB
        double m_tsBitRate;         //!< bit/s
        uint64_t m_udpDatagrams;
        uint64_t m_fifoOverflows;   //!< Bytes the video player didn't take
        std::vector<dvbt::Cf> m_cells;      //!< Equalised data cells, for display
        std::vector<float> m_responseDb;    //!< Channel frequency response, for display
        // Channel impulse response, for display: dB from the strongest path against delay from the FFT window's
        // start, m_impulseResolution samples a bin, wrapping round; and the guard interval, samples
        std::vector<float> m_impulseDb;
        double m_impulseResolution;
        int m_impulseGuard;

        Status();
        QString stateText(int bandwidth = 0) const; //!< For display
        //! A generic stream's received (not a transport stream: nothing to decode or play)
        static bool isGeneric(const dvbt2::BBFrameDecoder::Stats& bb)
        {
            return (bb.m_genericFrames > 0) && (bb.m_streamType >= 0) && (bb.m_streamType != 3);
        }
        bool isSatellite() const { return DVBDemodSettings::isSatellite(m_standard); }
        bool isLocked() const;      //!< Receiving a transport stream
        bool isSearching() const;   //!< Nothing found to synchronise to (yet)
    };

    DVBDemodSink();
    ~DVBDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force = false);
    //! Acquire again, as after a retune, and in Auto search for both standards (and the symbol rate, if automatic)
    void resetReceivers();
    //! DVB-S or S2: samples are dropped in one go when behind (samplesToDrop())
    bool dropsWhenBehind() const { return DVBDemodSettings::isSatellite(m_settings.m_standard); }
    //! A satellite receiver locked (DVB-S2's decoding a transport stream), as last seen
    bool satelliteLocked() const { return m_satelliteLocked; }
    //! Samples of a FIFO of size, fill in it, to drop before processing: when behind (over half full) while searching
    //! (which costs more than tracking), down to a quarter full, so what's searched is whole. Not locked: tracking
    //! catches up after a stall, and a stall too long overflows the FIFO for less than dropping would. Too slow to
    //! track, the lock's lost, and then it drops. 0 if none
    static unsigned int samplesToDrop(bool drops, bool locked, unsigned int fill, unsigned int size);
    //! Samples were dropped before the next fed: the satellite receivers start their search again from them
    void discontinuity();
    //! The symbol rate searched for again, of the signal in this band only (Hz from the channel's centre; a width of 0
    //! for the whole channel), as a transponder scan tries each signal it found
    //! spectrumOnly: the search gives the spectrum and its signals, no rate, so the receivers aren't run (a scan's sweep)
    //! generation: the caller's, for the band, given back in Status::m_bandGeneration once it's applied
    void setRateSearchBand(double frequency, double width, bool spectrumOnly, uint64_t generation);
    //! Where the device's centre, and so its LO leakage, is in the channel, if it is
    void configureDCRemoval();
    void getStatus(Status& status);
    DVBDemodTSFifo *getTSFifo() { return &m_tsFifo; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
    DVBDemodEPG *getEPG() { return &m_epg; }

    struct Recording
    {
        int m_id;
        DVBDemodRecorder::Status m_status;
    };

    //! Records the multiplex, or a service, to a .ts file. Returns its id, or -1 with the error
    int startRecording(const QString& fileName, DVBDemodRecorder::Mode mode, int serviceId, QString& error);
    //! What's queued is written, and the file closed, in a thread of its own (up to 64 MB to a slow disk would hold
    //! up the caller): this returns at once. Those still finishing are waited for when this is deleted
    void stopRecording(int id);
    std::vector<Recording> getRecordings();

private:
    // Channel sample rates the receiver resamples from, relative to 8 * bandwidth / 7. The lowest still holds the
    // occupied bandwidth (7.61 MHz for 8 MHz channels). The channelizer decimates by powers of 2, so its rate is
    // at most twice what's asked for
    static constexpr double m_minRateRatio = 0.875;
    static constexpr double m_maxRateRatio = 2.2;
    static constexpr double m_minSymbolRateRatio = 1.2;  //!< Channel sample rate to symbol rate: the signal's 1 + roll-off, less what's lost of its edges
    static constexpr double m_maxSymbolRateRatio = 64.0; //!< Its taps, at 10 symbols, get too many
    static constexpr double m_autoRelockSeconds = 10.0;   //!< With the symbol rate automatic, unlocked, before it's searched for again
    static constexpr double m_retuneSettleSeconds = 0.3;  //!< After a reset (a retune), the samples before it still come (queued in the device and DSP engine) for a while
    static constexpr double m_autoCheckSeconds = 2.0;     //!< With the symbol rate automatic, unlocked, before it's measured again (and then as often)
    static constexpr double m_maxRelockStep = 0.5;        //!< Of wall time between blocks counted towards the relock (not a stopped device's)
    static constexpr double m_setRateCheckSeconds = 5.0;  //!< With the symbol rate set, unlocked, before the signal's is measured
    static constexpr size_t m_packetBlockSize = 188 * 512; //!< Packets delivered are flushed at least this often
    static constexpr int m_tsFifoSize = 8 * 1024 * 1024;  //!< The video player's

    //! The receivers for the standard: DVB-T and T2 (for the bandwidth) or DVB-S and S2, the others deleted (as each
    //! has threads). Those already made for it are kept
    void createReceiver();
    void createSatelliteReceivers();
    void setEqualiser(bool enabled);
    //! The status as of receivers just reset, for the standard set: so the last of one gone doesn't stay, when nothing
    //! more's processed (as when the channel's rate is too low for the new one)
    void clearStatus();
    void processSatellite();
    void processBuffer();
    void updateRates();
    void processPacket(const uint8_t *packet);
    //! The packets delivered by the receivers, to the recorders, UDP and the video player's FIFO: a block at a time,
    //! so each's lock is taken once a block, not each packet
    void flushPackets();

    DVBDemodSettings m_settings;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    bool m_sampleRateOK;

    dvbt::Receiver *m_receiver;     //!< Made when a terrestrial standard is chosen
    dvbt2::Receiver *m_receiverT2;
    int m_receiverBandwidth = 0;    //!< Theirs
    dvbs::Receiver *m_receiverS;    //!< Made when a satellite standard is chosen
    dvbs2::Receiver *m_receiverS2;
    //! The satellite receivers' one front end, which runs it and them (in Auto both, while searching)
    std::unique_ptr<dvbs::SharedFrontEnd> m_sharedFrontEnd;
    void resetSatellite();
    void configureSatellite(double channelSampleRate, double symbolRate);
    int m_active;                   //!< Standard found, when automatic: 0 none, else StandardDVBT, T2, S or S2
    uint64_t m_unlockedSamples;     //!< Since the standard found was last locked
    dvbs::SymbolRateSearch m_rateSearch;    //!< With the symbol rate automatic
    double m_autoRate;              //!< The symbol rate found, when automatic, 0 while searching
    std::vector<dvbs::SymbolRateSearch::Signal> m_signals;  //!< From the last search
    std::vector<float> m_spectrum;
    double m_spectrumBinHz;
    double m_minSignalWidth;
    uint64_t m_rateSearches;
    uint64_t m_bandGeneration;
    uint64_t m_autoUnlockedSamples; //!< Since a receiver was locked at it
    double m_autoUnlockedSeconds = 0.0;     //!< The same in wall time
    QElapsedTimer m_relockClock;
    uint64_t m_autoCheckSamples = 0;        //!< Towards measuring the rate again (m_autoCheckSeconds)
    double m_autoCheckRate = 0.0;           //!< Another rate measured so, to be confirmed by the next measurement
    void restartAutoRelock() { m_autoUnlockedSamples = 0; m_autoUnlockedSeconds = 0.0; m_autoCheckSamples = 0; m_autoCheckRate = 0.0; }
    uint64_t m_fixedUnlockedSamples = 0;    //!< With the symbol rate set, since a receiver was locked

    double m_signalSymbolRate = 0.0;        //!< Status::m_signalSymbolRate
    uint64_t m_s2GoodFrames;        //!< The DVB-S2 receiver's frames decoded without error, when last seen
    QElapsedTimer m_s2LogClock;     //!< logS2Search()'s, while unlocked
    dvbs2::Receiver::Status m_s2LogLast;
    void logS2Search(const dvbs2::Receiver::Status& status, bool locked);
    uint64_t m_sGoodPackets;        //!< The DVB-S receiver's packets through Reed-Solomon, when last seen
    std::atomic<bool> m_satelliteLocked{false};
    // Moves the channel to 0 Hz, if it isn't there: in float, restarting from the exact phase each block
    bool m_ncoShift;
    dvbt::Cf m_ncoRot;
    dvbt::Cf m_ncoStep;
    double m_ncoPhase;          //!< At the start of the block, radians
    double m_ncoStepAngle;      //!< Radians a sample
    int m_ncoCount;
    // The device's LO leakage, a tone at its centre that zero-IF devices leave on the carriers there: tracked by a
    // running mean in a frame turning with it, as it's at -inputFrequencyOffset in the channel, and subtracted
    bool m_dcRemoval;
    std::complex<double> m_dcMean;
    dvbt::Cf m_dcRot;
    dvbt::Cf m_dcStep;
    double m_dcPhase;
    double m_dcStepAngle;
    int m_dcCount;
    std::atomic<bool> m_fftWarmUpStop;
    std::vector<dvbt::Cf> m_buffer;
    int m_bufferFill;

    double m_powerSum;
    uint64_t m_powerCount;
    double m_power;
    uint64_t m_displaySamples;
    std::vector<dvbt::Cf> m_cells;
    std::vector<float> m_responseDb;
    std::vector<float> m_impulseDb;
    double m_impulseResolution = 0.0;
    int m_impulseGuard = 0;
    uint64_t m_rateSamples;
    uint64_t m_ratePackets;
    //! Samples and packets of each of the last seconds, for the TS rate: DVB-T2 delivers packets an interleaving frame
    //! at a time, so over one second the rate would be that of 4 or 5 frames
    std::deque<std::pair<uint64_t, uint64_t>> m_rateHistory;
    double m_tsBitRate;

    DVBDemodUDP m_udp;
    DVBDemodTSFifo m_tsFifo;
    DVBDemodEPG m_epg;          //!< Fed whether or not video is enabled, so it's also there in server builds
    QMutex m_recordersMutex;    //!< For the following, as recordings are started and stopped from other threads
    std::vector<std::pair<int, std::unique_ptr<DVBDemodRecorder>>> m_recorders;
    int m_nextRecordingId;
    std::vector<std::future<void>> m_stoppingRecorders;   //!< Stopped, finishing their files
    std::vector<uint8_t> m_packetBlock;     //!< Packets delivered, not flushed yet
    AudioFifo m_audioFifo;
    std::thread m_fftWarmUp;

    QMutex m_statusMutex;
    Status m_status;

    static const int m_bufferSize = 16384;
};

#endif // INCLUDE_DVBDEMODSINK_H
