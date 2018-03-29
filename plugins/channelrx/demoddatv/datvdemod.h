///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef INCLUDE_DATVDEMOD_H
#define INCLUDE_DATVDEMOD_H

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

#define rfFilterFftLength 1024

#ifndef LEANSDR_FRAMEWORK
#define LEANSDR_FRAMEWORK

//LeanSDR

#include "leansdr/framework.h"
#include "leansdr/generic.h"
#include "leansdr/dsp.h"
#include "leansdr/sdr.h"
#include "leansdr/dvb.h"
#include "leansdr/rs.h"
#include "leansdr/filtergen.h"

#include "leansdr/hdlc.h"
#include "leansdr/iess.h"

#endif

#include "datvconstellation.h"
#include "datvvideoplayer.h"

#include "channel/channelsinkapi.h"
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

#include "datvideostream.h"
#include "datvideorender.h"

#include <QMutex>

enum DATVModulation { BPSK, QPSK, PSK8, APSK16, APSK32, APSK64E, QAM16, QAM64, QAM256 };
enum dvb_version { DVB_S, DVB_S2 };
enum dvb_sampler { SAMP_NEAREST, SAMP_LINEAR, SAMP_RRC };

inline int decimation(float Fin, float Fout) { int d = Fin / Fout; return std::max(d, 1); }

struct config
{
  dvb_version  standard;
  dvb_sampler sampler;

  int buf_factor;      // Buffer sizing
  float Fs;            // Sampling frequency (Hz)
  float Fderot;        // Shift the signal (Hz). Note: Ftune is faster
  int anf;             // Number of auto notch filters
  bool cnr;            // Measure CNR
  unsigned int decim;  // Decimation, 0=auto
  float Fm;            // QPSK symbol rate (Hz)
  leansdr::cstln_lut<256>::predef constellation;
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
      standard(DVB_S),
      sampler(SAMP_LINEAR),
      buf_factor(4),
      Fs(2.4e6),
      Fderot(0),
      anf(0),
      cnr(false),
      decim(0),
      Fm(2e6),
      constellation(leansdr::cstln_lut<256>::QPSK),
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


struct DATVConfig
{
    int intMsps;
    int intRFBandwidth;
    int intCenterFrequency;
    dvb_version enmStandard;
    DATVModulation enmModulation;
    leansdr::code_rate enmFEC;
    int intSampleRate;
    int intSymbolRate;
    int intNotchFilters;
    bool blnAllowDrift;
    bool blnFastLock;
    dvb_sampler enmFilter;
    bool blnHardMetric;
    float fltRollOff;
    bool blnViterbi;
    int intExcursion;

    DATVConfig() :
        intMsps(1024000),
        intRFBandwidth(1024000),
        intCenterFrequency(0),
        enmStandard(DVB_S),
        enmModulation(BPSK),
        enmFEC(leansdr::FEC12),
        intSampleRate(1024000),
        intSymbolRate(250000),
        intNotchFilters(1),
        blnAllowDrift(false),
        blnFastLock(false),
        enmFilter(SAMP_LINEAR),
        blnHardMetric(false),
        fltRollOff(0.35),
        blnViterbi(false),
        intExcursion(10)
    {
    }
};


class DATVDemod : public BasebandSampleSink, public ChannelSinkAPI
{
	Q_OBJECT

public:

    DATVDemod(DeviceSourceAPI *);
    ~DATVDemod();

    virtual void destroy() { delete this; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }
    virtual qint64 getCenterFrequency() const { return m_objRunning.intCenterFrequency; }

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data __attribute__((unused))) { return false; }

    void configure(
        MessageQueue* objMessageQueue,
        int intRFBandwidth,
        int intCenterFrequency,
        dvb_version enmStandard,
        DATVModulation enmModulation,
        leansdr::code_rate enmFEC,
        int intSymbolRate,
        int intNotchFilters,
        bool blnAllowDrift,
        bool blnFastLock,
        dvb_sampler enmFilter,
        bool blnHardMetric,
        float fltRollOff,
        bool blnViterbi,
        int intfltExcursion);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    bool SetTVScreen(TVScreen *objScreen);
    DATVideostream * SetVideoRender(DATVideoRender *objScreen);

    bool PlayVideo(bool blnStartStop);

    void InitDATVParameters(
        int intMsps,
        int intRFBandwidth,
        int intCenterFrequency,
        dvb_version enmStandard,
        DATVModulation enmModulation,
        leansdr::code_rate enmFEC,
        int intSampleRate,
        int intSymbolRate,
        int intNotchFilters,
        bool blnAllowDrift,
        bool blnFastLock,
        dvb_sampler enmFilter,
        bool blnHardMetric,
        float fltRollOff,
        bool blnViterbi,
        int intEExcursion);

    void CleanUpDATVFramework(bool blnRelease);
    int GetSampleRate();
    void InitDATVFramework();
    double getMagSq() const { return m_objMagSqAverage; } //!< Beware this is scaled to 2^30

    static const QString m_channelIdURI;
    static const QString m_channelId;


    class MsgConfigureChannelizer : public Message
    {
        MESSAGE_CLASS_DECLARATION

        public:
            int getCenterFrequency() const { return m_centerFrequency; }

            static MsgConfigureChannelizer* create(int centerFrequency)
            {
                return new MsgConfigureChannelizer(centerFrequency);
            }

        private:
            int m_centerFrequency;

            MsgConfigureChannelizer(int centerFrequency) :
            Message(),
            m_centerFrequency(centerFrequency)
            { }
    };

private:
    class MsgConfigureDATVDemod : public Message
    {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgConfigureDATVDemod* create(
                int intRFBandwidth,
                int intCenterFrequency,
                dvb_version enmStandard,
                DATVModulation enmModulation,
                leansdr::code_rate enmFEC,
                int intSymbolRate,
                int intNotchFilters,
                bool blnAllowDrift,
                bool blnFastLock,
                dvb_sampler enmFilter,
                bool blnHardMetric,
                float fltRollOff,
                bool blnViterbi,
                int intExcursion)
            {
                return new MsgConfigureDATVDemod(intRFBandwidth,intCenterFrequency,enmStandard, enmModulation, enmFEC, intSymbolRate, intNotchFilters, blnAllowDrift,blnFastLock,enmFilter,blnHardMetric,fltRollOff, blnViterbi, intExcursion);
            }

            DATVConfig m_objMsgConfig;

        private:
            MsgConfigureDATVDemod(
                    int intRFBandwidth,
                    int intCenterFrequency,
                    dvb_version enmStandard,
                    DATVModulation enmModulation,
                    leansdr::code_rate enmFEC,
                    int intSymbolRate,
                    int intNotchFilters,
                    bool blnAllowDrift,
                    bool blnFastLock,
                    dvb_sampler enmFilter,
                    bool blnHardMetric,
                    float fltRollOff,
                    bool blnViterbi,
                    int intExcursion) :
                Message()
            {
                m_objMsgConfig.intRFBandwidth = intRFBandwidth;
                m_objMsgConfig.intCenterFrequency = intCenterFrequency;
                m_objMsgConfig.enmStandard = enmStandard;
                m_objMsgConfig.enmModulation = enmModulation;
                m_objMsgConfig.enmFEC = enmFEC;
                m_objMsgConfig.intSymbolRate = intSymbolRate;
                m_objMsgConfig.intNotchFilters = intNotchFilters;
                m_objMsgConfig.blnAllowDrift = blnAllowDrift;
                m_objMsgConfig.blnFastLock = blnFastLock;
                m_objMsgConfig.enmFilter= enmFilter;
                m_objMsgConfig.blnHardMetric = blnHardMetric;
                m_objMsgConfig.fltRollOff = fltRollOff;
                m_objMsgConfig.blnViterbi = blnViterbi;
                m_objMsgConfig.intExcursion = intExcursion;
            }
    };

    unsigned long m_lngExpectedReadIQ;
    unsigned long m_lngReadIQ;

    //************** LEANDBV Parameters **************

    unsigned long BUF_BASEBAND;
    unsigned long BUF_SYMBOLS;
    unsigned long BUF_BYTES;
    unsigned long BUF_MPEGBYTES;
    unsigned long BUF_PACKETS;
    unsigned long BUF_SLOW;

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

    leansdr::pipebuf<leansdr::softsymbol> *p_symbols;
    leansdr::pipebuf<leansdr::f32> *p_freq;
    leansdr::pipebuf<leansdr::f32> *p_ss;
    leansdr::pipebuf<leansdr::f32> *p_mer;
    leansdr::pipebuf<leansdr::cf32> *p_sampled;

    //DECIMATION
    leansdr::pipebuf<leansdr::cf32> *p_decimated;
    leansdr::decimator<leansdr::cf32> *p_decim;

    //PROCESSED DATA MONITORING
    leansdr::file_writer<leansdr::cf32> *r_ppout;

    //GENERIC CONSTELLATION RECEIVER
    leansdr::cstln_receiver<leansdr::f32> *m_objDemodulator;

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

    DeviceSourceAPI* m_deviceAPI;

    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    //*************** DATV PARAMETERS  ***************
    TVScreen * m_objRegisteredTVScreen;
    DATVideoRender * m_objRegisteredVideoRender;
    DATVideostream * m_objVideoStream;
    DATVideoRenderThread * m_objRenderThread;

    fftfilt * m_objRFFilter;
    NCO m_objNCO;

    bool m_blnInitialized;
    bool m_blnRenderingVideo;
    bool m_blnStartStopVideo;

    DATVModulation m_enmModulation;

    DATVConfig m_objRunning;
    MovingAverageUtil<double, double, 32> m_objMagSqAverage;

    QMutex m_objSettingsMutex;

    void ApplySettings();
};

#endif // INCLUDE_DATVDEMOD_H
