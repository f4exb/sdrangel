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

#include "datvdemod.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"

#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "device/devicesourceapi.h"

const QString DATVDemod::m_channelIdURI = "sdrangel.channel.demoddatv";
const QString DATVDemod::m_channelId = "DATVDemod";

MESSAGE_CLASS_DEFINITION(DATVDemod::MsgConfigureDATVDemod, Message)
MESSAGE_CLASS_DEFINITION(DATVDemod::MsgConfigureChannelizer, Message)

DATVDemod::DATVDemod(DeviceSourceAPI *deviceAPI) :
    ChannelSinkAPI(m_channelIdURI),
    m_blnNeedConfigUpdate(false),
    m_deviceAPI(deviceAPI),
    m_objRegisteredTVScreen(0),
    m_objRegisteredVideoRender(0),
    m_objVideoStream(NULL),
    m_objRenderThread(NULL),
    m_blnRenderingVideo(false),
    m_blnStartStopVideo(false),
    m_enmModulation(BPSK /*DATV_FM1*/),
    m_objSettingsMutex(QMutex::NonRecursive)
{
    setObjectName("DATVDemod");

    //*************** DATV PARAMETERS  ***************
    m_blnInitialized=false;
    CleanUpDATVFramework(false);

    m_objVideoStream = new DATVideostream();

    m_objRFFilter = new fftfilt(-256000.0 / 1024000.0, 256000.0 / 1024000.0, rfFilterFftLength);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

DATVDemod::~DATVDemod()
{
    m_blnInitialized=false;

    if(m_objVideoStream!=NULL)
    {
        //Immediately exit from DATVideoStream if waiting for data before killing thread
        m_objVideoStream->ThreadTimeOut=0;
    }

    if(m_objRenderThread!=NULL)
    {
        if(m_objRenderThread->isRunning())
        {
           m_objRenderThread->stopRendering();
           m_objRenderThread->quit();
        }

        m_objRenderThread->wait(2000);
    }

    CleanUpDATVFramework(true);

    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete m_objRFFilter;
}

bool DATVDemod::SetTVScreen(TVScreen *objScreen)
{
    m_objRegisteredTVScreen = objScreen;
    return true;
}

DATVideostream * DATVDemod::SetVideoRender(DATVideoRender *objScreen)
{
    m_objRegisteredVideoRender = objScreen;

    m_objRenderThread = new DATVideoRenderThread(m_objRegisteredVideoRender,m_objVideoStream);

    return m_objVideoStream;
}


bool DATVDemod::PlayVideo(bool blnStartStop)
{

    if(m_objVideoStream==NULL)
    {
        return false;
    }

    if(m_objRegisteredVideoRender==NULL)
    {
        return false;
    }

    if(m_objRenderThread==NULL)
    {
        return false;
    }

    if (m_blnStartStopVideo && !blnStartStop)
    {
        return true;
    }

    if(blnStartStop==true)
    {
        m_blnStartStopVideo=true;
    }

    if(m_objRenderThread->isRunning())
    {
       if(blnStartStop==true)
       {
           m_objRenderThread->stopRendering();
       }

       return true;
    }

    if(m_objVideoStream->bytesAvailable()>0)
    {
        m_objRenderThread->setStreamAndRenderer(m_objRegisteredVideoRender,m_objVideoStream);
        m_objVideoStream->MultiThreaded=true;
        m_objVideoStream->ThreadTimeOut=5000; //5000 ms
        m_objRenderThread->start();
    }

    return true;
}

void DATVDemod::configure(MessageQueue* objMessageQueue,
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
    Message* msgCmd = MsgConfigureDATVDemod::create(intRFBandwidth,intCenterFrequency,enmStandard, enmModulation, enmFEC, intSymbolRate, intNotchFilters, blnAllowDrift,blnFastLock,enmFilter,blnHardMetric,fltRollOff, blnViterbi,intExcursion);
    objMessageQueue->push(msgCmd);
}

void DATVDemod::InitDATVParameters(int intMsps,
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
                                   int intExcursion)
{
    Real fltLowCut;
    Real fltHiCut;

    m_objSettingsMutex.lock();

    m_blnInitialized=false;

    //Bandpass filter shaping

    fltLowCut = -((float)intRFBandwidth / 2.0) / (float)intMsps;
    fltHiCut  = ((float)intRFBandwidth / 2.0) / (float)intMsps;
    m_objRFFilter->create_filter(fltLowCut, fltHiCut);
    m_objNCO.setFreq(-(float)intCenterFrequency,(float)intMsps);

    //Config update

    m_objRunning.intMsps = intMsps;
    m_objRunning.intCenterFrequency = intCenterFrequency;
    m_objRunning.intRFBandwidth = intRFBandwidth;
    m_objRunning.enmStandard = enmStandard;
    m_objRunning.enmModulation = enmModulation;
    m_objRunning.enmFEC = enmFEC;
    m_objRunning.intSampleRate = intSampleRate;
    m_objRunning.intSymbolRate = intSymbolRate;
    m_objRunning.intNotchFilters = intNotchFilters;
    m_objRunning.blnAllowDrift = blnAllowDrift;
    m_objRunning.blnFastLock = blnFastLock;
    m_objRunning.enmFilter = enmFilter;
    m_objRunning.blnHardMetric = blnHardMetric;
    m_objRunning.fltRollOff = fltRollOff;
    m_objRunning.blnViterbi = blnViterbi;
    m_objRunning.intExcursion = intExcursion;

    m_blnInitialized=true;

    m_objSettingsMutex.unlock();

    m_blnNeedConfigUpdate=true;
}

void DATVDemod::CleanUpDATVFramework(bool blnRelease)
{
    if(blnRelease==true)
    {
        if(m_objScheduler!=NULL)
        {
            m_objScheduler->shutdown();
            delete m_objScheduler;
        }

        // NOTCH FILTER

        if(r_auto_notch!=NULL) delete r_auto_notch;
        if(p_autonotched!=NULL) delete p_autonotched;

        // FREQUENCY CORRECTION : DEROTATOR
        if(p_derot!=NULL) delete p_derot;
        if(r_derot!=NULL) delete r_derot;

        // CNR ESTIMATION
        if(p_cnr!=NULL) delete p_cnr;
        if(r_cnr!=NULL) delete r_cnr;

        //FILTERING
        if(r_resample!=NULL) delete r_resample;
        if(p_resampled!=NULL) delete p_resampled;
        if(coeffs!=NULL) delete coeffs;

        // OUTPUT PREPROCESSED DATA
        if(sampler!=NULL) delete sampler;
        if(coeffs_sampler!=NULL) delete coeffs_sampler;
        if(p_symbols!=NULL) delete p_symbols;
        if(p_freq!=NULL) delete p_freq;
        if(p_ss!=NULL) delete p_ss;
        if(p_mer!=NULL) delete p_mer;
        if(p_sampled!=NULL) delete p_sampled;

        //DECIMATION
        if(p_decimated!=NULL) delete p_decimated;
        if(p_decim!=NULL) delete p_decim;
        if(r_ppout!=NULL) delete r_ppout;


        //GENERIC CONSTELLATION RECEIVER
        if(m_objDemodulator!=NULL) delete m_objDemodulator;

        //DECONVOLUTION AND SYNCHRONIZATION
        if(p_bytes!=NULL) delete p_bytes;
        if(r_deconv!=NULL) delete r_deconv;
        if(r!=NULL) delete r;
        if(p_descrambled!=NULL) delete p_descrambled;
        if(p_frames!=NULL) delete p_frames;
        if(r_etr192_descrambler!=NULL) delete r_etr192_descrambler;
        if(r_sync!=NULL) delete r_sync;
        if(p_mpegbytes!=NULL) delete p_mpegbytes;
        if(p_lock!=NULL) delete p_lock;
        if(p_locktime!=NULL) delete p_locktime;
        if(r_sync_mpeg!=NULL) delete r_sync_mpeg;


        // DEINTERLEAVING
        if(p_rspackets!=NULL) delete p_rspackets;
        if(r_deinter!=NULL) delete r_deinter;
        if(p_vbitcount!=NULL) delete p_vbitcount;
        if(p_verrcount!=NULL) delete p_verrcount;
        if(p_rtspackets!=NULL) delete p_rtspackets;
        if(r_rsdec!=NULL) delete r_rsdec;

        //BER ESTIMATION
        if(p_vber!=NULL) delete p_vber;
        if(r_vber!=NULL) delete r_vber;

        // DERANDOMIZATION
        if(p_tspackets!=NULL) delete p_tspackets;
        if(r_derand!=NULL) delete r_derand;


        //OUTPUT : To remove
        if(r_stdout!=NULL) delete r_stdout;
        if(r_videoplayer!=NULL) delete r_videoplayer;

        //CONSTELLATION
        if(r_scope_symbols!=NULL) delete r_scope_symbols;

        // INPUT
        //if(p_rawiq!=NULL) delete p_rawiq;
        //if(p_rawiq_writer!=NULL) delete p_rawiq_writer;
        //if(p_preprocessed!=NULL) delete p_preprocessed;


    }

    m_objScheduler=NULL;

    // INPUT

    p_rawiq = NULL;
    p_rawiq_writer = NULL;

    p_preprocessed = NULL;

    // NOTCH FILTER
    r_auto_notch = NULL;
    p_autonotched = NULL;

    // FREQUENCY CORRECTION : DEROTATOR
    p_derot = NULL;
    r_derot=NULL;

    // CNR ESTIMATION
    p_cnr = NULL;
    r_cnr = NULL;

    //FILTERING
    r_resample = NULL;
    p_resampled = NULL;
    coeffs = NULL;
    ncoeffs=0;

    // OUTPUT PREPROCESSED DATA
    sampler = NULL;
    coeffs_sampler=NULL;
    ncoeffs_sampler=0;

    p_symbols = NULL;
    p_freq = NULL;
    p_ss = NULL;
    p_mer = NULL;
    p_sampled = NULL;

    //DECIMATION
    p_decimated = NULL;
    p_decim = NULL;
    r_ppout = NULL;

    //GENERIC CONSTELLATION RECEIVER
    m_objDemodulator = NULL;

    //DECONVOLUTION AND SYNCHRONIZATION
    p_bytes=NULL;
    r_deconv=NULL;
    r = NULL;

    p_descrambled = NULL;
    p_frames = NULL;
    r_etr192_descrambler = NULL;
    r_sync = NULL;

    p_mpegbytes = NULL;
    p_lock = NULL;
    p_locktime = NULL;
    r_sync_mpeg = NULL;


    // DEINTERLEAVING
    p_rspackets = NULL;
    r_deinter = NULL;

    p_vbitcount = NULL;
    p_verrcount = NULL;
    p_rtspackets = NULL;
    r_rsdec = NULL;


    //BER ESTIMATION
    p_vber = NULL;
    r_vber  = NULL;


    // DERANDOMIZATION
    p_tspackets = NULL;
    r_derand = NULL;


    //OUTPUT : To remove
    r_stdout = NULL;
    r_videoplayer = NULL;


    //CONSTELLATION
    r_scope_symbols = NULL;
}

void DATVDemod::InitDATVFramework()
{
    m_blnDVBInitialized=false;
    m_lngReadIQ=0;
    CleanUpDATVFramework(false);

    qDebug()  << "DATVDemod::InitDATVParameters:"
                <<  " Msps: " << m_objRunning.intMsps
                <<  " Sample Rate: " << m_objRunning.intSampleRate
                <<  " Symbol Rate: " << m_objRunning.intSymbolRate
                <<  " Modulation: " << m_objRunning.enmModulation
                <<  " Notch Filters: " << m_objRunning.intNotchFilters
                <<  " Allow Drift: " << m_objRunning.blnAllowDrift
                <<  " Fast Lock: " << m_objRunning.blnFastLock
                <<  " Filter: " << m_objRunning.enmFilter
                <<  " HARD METRIC: " << m_objRunning.blnHardMetric
                <<  " RollOff: " << m_objRunning.fltRollOff
                <<  " Viterbi: " << m_objRunning.blnViterbi
                <<  " Excursion: " << m_objRunning.intExcursion;

    m_objCfg.standard = m_objRunning.enmStandard;

    m_objCfg.fec = m_objRunning.enmFEC;
    m_objCfg.Fs = (float) m_objRunning.intSampleRate;
    m_objCfg.Fm = (float) m_objRunning.intSymbolRate;
    m_objCfg.fastlock = m_objRunning.blnFastLock;

    m_objCfg.sampler = m_objRunning.enmFilter;
    m_objCfg.rolloff=m_objRunning.fltRollOff;  //0...1
    m_objCfg.rrc_rej=(float) m_objRunning.intExcursion;  //dB
    m_objCfg.rrc_steps=0; //auto

    switch(m_objRunning.enmModulation)
    {
        case BPSK:
           m_objCfg.constellation = leansdr::cstln_lut<256>::BPSK;
        break;

        case QPSK:
            m_objCfg.constellation = leansdr::cstln_lut<256>::QPSK;
        break;

        case PSK8:
            m_objCfg.constellation = leansdr::cstln_lut<256>::PSK8;
        break;

        case APSK16:
            m_objCfg.constellation = leansdr::cstln_lut<256>::APSK16;
        break;

        case APSK32:
           m_objCfg.constellation = leansdr::cstln_lut<256>::APSK32;
        break;

        case APSK64E:
           m_objCfg.constellation = leansdr::cstln_lut<256>::APSK64E;
        break;

        case QAM16:
           m_objCfg.constellation = leansdr::cstln_lut<256>::QAM16;
        break;

        case QAM64:
           m_objCfg.constellation = leansdr::cstln_lut<256>::QAM64;
        break;

        case QAM256:
           m_objCfg.constellation = leansdr::cstln_lut<256>::QAM256;
        break;

        default:
           m_objCfg.constellation = leansdr::cstln_lut<256>::BPSK;
        break;
    }

    m_objCfg.allow_drift = m_objRunning.blnAllowDrift;
    m_objCfg.anf = m_objRunning.intNotchFilters;
    m_objCfg.hard_metric = m_objRunning.blnHardMetric;
    m_objCfg.sampler = m_objRunning.enmFilter;
    m_objCfg.viterbi = m_objRunning.blnViterbi;


    // Min buffer size for baseband data
    //   scopes: 1024
    //   ss_estimator: 1024
    //   anf: 4096
    //   cstln_receiver: reads in chunks of 128+1
    BUF_BASEBAND = 4096 * m_objCfg.buf_factor;

    // Min buffer size for IQ symbols
    //   cstln_receiver: writes in chunks of 128/omega symbols (margin 128)
    //   deconv_sync: reads at least 64+32
    // A larger buffer improves performance significantly.
    BUF_SYMBOLS = 1024 * m_objCfg.buf_factor;

    // Min buffer size for unsynchronized bytes
    //   deconv_sync: writes 32 bytes
    //   mpeg_sync: reads up to 204*scan_syncs = 1632 bytes
    BUF_BYTES = 2048 * m_objCfg.buf_factor;

    // Min buffer size for synchronized (but interleaved) bytes
    //   mpeg_sync: writes 1 rspacket
    //   deinterleaver: reads 17*11*12+204 = 2448 bytes
    BUF_MPEGBYTES = 2448 * m_objCfg.buf_factor;

    // Min buffer size for packets: 1
    BUF_PACKETS = m_objCfg.buf_factor;

    // Min buffer size for misc measurements: 1
    BUF_SLOW = m_objCfg.buf_factor;

    m_lngExpectedReadIQ  = BUF_BASEBAND;

    m_objScheduler = new leansdr::scheduler();

    //***************
    p_rawiq = new leansdr::pipebuf<leansdr::cf32>(m_objScheduler, "rawiq", BUF_BASEBAND);
    p_rawiq_writer = new leansdr::pipewriter<leansdr::cf32>(*p_rawiq);
    p_preprocessed = p_rawiq;

    // NOTCH FILTER

    if ( m_objCfg.anf>0 )
    {
        p_autonotched = new leansdr::pipebuf<leansdr::cf32>(m_objScheduler, "autonotched", BUF_BASEBAND);
        r_auto_notch = new leansdr::auto_notch<leansdr::f32>(m_objScheduler, *p_preprocessed, *p_autonotched, m_objCfg.anf, 0);
        p_preprocessed = p_autonotched;
    }


    // FREQUENCY CORRECTION

    //******** -> if ( m_objCfg.Fderot>0 )

    // CNR ESTIMATION

    p_cnr = new leansdr::pipebuf<leansdr::f32>(m_objScheduler, "cnr", BUF_SLOW);

    if ( m_objCfg.cnr==true )
    {
        r_cnr = new leansdr::cnr_fft<leansdr::f32>(m_objScheduler, *p_preprocessed, *p_cnr, m_objCfg.Fm/m_objCfg.Fs);
        r_cnr->decimation = decimation(m_objCfg.Fs, 1);  // 1 Hz
    }

    // FILTERING

    int decim = 1;

    //******** -> if ( m_objCfg.resample )


    // DECIMATION
    // (Unless already done in resampler)

    //******** -> if ( !m_objCfg.resample && m_objCfg.decim>1 )

    //Resampling FS


    // Generic constellation receiver

    p_symbols = new leansdr::pipebuf<leansdr::softsymbol>(m_objScheduler, "PSK soft-symbols", BUF_SYMBOLS);
    p_freq = new leansdr::pipebuf<leansdr::f32> (m_objScheduler, "freq", BUF_SLOW);
    p_ss = new leansdr::pipebuf<leansdr::f32> (m_objScheduler, "SS", BUF_SLOW);
    p_mer = new leansdr::pipebuf<leansdr::f32> (m_objScheduler, "MER", BUF_SLOW);
    p_sampled = new leansdr::pipebuf<leansdr::cf32> (m_objScheduler, "PSK symbols", BUF_BASEBAND);

    switch ( m_objCfg.sampler )
    {
        case SAMP_NEAREST:
          sampler = new leansdr::nearest_sampler<float>();
          break;

        case SAMP_LINEAR:
          sampler = new leansdr::linear_sampler<float>();
          break;

        case SAMP_RRC:
        {


          if ( m_objCfg.rrc_steps == 0 )
          {
            // At least 64 discrete sampling points between symbols
            m_objCfg.rrc_steps = std::max(1, (int)(64*m_objCfg.Fm / m_objCfg.Fs));
          }

          float Frrc = m_objCfg.Fs * m_objCfg.rrc_steps;  // Sample freq of the RRC filter
          float transition = (m_objCfg.Fm/2) * m_objCfg.rolloff;
          int order = m_objCfg.rrc_rej * Frrc / (22*transition);
          ncoeffs_sampler = leansdr::filtergen::root_raised_cosine(order, m_objCfg.Fm/Frrc, m_objCfg.rolloff, &coeffs_sampler);

          sampler = new leansdr::fir_sampler<float,float>(ncoeffs_sampler, coeffs_sampler, m_objCfg.rrc_steps);
          break;
        }

        default:
          qCritical("DATVDemod::InitDATVFramework: Interpolator not implemented");
          return;
    }

    m_objDemodulator = new leansdr::cstln_receiver<leansdr::f32>(m_objScheduler, sampler, *p_preprocessed, *p_symbols, p_freq, p_ss, p_mer, p_sampled);

    if ( m_objCfg.standard == DVB_S )
    {
      if ( m_objCfg.constellation != leansdr::cstln_lut<256>::QPSK && m_objCfg.constellation != leansdr::cstln_lut<256>::BPSK )
      {
        fprintf(stderr, "Warning: non-standard constellation for DVB-S\n");
      }
    }

    if ( m_objCfg.standard == DVB_S2 )
    {
      // For DVB-S2 testing only.
      // Constellation should be determined from PL signalling.
      fprintf(stderr, "DVB-S2: Testing symbol sampler only.\n");
    }

    m_objDemodulator->cstln = make_dvbs2_constellation(m_objCfg.constellation, m_objCfg.fec);

    if ( m_objCfg.hard_metric )
    {
      m_objDemodulator->cstln->harden();
    }

    m_objDemodulator->set_omega(m_objCfg.Fs/m_objCfg.Fm);

    //******** if ( m_objCfg.Ftune )
    //{
    //  m_objDemodulator->set_freq(m_objCfg.Ftune/m_objCfg.Fs);
    //}

    if ( m_objCfg.allow_drift )
    {
      m_objDemodulator->set_allow_drift(true);
    }

    //******** -> if ( m_objCfg.viterbi )
    if ( m_objCfg.viterbi )
    {
      m_objDemodulator->pll_adjustment /= 6;
    }


    m_objDemodulator->meas_decimation = decimation(m_objCfg.Fs, m_objCfg.Finfo);

    // TRACKING FILTERS


    if ( r_cnr )
    {
      r_cnr->freq_tap = &m_objDemodulator->freq_tap;
      r_cnr->tap_multiplier = 1.0 / decim;
    }

    //constellation

    if (m_objRegisteredTVScreen)
    {
        m_objRegisteredTVScreen->resizeTVScreen(256,256);

        r_scope_symbols = new leansdr::datvconstellation<leansdr::f32>(m_objScheduler, *p_sampled, -128,128, NULL, m_objRegisteredTVScreen);
        r_scope_symbols->decimation = 1;
        r_scope_symbols->cstln = &m_objDemodulator->cstln;
        r_scope_symbols->calculate_cstln_points();
    }

    // DECONVOLUTION AND SYNCHRONIZATION

    p_bytes = new leansdr::pipebuf<leansdr::u8>(m_objScheduler, "bytes", BUF_BYTES);

    r_deconv = NULL;

    //******** -> if ( m_objCfg.viterbi )

    if ( m_objCfg.viterbi )
    {
      if ( m_objCfg.fec == leansdr::FEC23 && (m_objDemodulator->cstln->nsymbols == 4 || m_objDemodulator->cstln->nsymbols == 64) )
      {
        m_objCfg.fec = leansdr::FEC46;
      }

      //To uncomment -> Linking Problem : undefined symbol: _ZN7leansdr21viterbi_dec_interfaceIhhiiE6updateEPiS2_
      r = new leansdr::viterbi_sync(m_objScheduler, (*p_symbols), (*p_bytes), m_objDemodulator->cstln, m_objCfg.fec);

      if ( m_objCfg.fastlock )
      {
          r->resync_period = 1;
      }
    }
    else
    {
        r_deconv = make_deconvol_sync_simple(m_objScheduler, (*p_symbols), (*p_bytes), m_objCfg.fec);
        r_deconv->fastlock = m_objCfg.fastlock;
    }

    //******* -> if ( m_objCfg.hdlc )

    p_mpegbytes = new leansdr::pipebuf<leansdr::u8> (m_objScheduler, "mpegbytes", BUF_MPEGBYTES);
    p_lock = new leansdr::pipebuf<int> (m_objScheduler, "lock", BUF_SLOW);
    p_locktime = new leansdr::pipebuf<leansdr::u32> (m_objScheduler, "locktime", BUF_PACKETS);

    r_sync_mpeg = new leansdr::mpeg_sync<leansdr::u8, 0>(m_objScheduler, *p_bytes, *p_mpegbytes, r_deconv, p_lock, p_locktime);
    r_sync_mpeg->fastlock = m_objCfg.fastlock;

    // DEINTERLEAVING

    p_rspackets = new leansdr::pipebuf< leansdr::rspacket<leansdr::u8> >(m_objScheduler, "RS-enc packets", BUF_PACKETS);
    r_deinter = new leansdr::deinterleaver<leansdr::u8>(m_objScheduler, *p_mpegbytes, *p_rspackets);


    // REED-SOLOMON

    p_vbitcount = new leansdr::pipebuf<int>(m_objScheduler, "Bits processed", BUF_PACKETS);
    p_verrcount = new leansdr::pipebuf<int>(m_objScheduler, "Bits corrected", BUF_PACKETS);
    p_rtspackets = new leansdr::pipebuf<leansdr::tspacket>(m_objScheduler, "rand TS packets", BUF_PACKETS);
    r_rsdec = new leansdr::rs_decoder<leansdr::u8, 0> (m_objScheduler, *p_rspackets, *p_rtspackets, p_vbitcount, p_verrcount);


    // BER ESTIMATION


    /*
    p_vber = new pipebuf<float> (m_objScheduler, "VBER", BUF_SLOW);
    r_vber = new rate_estimator<float> (m_objScheduler, *p_verrcount, *p_vbitcount, *p_vber);
    r_vber->sample_size = m_objCfg.Fm/2;  // About twice per second, depending on CR
    // Require resolution better than 2E-5
    if ( r_vber->sample_size < 50000 )
    {
        r_vber->sample_size = 50000;
    }
    */

    // DERANDOMIZATION

    p_tspackets = new leansdr::pipebuf<leansdr::tspacket>(m_objScheduler, "TS packets", BUF_PACKETS);
    r_derand = new leansdr::derandomizer(m_objScheduler, *p_rtspackets, *p_tspackets);


    // OUTPUT
    r_videoplayer = new leansdr::datvvideoplayer<leansdr::tspacket>(m_objScheduler, *p_tspackets,m_objVideoStream);

    m_blnDVBInitialized=true;
}

void DATVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
    float fltI;
    float fltQ;
    leansdr::cf32 objIQ;
    //Complex objC;
    fftfilt::cmplx *objRF;
    int intRFOut;
    double magSq;

    //********** Bis repetita : Let's rock and roll buddy ! **********

#ifdef EXTENDED_DIRECT_SAMPLE

    qint16 * ptrBuffer;
    qint32 intLen;

    //********** Reading direct samples **********

    SampleVector::const_iterator it = begin;
    intLen = it->intLen;
    ptrBuffer = it->ptrBuffer;
    ptrBufferToRelease = ptrBuffer;
    ++it;

    for(qint32 intInd=0; intInd<intLen-1; intInd +=2)
    {

        fltI= ((qint32) (*ptrBuffer)) << 4;
        ptrBuffer ++;
        fltQ= ((qint32) (*ptrBuffer)) << 4;
        ptrBuffer ++;

#else

    for (SampleVector::const_iterator it = begin; it != end; ++it /* ++it **/)
    {
        fltI = it->real();
        fltQ = it->imag();
#endif


        //********** demodulation **********


        if (m_blnNeedConfigUpdate)
        {

            m_objSettingsMutex.lock();

            m_blnNeedConfigUpdate=false;

            InitDATVFramework();

            m_objSettingsMutex.unlock();

        }


        //********** iq stream ****************

        Complex objC(fltI,fltQ);

        objC *= m_objNCO.nextIQ();

        intRFOut = m_objRFFilter->runFilt(objC, &objRF); // filter RF before demod

        for (int intI = 0 ; intI < intRFOut; intI++)
        {
            objIQ.re = objRF->real();
            objIQ.im = objRF->imag();
            magSq = objIQ.re*objIQ.re + objIQ.im*objIQ.im;
            m_objMagSqAverage(magSq);

            objRF ++;

            if (m_blnDVBInitialized
               && (p_rawiq_writer!=NULL)
               && (m_objScheduler!=NULL))
            {
                p_rawiq_writer->write(objIQ);
                m_lngReadIQ++;

                //Leave +1 by safety
                if((m_lngReadIQ+1)>=p_rawiq_writer->writable())
                {
                    m_objScheduler->step();

                    m_lngReadIQ=0;
                    delete p_rawiq_writer;
                    p_rawiq_writer = new leansdr::pipewriter<leansdr::cf32>(*p_rawiq);
                }
            }

        }
    }
}

void DATVDemod::start()
{

}

void DATVDemod::stop()
{

}

bool DATVDemod::handleMessage(const Message& cmd)
{
    if (DownChannelizer::MsgChannelizerNotification::match(cmd))
    {
        DownChannelizer::MsgChannelizerNotification& objNotif = (DownChannelizer::MsgChannelizerNotification&) cmd;

        qDebug() << "DATVDemod::handleMessage: MsgChannelizerNotification:"
                << " m_intSampleRate: " << objNotif.getSampleRate()
                << " m_intFrequencyOffset: " << objNotif.getFrequencyOffset();

        if (m_objRunning.intMsps != objNotif.getSampleRate())
        {
            m_objRunning.intMsps = objNotif.getSampleRate();
            m_objRunning.intSampleRate = m_objRunning.intMsps;

            ApplySettings();
        }

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
                m_channelizer->getInputSampleRate(),
                cfg.getCenterFrequency());

        qDebug() << "DATVDemod::handleMessage: MsgConfigureChannelizer: sampleRate: " << m_channelizer->getInputSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        return true;
    }
    else if (MsgConfigureDATVDemod::match(cmd))
	{
        MsgConfigureDATVDemod& objCfg = (MsgConfigureDATVDemod&) cmd;


        if((objCfg.m_objMsgConfig.blnAllowDrift != m_objRunning.blnAllowDrift)
           || (objCfg.m_objMsgConfig.intRFBandwidth != m_objRunning.intRFBandwidth)
           || (objCfg.m_objMsgConfig.intCenterFrequency != m_objRunning.intCenterFrequency)
           || (objCfg.m_objMsgConfig.blnFastLock != m_objRunning.blnFastLock)
           || (objCfg.m_objMsgConfig.blnHardMetric != m_objRunning.blnHardMetric)
           || (objCfg.m_objMsgConfig.enmFilter != m_objRunning.enmFilter)
           || (objCfg.m_objMsgConfig.fltRollOff != m_objRunning.fltRollOff)
           || (objCfg.m_objMsgConfig.blnViterbi != m_objRunning.blnViterbi)
           || (objCfg.m_objMsgConfig.enmFEC != m_objRunning.enmFEC)
           || (objCfg.m_objMsgConfig.enmModulation != m_objRunning.enmModulation)
           || (objCfg.m_objMsgConfig.enmStandard != m_objRunning.enmStandard)
           || (objCfg.m_objMsgConfig.intNotchFilters != m_objRunning.intNotchFilters)
           || (objCfg.m_objMsgConfig.intSymbolRate != m_objRunning.intSymbolRate)
           || (objCfg.m_objMsgConfig.intExcursion != m_objRunning.intExcursion))
         {
            m_objRunning.blnAllowDrift = objCfg.m_objMsgConfig.blnAllowDrift;
            m_objRunning.blnFastLock = objCfg.m_objMsgConfig.blnFastLock;
            m_objRunning.blnHardMetric = objCfg.m_objMsgConfig.blnHardMetric;
            m_objRunning.enmFilter = objCfg.m_objMsgConfig.enmFilter;
            m_objRunning.fltRollOff = objCfg.m_objMsgConfig.fltRollOff;
            m_objRunning.blnViterbi = objCfg.m_objMsgConfig.blnViterbi;
            m_objRunning.enmFEC = objCfg.m_objMsgConfig.enmFEC;
            m_objRunning.enmModulation = objCfg.m_objMsgConfig.enmModulation;
            m_objRunning.enmStandard = objCfg.m_objMsgConfig.enmStandard;
            m_objRunning.intNotchFilters = objCfg.m_objMsgConfig.intNotchFilters;
            m_objRunning.intSymbolRate = objCfg.m_objMsgConfig.intSymbolRate;
            m_objRunning.intRFBandwidth = objCfg.m_objMsgConfig.intRFBandwidth;
            m_objRunning.intCenterFrequency = objCfg.m_objMsgConfig.intCenterFrequency;
            m_objRunning.intExcursion = objCfg.m_objMsgConfig.intExcursion;

            qDebug() << "ATVDemod::handleMessage: MsgConfigureDATVDemod:"
                    << " blnAllowDrift: " << objCfg.m_objMsgConfig.blnAllowDrift
                    << " intRFBandwidth: " << objCfg.m_objMsgConfig.intRFBandwidth
                    << " intCenterFrequency: " << objCfg.m_objMsgConfig.intCenterFrequency
                    << " blnFastLock: " << objCfg.m_objMsgConfig.blnFastLock
                    << " enmFilter: " << objCfg.m_objMsgConfig.enmFilter
                    << " fltRollOff: " << objCfg.m_objMsgConfig.fltRollOff
                    << " blnViterbi: " << objCfg.m_objMsgConfig.blnViterbi
                    << " enmFEC: " << objCfg.m_objMsgConfig.enmFEC
                    << " enmModulation: " << objCfg.m_objMsgConfig.enmModulation
                    << " enmStandard: " << objCfg.m_objMsgConfig.enmStandard
                    << " intNotchFilters: " << objCfg.m_objMsgConfig.intNotchFilters
                    << " intSymbolRate: " << objCfg.m_objMsgConfig.intSymbolRate
                    << " intRFBandwidth: " << objCfg.m_objMsgConfig.intRFBandwidth
                    << " intCenterFrequency: " << objCfg.m_objMsgConfig.intCenterFrequency
                    << " intExcursion: " << objCfg.m_objMsgConfig.intExcursion;

            ApplySettings();
        }

		return true;
	}
	else
	{
		return false;
	}
}

void DATVDemod::ApplySettings()
{
    if(m_objRunning.intMsps==0)
    {
        return;
    }

    InitDATVParameters(m_objRunning.intMsps,
                       m_objRunning.intRFBandwidth,
                       m_objRunning.intCenterFrequency,
                       m_objRunning.enmStandard,
                       m_objRunning.enmModulation,
                       m_objRunning.enmFEC,
                       m_objRunning.intSampleRate,
                       m_objRunning.intSymbolRate,
                       m_objRunning.intNotchFilters,
                       m_objRunning.blnAllowDrift,
                       m_objRunning.blnFastLock,
                       m_objRunning.enmFilter,
                       m_objRunning.blnHardMetric,
                       m_objRunning.fltRollOff,
                       m_objRunning.blnViterbi,
                       m_objRunning.intExcursion);
}

int DATVDemod::GetSampleRate()
{
    return m_objRunning.intMsps;
}

