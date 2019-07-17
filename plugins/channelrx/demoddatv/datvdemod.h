///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef INCLUDE_DATVDEMOD_H
#define INCLUDE_DATVDEMOD_H

class DeviceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

#define rfFilterFftLength 1024

//LeanSDR
#include "leansdr/framework.h"
#include "leansdr/generic.h"
#include "leansdr/dvb.h"
#include "leansdr/filtergen.h"

#include "leansdr/hdlc.h"
#include "leansdr/iess.h"

#include "datvconstellation.h"
#include "datvdvbs2constellation.h"
#include "datvvideoplayer.h"
#include "datvideostream.h"
#include "datvideorender.h"
#include "datvdemodsettings.h"

#include "channel/channelapi.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"
#include "dsp/fftfilt.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "util/movingaverage.h"

#include <QMutex>

// enum DATVModulation { BPSK, QPSK, PSK8, APSK16, APSK32, APSK64E, QAM16, QAM64, QAM256 };
// enum dvb_version { DVB_S, DVB_S2 };
// enum dvb_sampler { SAMP_NEAREST, SAMP_LINEAR, SAMP_RRC };

inline int decimation(float Fin, float Fout) { int d = Fin / Fout; return std::max(d, 1); }

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
        cnr(false),
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

class DATVDemod : public BasebandSampleSink, public ChannelAPI
{
	Q_OBJECT

public:

    DATVDemod(DeviceAPI *);
    ~DATVDemod();

    virtual void destroy() { delete this; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }
    virtual qint64 getCenterFrequency() const { return m_settings.m_centerFrequency; }

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void) data; return false; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_centerFrequency;
    }

    bool SetTVScreen(TVScreen *objScreen);
    DATVideostream * SetVideoRender(DATVideoRender *objScreen);
    bool audioActive();
    bool audioDecodeOK();
    bool videoActive();
    bool videoDecodeOK();

    bool PlayVideo(bool blnStartStop);

    void InitDATVParameters(
        int intMsps,
        int intRFBandwidth,
        int intCenterFrequency,
        DATVDemodSettings::dvb_version enmStandard,
        DATVDemodSettings::DATVModulation enmModulation,
        leansdr::code_rate enmFEC,
        int intSampleRate,
        int intSymbolRate,
        int intNotchFilters,
        bool blnAllowDrift,
        bool blnFastLock,
        DATVDemodSettings::dvb_sampler enmFilter,
        bool blnHardMetric,
        float fltRollOff,
        bool blnViterbi,
        int intEExcursion);

    void CleanUpDATVFramework(bool blnRelease);
    int GetSampleRate();
    void InitDATVFramework();
    void InitDATVS2Framework();
    double getMagSq() const { return m_objMagSqAverage; } //!< Beware this is scaled to 2^30
    int getModcodModulation() const { return m_modcodModulation; }
    int getModcodCodeRate() const { return m_modcodCodeRate; }
    bool isCstlnSetByModcod() const { return m_cstlnSetByModcod; }
    static DATVDemodSettings::DATVCodeRate getCodeRateFromLeanDVBCode(int leanDVBCodeRate);
    static DATVDemodSettings::DATVModulation getModulationFromLeanDVBCode(int leanDVBModulation);
    static int getLeanDVBCodeRateFromDATV(DATVDemodSettings::DATVCodeRate datvCodeRate);
    static int getLeanDVBModulationFromDATV(DATVDemodSettings::DATVModulation datvModulation);

    static const QString m_channelIdURI;
    static const QString m_channelId;

    class MsgConfigureChannelizer : public Message
    {
        MESSAGE_CLASS_DECLARATION

        public:
            int getCenterFrequency() const { return m_centerFrequency; }

            static MsgConfigureChannelizer* create(int centerFrequency) {
                return new MsgConfigureChannelizer(centerFrequency);
            }

        private:
            int m_centerFrequency;

            MsgConfigureChannelizer(int centerFrequency) :
            Message(),
            m_centerFrequency(centerFrequency)
            {}
    };

    class MsgConfigureDATVDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DATVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDATVDemod* create(const DATVDemodSettings& settings, bool force)
        {
            return new MsgConfigureDATVDemod(settings, force);
        }

    private:
        DATVDemodSettings m_settings;
        bool m_force;

        MsgConfigureDATVDemod(const DATVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgReportModcodCstlnChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        DATVDemodSettings::DATVModulation getModulation() const { return m_modulation; }
        DATVDemodSettings::DATVCodeRate getCodeRate() const { return m_codeRate; }

        static MsgReportModcodCstlnChange* create(const DATVDemodSettings::DATVModulation& modulation,
            const DATVDemodSettings::DATVCodeRate& codeRate)
        {
            return new MsgReportModcodCstlnChange(modulation, codeRate);
        }

    private:
        DATVDemodSettings::DATVModulation m_modulation;
        DATVDemodSettings::DATVCodeRate m_codeRate;

        MsgReportModcodCstlnChange(
            const DATVDemodSettings::DATVModulation& modulation,
            const DATVDemodSettings::DATVCodeRate& codeRate
        ) :
            Message(),
            m_modulation(modulation),
            m_codeRate(codeRate)
        { }
    };

private:
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
    leansdr::file_writer<leansdr::tspacket> *r_stdout;
    leansdr::datvvideoplayer<leansdr::tspacket> *r_videoplayer;

    //CONSTELLATION
    leansdr::datvconstellation<leansdr::f32> *r_scope_symbols;
    leansdr::datvdvbs2constellation<leansdr::f32> *r_scope_symbols_dvbs2;

    DeviceAPI* m_deviceAPI;

    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    //*************** DATV PARAMETERS  ***************
    TVScreen *m_objRegisteredTVScreen;
    DATVideoRender *m_objRegisteredVideoRender;
    DATVideostream *m_objVideoStream;
    DATVideoRenderThread *m_objRenderThread;

    // Audio
	AudioFifo m_audioFifo;

    fftfilt * m_objRFFilter;
    NCO m_objNCO;

    bool m_blnInitialized;
    bool m_blnRenderingVideo;
    bool m_blnStartStopVideo;
    bool m_cstlnSetByModcod;
    int m_modcodModulation;
    int m_modcodCodeRate;

    DATVDemodSettings::DATVModulation m_enmModulation;

    //DATVConfig m_objRunning;
    DATVDemodSettings m_settings;
    int m_sampleRate;
    int m_inputFrequencyOffset;
    MovingAverageUtil<double, double, 32> m_objMagSqAverage;

    QMutex m_objSettingsMutex;

    //void ApplySettings();
    void applySettings(const DATVDemodSettings& settings, bool force = false);
	void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
};

#endif // INCLUDE_DATVDEMOD_H
