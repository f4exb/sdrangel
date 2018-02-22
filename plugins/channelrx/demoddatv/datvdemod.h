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
#include <dsp/basebandsamplesink.h>
#include <dsp/devicesamplesource.h>
#include <dsp/dspcommands.h>
#include <dsp/downchannelizer.h>
#include <dsp/fftfilt.h>
#include <QMutex>
#include <QElapsedTimer>
#include <vector>
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include <QBuffer>

#include "datvideostream.h"
#include "datvideorender.h"

using namespace leansdr;

enum DATVModulation { BPSK, QPSK, PSK8, APSK16, APSK32, APSK64E, QAM16, QAM64, QAM256 };
enum dvb_version { DVB_S, DVB_S2 };
enum dvb_sampler { SAMP_NEAREST, SAMP_LINEAR, SAMP_RRC };

inline int decimation(float Fin, float Fout) { int d = Fin / Fout; return max(d, 1); }

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
  cstln_lut<256>::predef constellation;
  code_rate fec;
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

  config() :  buf_factor(4),
              Fs(2.4e6),
              Fderot(0),
              anf(1),
              cnr(false),
              decim(0),
              Fm(2e6),
              standard(DVB_S),
              constellation(cstln_lut<256>::QPSK),
              fec(FEC12),
              Ftune(0),
              allow_drift(false),
              fastlock(true),
              viterbi(false),
              hard_metric(false),
              resample(false),
              resample_rej(10),
              sampler(SAMP_LINEAR),
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
    code_rate enmFEC;
    int intSampleRate;
    int intSymbolRate;
    int intNotchFilters;
    bool blnAllowDrift;
    bool blnFastLock;
    bool blnHDLC;
    bool blnHardMetric;
    bool blnResample;
    bool blnViterbi;

    DATVConfig() :
        intMsps(1024000),
        intRFBandwidth(1024000),
        intCenterFrequency(0),
        enmStandard(DVB_S),
        enmModulation(BPSK),
        enmFEC(FEC12),
        intSampleRate(1024000),
        intSymbolRate(250000),
        intNotchFilters(1),
        blnAllowDrift(false),
        blnFastLock(false),
        blnHDLC(false),
        blnHardMetric(false),
        blnResample(false),
        blnViterbi(false)
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



    void configure(MessageQueue* objMessageQueue,
                   int intRFBandwidth,
                   int intCenterFrequency,
                   dvb_version enmStandard,
                   DATVModulation enmModulation,
                   code_rate enmFEC,
                   int intSymbolRate,
                   int intNotchFilters,
                   bool blnAllowDrift,
                   bool blnFastLock,
                   bool blnHDLC,
                   bool blnHardMetric,
                   bool blnResample,
                   bool blnViterbi);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    bool SetDATVScreen(DATVScreen *objScreen);
    DATVideostream * SetVideoRender(DATVideoRender *objScreen);

    bool PlayVideo(bool blnStartStop);

    void InitDATVParameters(int intMsps,
                            int intRFBandwidth,
                            int intCenterFrequency,
                            dvb_version enmStandard,
                            DATVModulation enmModulation,
                            code_rate enmFEC,
                            int intSampleRate,
                            int intSymbolRate,
                            int intNotchFilters,
                            bool blnAllowDrift,
                            bool blnFastLock,
                            bool blnHDLC,
                            bool blnHardMetric,
                            bool blnResample,
                            bool blnViterbi);

    void CleanUpDATVFramework(bool blnRelease);
    int GetSampleRate();
    void InitDATVFramework();

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

    scheduler * m_objScheduler;
    struct config m_objCfg;

    bool m_blnDVBInitialized;
    bool m_blnNeedConfigUpdate;

    //LeanSDR Pipe Buffer
    // INPUT

    pipebuf<cf32> *p_rawiq;
    pipewriter<cf32> *p_rawiq_writer;
    pipebuf<cf32> *p_preprocessed;

    // NOTCH FILTER
    auto_notch<f32> *r_auto_notch;
    pipebuf<cf32> *p_autonotched;

    // FREQUENCY CORRECTION : DEROTATOR
    pipebuf<cf32> *p_derot;
    rotator<f32> *r_derot;

    // CNR ESTIMATION
    pipebuf<f32> *p_cnr;
    cnr_fft<f32> *r_cnr;

    //FILTERING
    fir_filter<cf32,float> *r_resample;
    pipebuf<cf32> *p_resampled;
    float *coeffs;
    int ncoeffs;

    // OUTPUT PREPROCESSED DATA
    sampler_interface<f32> *sampler;
    float *coeffs_sampler;
    int ncoeffs_sampler;

    pipebuf<softsymbol> *p_symbols;
    pipebuf<f32> *p_freq;
    pipebuf<f32> *p_ss;
    pipebuf<f32> *p_mer;
    pipebuf<cf32> *p_sampled;

    //DECIMATION
    pipebuf<cf32> *p_decimated;
    decimator<cf32> *p_decim;

    //PROCESSED DATA MONITORING
    file_writer<cf32> *r_ppout;

    //GENERIC CONSTELLATION RECEIVER
    cstln_receiver<f32> *m_objDemodulator;

    // DECONVOLUTION AND SYNCHRONIZATION
    pipebuf<u8> *p_bytes;
    deconvol_sync_simple *r_deconv;
    viterbi_sync *r;
    pipebuf<u8> *p_descrambled;
    pipebuf<u8> *p_frames;

    etr192_descrambler * r_etr192_descrambler;
    hdlc_sync *r_sync;

    pipebuf<u8> *p_mpegbytes;
    pipebuf<int> *p_lock;
    pipebuf<u32> *p_locktime;
    mpeg_sync<u8,0> *r_sync_mpeg;


    // DEINTERLEAVING
    pipebuf< rspacket<u8> > *p_rspackets;
    deinterleaver<u8> *r_deinter;

    // REED-SOLOMON
    pipebuf<int> *p_vbitcount;
    pipebuf<int> *p_verrcount;
    pipebuf<tspacket> *p_rtspackets;
    rs_decoder<u8,0> *r_rsdec;

    // BER ESTIMATION
    pipebuf<float> *p_vber;
    rate_estimator<float> *r_vber;

    // DERANDOMIZATION
    pipebuf<tspacket> *p_tspackets;
    derandomizer *r_derand;


    //OUTPUT
    file_writer<tspacket> *r_stdout;
    datvvideoplayer<tspacket> *r_videoplayer;

    //CONSTELLATION
    datvconstellation<f32> *r_scope_symbols;


private:

       DeviceSourceAPI* m_deviceAPI;

       ThreadedBasebandSampleSink* m_threadedChannelizer;
       DownChannelizer* m_channelizer;

       //*************** DATV PARAMETERS  ***************
       DATVScreen * m_objRegisteredDATVScreen;
       DATVideoRender * m_objRegisteredVideoRender;
       DATVideostream * m_objVideoStream;
       DATVideoRenderThread * m_objRenderThread;

       fftfilt * m_objRFFilter;
       NCO m_objNCO;

       bool m_blnInitialized;
       bool m_blnRenderingVideo;

       DATVModulation m_enmModulation;

       //QElapsedTimer m_objTimer;
private:

    class MsgConfigureDATVDemod : public Message
    {
		MESSAGE_CLASS_DECLARATION

        public:
            static MsgConfigureDATVDemod* create(int intRFBandwidth,
                                                 int intCenterFrequency,
                                                 dvb_version enmStandard,
                                                 DATVModulation enmModulation,
                                                 code_rate enmFEC,                                                 
                                                 int intSymbolRate,
                                                 int intNotchFilters,
                                                 bool blnAllowDrift,
                                                 bool blnFastLock,
                                                 bool blnHDLC,
                                                 bool blnHardMetric,
                                                 bool blnResample,
                                                 bool blnViterbi)
            {
                return new MsgConfigureDATVDemod(intRFBandwidth,intCenterFrequency,enmStandard, enmModulation, enmFEC, intSymbolRate, intNotchFilters, blnAllowDrift,blnFastLock,blnHDLC,blnHardMetric,blnResample, blnViterbi);
            }

            DATVConfig m_objMsgConfig;

        private:
            MsgConfigureDATVDemod(int intRFBandwidth,
                                  int intCenterFrequency,
                                  dvb_version enmStandard,
                                  DATVModulation enmModulation,
                                  code_rate enmFEC,
                                  int intSymbolRate,
                                  int intNotchFilters,
                                  bool blnAllowDrift,
                                  bool blnFastLock,
                                  bool blnHDLC,
                                  bool blnHardMetric,
                                  bool blnResample,
                                  bool blnViterbi) :
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
                                        m_objMsgConfig.blnHDLC = blnHDLC;
                                        m_objMsgConfig.blnHardMetric = blnHardMetric;
                                        m_objMsgConfig.blnResample = blnResample;
                                        m_objMsgConfig.blnViterbi = blnViterbi;
                                  }
    };


    DATVConfig m_objRunning;

    QMutex m_objSettingsMutex;

    void ApplySettings();

};

#endif // INCLUDE_DATVDEMOD_H
