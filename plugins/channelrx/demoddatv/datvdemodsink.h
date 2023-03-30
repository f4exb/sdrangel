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

#ifndef INCLUDE_DATVDEMODSINK_H
#define INCLUDE_DATVDEMODSINK_H

#include <QRecursiveMutex>

//LeanSDR
#include "leansdr/framework.h"
#include "leansdr/generic.h"
#include "leansdr/dvb.h"
#include "leansdr/filtergen.h"

#include "leansdr/hdlc.h"
#include "leansdr/iess.h"

#include "datvconstellation.h"
#include "datvmeter.h"
#include "datvdvbs2constellation.h"
#include "datvvideoplayer.h"
#include "datvideostream.h"
#include "datvudpstream.h"
#include "datvideorender.h"
#include "datvdemodsettings.h"

#include "dsp/channelsamplesink.h"
#include "dsp/fftfilt.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"

class TVScreen;
class DATVideoRender;
class QLabel;

class DATVDemodSink : public ChannelSampleSink {
public:
    DATVDemodSink();
	~DATVDemodSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setTVScreen(TVScreen *tvScreen);
    void SetVideoRender(DATVideoRender *screen);
    DATVideostream *getVideoStream() { return m_videoStream; }
    DATVUDPStream *getUDPStream() { return &m_udpStream; }
    bool audioActive();
    bool audioDecodeOK();
    bool videoActive();
    bool videoDecodeOK();
    bool udpRunning();

    bool playVideo();
    void stopVideo();

    int GetSampleRate();
    double getMagSq() const { return m_objMagSqAverage; } //!< Beware this is scaled to 2^30
    int getModcodModulation() const { return m_modcodModulation; }
    int getModcodCodeRate() const { return m_modcodCodeRate; }
    bool isCstlnSetByModcod() const { return m_cstlnSetByModcod; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }

    float getMERAvg() const {
        return r_merMeter ? r_merMeter->m_avg : 0;
    }

    float getMERRMS() const {
        return r_merMeter ? r_merMeter->m_rms : 0;
    }

    float getMERPeak() const {
        return r_merMeter ? r_merMeter->m_peak : 0;
    }

    int getMERNbAvg() const {
        return r_merMeter ? r_merMeter->m_nbAvg : 1;
    }

    float getCNRAvg() const {
        return r_cnrMeter ? r_cnrMeter->m_avg : 0;
    }

    float getCNRRMS() const {
        return r_cnrMeter ? r_cnrMeter->m_rms : 0;
    }

    float getCNRPeak() const {
        return r_cnrMeter ? r_cnrMeter->m_peak : 0;
    }

    int getCNRNbAvg() const {
        return r_cnrMeter ? r_cnrMeter->m_nbAvg : 1;
    }

    void applySettings(const DATVDemodSettings& settings, bool force = false);
	void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);

private:
    struct config
    {
        DATVDemodSettings::dvb_version standard;
        DATVDemodSettings::dvb_sampler sampler;

        int buf_factor;      // Buffer sizing
        float Fs;            // Sampling frequency (Hz)
        float Fderot;        // Shift the signal (Hz). Note: Ftune is faster
        int anf;             // Number of auto notch filters
        bool cnr;            // Measure CNR
        unsigned int decim;  // Decimation, 0=auto
        float Fm;            // QPSK symbol rate (Hz)
        leansdr::cstln_lut<leansdr::eucl_ss, 256>::predef constellation;
        leansdr::code_rate fec;
        float Ftune;         // Bias frequency for the QPSK demodulator (Hz)
        bool allow_drift;
        bool fastlock;
        bool viterbi;
        bool hard_metric;
        bool resample;
        float resample_rej;  // Approx. filter rejection in dB
        int rrc_steps;       // Discrete steps between symbols, 0=auto
        float rrc_rej;       // Approx. RRC filter rejection in dB
        float rolloff;       // Roll-off 0..1
        bool hdlc;           // Expect HDLC frames instead of MPEG packets
        bool packetized;     // Output frames with 16-bit BE length
        float Finfo;         // Desired refresh rate on fd_info (Hz)

        config() :
            standard(DATVDemodSettings::DVB_S),
            sampler(DATVDemodSettings::SAMP_LINEAR),
            buf_factor(4),
            Fs(2.4e6),
            Fderot(0),
            anf(0),
            cnr(true),
            decim(0),
            Fm(2e6),
            constellation(leansdr::cstln_lut<leansdr::eucl_ss, 256>::QPSK),
            fec(leansdr::FEC12),
            Ftune(0),
            allow_drift(false),
            fastlock(true),
            viterbi(false),
            hard_metric(false),
            resample(false),
            resample_rej(10),
            rrc_steps(0),
            rrc_rej(10),
            rolloff(0.35),
            hdlc(false),
            packetized(false),
            Finfo(5)
        {
        }
    };

    inline int decimation(float Fin, float Fout) { int d = Fin / Fout; return std::max(d, 1); }

    void CleanUpDATVFramework();
    void ResetDATVFrameworkPointers();
    void InitDATVFramework();
    void InitDATVS2Framework();

    static int getLeanDVBCodeRateFromDATV(DATVDemodSettings::DATVCodeRate datvCodeRate);
    static int getLeanDVBModulationFromDATV(DATVDemodSettings::DATVModulation datvModulation);

    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }

    unsigned long m_lngExpectedReadIQ;
    long m_lngReadIQ;

    //************** LEANDBV Parameters **************

    unsigned long BUF_BASEBAND;
    unsigned long BUF_SYMBOLS;
    unsigned long BUF_BYTES;
    unsigned long BUF_MPEGBYTES;
    unsigned long BUF_PACKETS;
    unsigned long BUF_SLOW;


    //dvbs2
    unsigned long BUF_SLOTS;
    unsigned long BUF_FRAMES;
    unsigned long BUF_S2PACKETS;
    unsigned long S2_MAX_SYMBOLS;

    //************** LEANDBV Scheduler ***************

    leansdr::scheduler * m_objScheduler;
    struct config m_objCfg;

    bool m_blnDVBInitialized;
    bool m_blnNeedConfigUpdate;

    //LeanSDR Pipe Buffer
    // INPUT

    leansdr::pipebuf<leansdr::cf32> *p_rawiq;
    leansdr::pipewriter<leansdr::cf32> *p_rawiq_writer;
    leansdr::pipebuf<leansdr::cf32> *p_preprocessed;
    static const int m_RawIQMinWrite = 1;

    // NOTCH FILTER
    leansdr::auto_notch<leansdr::f32> *r_auto_notch;
    leansdr::pipebuf<leansdr::cf32> *p_autonotched;

    // FREQUENCY CORRECTION : DEROTATOR
    leansdr::pipebuf<leansdr::cf32> *p_derot;
    leansdr::rotator<leansdr::f32> *r_derot;

    // CNR ESTIMATION
    leansdr::pipebuf<leansdr::f32> *p_cnr;
    leansdr::cnr_fft<leansdr::f32> *r_cnr;

    //FILTERING
    leansdr::fir_filter<leansdr::cf32,float> *r_resample;
    leansdr::pipebuf<leansdr::cf32> *p_resampled;
    float *coeffs;
    int ncoeffs;

    // OUTPUT PREPROCESSED DATA
    leansdr::sampler_interface<leansdr::f32> *sampler;
    float *coeffs_sampler;
    int ncoeffs_sampler;

    leansdr::pipebuf<leansdr::eucl_ss> *p_symbols;
    leansdr::pipebuf<leansdr::f32> *p_freq;
    leansdr::pipebuf<leansdr::f32> *p_ss;
    leansdr::pipebuf<leansdr::f32> *p_mer;
    leansdr::pipebuf<leansdr::cf32> *p_sampled;

    //dvb-s2
    void *p_slots_dvbs2;
    leansdr::pipebuf<leansdr::cf32> *p_cstln;
    leansdr::pipebuf<leansdr::cf32> *p_cstln_pls;
    leansdr::pipebuf<int> *p_framelock;
    void *m_objDemodulatorDVBS2;
    void *p_fecframes;
    void *p_bbframes;
    void *p_s2_deinterleaver;
    void *r_fecdec;
    void *r_fecdecsoft;
    void *r_fecdechelper;
    void *p_deframer;

    //DECIMATION
    leansdr::pipebuf<leansdr::cf32> *p_decimated;
    leansdr::decimator<leansdr::cf32> *p_decim;

    //PROCESSED DATA MONITORING
    leansdr::file_writer<leansdr::cf32> *r_ppout;

    //GENERIC CONSTELLATION RECEIVER
    leansdr::cstln_receiver<leansdr::f32, leansdr::eucl_ss> *m_objDemodulator;

    // DECONVOLUTION AND SYNCHRONIZATION
    leansdr::pipebuf<leansdr::u8> *p_bytes;
    leansdr::deconvol_sync_simple *r_deconv;
    leansdr::viterbi_sync *r;
    leansdr::pipebuf<leansdr::u8> *p_descrambled;
    leansdr::pipebuf<leansdr::u8> *p_frames;

    leansdr::etr192_descrambler * r_etr192_descrambler;
    leansdr::hdlc_sync *r_sync;

    leansdr::pipebuf<leansdr::u8> *p_mpegbytes;
    leansdr::pipebuf<int> *p_lock;
    leansdr::pipebuf<leansdr::u32> *p_locktime;
    leansdr::mpeg_sync<leansdr::u8, 0> *r_sync_mpeg;


    // DEINTERLEAVING
    leansdr::pipebuf<leansdr::rspacket<leansdr::u8> > *p_rspackets;
    leansdr::deinterleaver<leansdr::u8> *r_deinter;

    // REED-SOLOMON
    leansdr::pipebuf<int> *p_vbitcount;
    leansdr::pipebuf<int> *p_verrcount;
    leansdr::pipebuf<leansdr::tspacket> *p_rtspackets;
    leansdr::rs_decoder<leansdr::u8, 0> *r_rsdec;

    // BER ESTIMATION
    leansdr::pipebuf<float> *p_vber;
    leansdr::rate_estimator<float> *r_vber;

    // DERANDOMIZATION
    leansdr::pipebuf<leansdr::tspacket> *p_tspackets;
    leansdr::derandomizer *r_derand;


    //OUTPUT
    leansdr::datvvideoplayer<leansdr::tspacket> *r_videoplayer;

    //CONSTELLATION
    leansdr::datvconstellation<leansdr::f32> *r_scope_symbols;
    leansdr::datvdvbs2constellation<leansdr::f32> *r_scope_symbols_dvbs2;
    leansdr::datvmeter *r_merMeter;
    leansdr::datvmeter *r_cnrMeter;

    //*************** DATV PARAMETERS  ***************
    TVScreen *m_tvScreen;
    DATVideoRender *m_videoRender;
    DATVideostream *m_videoStream;
    DATVUDPStream m_udpStream;
    DATVideoRenderThread *m_videoThread;

    // Audio
	AudioFifo m_audioFifo;

    fftfilt * m_objRFFilter;
    NCO m_objNCO;

    bool m_blnInitialized;
    bool m_blnRenderingVideo;
    bool m_cstlnSetByModcod;
    int m_modcodModulation;
    int m_modcodCodeRate;

    DATVDemodSettings::DATVModulation m_enmModulation;

    //DATVConfig m_objRunning;
    DATVDemodSettings m_settings;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    MovingAverageUtil<double, double, 32> m_objMagSqAverage;

    MessageQueue *m_messageQueueToGUI;
    QRecursiveMutex m_mutex;

    static const unsigned int m_rfFilterFftLength;
};

#endif // INCLUDE_DATVDEMODSINK_H
