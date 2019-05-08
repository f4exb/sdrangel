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

#include "datvdemod.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"

#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "device/deviceapi.h"

const QString DATVDemod::m_channelIdURI = "sdrangel.channel.demoddatv";
const QString DATVDemod::m_channelId = "DATVDemod";

MESSAGE_CLASS_DEFINITION(DATVDemod::MsgConfigureDATVDemod, Message)
MESSAGE_CLASS_DEFINITION(DATVDemod::MsgConfigureChannelizer, Message)

DATVDemod::DATVDemod(DeviceAPI *deviceAPI) :
    ChannelSinkAPI(m_channelIdURI),
    m_blnNeedConfigUpdate(false),
    m_deviceAPI(deviceAPI),
    m_objRegisteredTVScreen(0),
    m_objRegisteredVideoRender(0),
    m_objVideoStream(nullptr),
    m_objRenderThread(nullptr),
    m_audioFifo(48000),
    m_blnRenderingVideo(false),
    m_blnStartStopVideo(false),
    m_enmModulation(DATVDemodSettings::BPSK /*DATV_FM1*/),
    m_sampleRate(1024000),
    m_objSettingsMutex(QMutex::NonRecursive)
{
    setObjectName("DATVDemod");

	DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifo, getInputMessageQueue());
	//m_audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate();

    //*************** DATV PARAMETERS  ***************
    m_blnInitialized=false;
    CleanUpDATVFramework(false);

    m_objVideoStream = new DATVideostream();

    m_objRFFilter = new fftfilt(-256000.0 / 1024000.0, 256000.0 / 1024000.0, rfFilterFftLength);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addChannelSink(m_threadedChannelizer);
    m_deviceAPI->addChannelSinkAPI(this);
}

DATVDemod::~DATVDemod()
{
    m_blnInitialized=false;

    if(m_objVideoStream!=nullptr)
    {
        //Immediately exit from DATVideoStream if waiting for data before killing thread
        m_objVideoStream->ThreadTimeOut=0;
    }

    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);

    if(m_objRenderThread!=nullptr)
    {
        if(m_objRenderThread->isRunning())
        {
           m_objRenderThread->stopRendering();
           m_objRenderThread->quit();
        }

        m_objRenderThread->wait(2000);
    }

    CleanUpDATVFramework(true);

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete m_objRFFilter;
}

bool DATVDemod::SetTVScreen(TVScreen *objScreen)
{
    m_objRegisteredTVScreen = objScreen;
    return true;
}

DATVideostream *DATVDemod::SetVideoRender(DATVideoRender *objScreen)
{
    m_objRegisteredVideoRender = objScreen;
    m_objRegisteredVideoRender->setAudioFIFO(&m_audioFifo);
    m_objRenderThread = new DATVideoRenderThread(m_objRegisteredVideoRender, m_objVideoStream);
    return m_objVideoStream;
}

bool DATVDemod::audioActive()
{
    if (m_objRegisteredVideoRender) {
        return m_objRegisteredVideoRender->getAudioStreamIndex() >= 0;
    } else {
        return false;
    }
}

bool DATVDemod::videoActive()
{
    if (m_objRegisteredVideoRender) {
        return m_objRegisteredVideoRender->getVideoStreamIndex() >= 0;
    } else {
        return false;
    }
}

bool DATVDemod::audioDecodeOK()
{
    if (m_objRegisteredVideoRender) {
        return m_objRegisteredVideoRender->getAudioDecodeOK();
    } else {
        return false;
    }
}

bool DATVDemod::videoDecodeOK()
{
    if (m_objRegisteredVideoRender) {
        return m_objRegisteredVideoRender->getVideoDecodeOK();
    } else {
        return false;
    }
}

bool DATVDemod::PlayVideo(bool blnStartStop)
{

    if (m_objVideoStream == nullptr) {
        return false;
    }

    if (m_objRegisteredVideoRender == nullptr) {
        return false;
    }

    if (m_objRenderThread == nullptr) {
        return false;
    }

    if (m_blnStartStopVideo && !blnStartStop) {
        return true;
    }

    if (blnStartStop == true) {
        m_blnStartStopVideo = true;
    }

    if (m_objRenderThread->isRunning())
    {
        if (blnStartStop == true) {
            m_objRenderThread->stopRendering();
        }

        return true;
    }

    if (m_objVideoStream->bytesAvailable() > 0)
    {
        m_objRenderThread->setStreamAndRenderer(m_objRegisteredVideoRender, m_objVideoStream);
        m_objVideoStream->MultiThreaded = true;
        m_objVideoStream->ThreadTimeOut = 5000; //5000 ms
        m_objRenderThread->start();
    }

    return true;
}

void DATVDemod::CleanUpDATVFramework(bool blnRelease)
{
    if (blnRelease == true)
    {
        if (m_objScheduler != nullptr)
        {
            m_objScheduler->shutdown();
            delete m_objScheduler;
        }

        // NOTCH FILTER

        if (r_auto_notch != nullptr) {
            delete r_auto_notch;
        }
        if (p_autonotched != nullptr) {
            delete p_autonotched;
        }

        // FREQUENCY CORRECTION : DEROTATOR
        if (p_derot != nullptr) {
            delete p_derot;
        }
        if (r_derot != nullptr) {
            delete r_derot;
        }

        // CNR ESTIMATION
        if (p_cnr != nullptr) {
            delete p_cnr;
        }
        if (r_cnr != nullptr) {
            delete r_cnr;
        }

        //FILTERING
        if (r_resample != nullptr) {
            delete r_resample;
        }
        if (p_resampled != nullptr) {
            delete p_resampled;
        }
        if (coeffs != nullptr) {
            delete coeffs;
        }

        // OUTPUT PREPROCESSED DATA
        if (sampler != nullptr) {
            delete sampler;
        }
        if (coeffs_sampler != nullptr) {
            delete coeffs_sampler;
        }
        if (p_symbols != nullptr) {
            delete p_symbols;
        }
        if (p_freq != nullptr) {
            delete p_freq;
        }
        if (p_ss != nullptr) {
            delete p_ss;
        }
        if (p_mer != nullptr) {
            delete p_mer;
        }
        if (p_sampled != nullptr) {
            delete p_sampled;
        }

        //DECIMATION
        if (p_decimated != nullptr) {
            delete p_decimated;
        }
        if (p_decim != nullptr) {
            delete p_decim;
        }
        if (r_ppout != nullptr) {
            delete r_ppout;
        }

        //GENERIC CONSTELLATION RECEIVER
        if (m_objDemodulator != nullptr) {
            delete m_objDemodulator;
        }

        //DECONVOLUTION AND SYNCHRONIZATION
        if (p_bytes != nullptr) {
            delete p_bytes;
        }
        if (r_deconv != nullptr) {
            delete r_deconv;
        }
        if (r != nullptr) {
            delete r;
        }
        if (p_descrambled != nullptr) {
            delete p_descrambled;
        }
        if (p_frames != nullptr) {
            delete p_frames;
        }
        if (r_etr192_descrambler != nullptr) {
            delete r_etr192_descrambler;
        }
        if (r_sync != nullptr) {
            delete r_sync;
        }
        if (p_mpegbytes != nullptr) {
            delete p_mpegbytes;
        }
        if (p_lock != nullptr) {
            delete p_lock;
        }
        if (p_locktime != nullptr) {
            delete p_locktime;
        }
        if (r_sync_mpeg != nullptr) {
            delete r_sync_mpeg;
        }

        // DEINTERLEAVING
        if (p_rspackets != nullptr) {
            delete p_rspackets;
        }
        if (r_deinter != nullptr) {
            delete r_deinter;
        }
        if (p_vbitcount != nullptr) {
            delete p_vbitcount;
        }
        if (p_verrcount != nullptr) {
            delete p_verrcount;
        }
        if (p_rtspackets != nullptr) {
            delete p_rtspackets;
        }
        if (r_rsdec != nullptr) {
            delete r_rsdec;
        }

        //BER ESTIMATION
        if (p_vber != nullptr) {
            delete p_vber;
        }
        if (r_vber != nullptr) {
            delete r_vber;
        }

        // DERANDOMIZATION
        if (p_tspackets != nullptr) {
            delete p_tspackets;
        }
        if (r_derand != nullptr) {
            delete r_derand;
        }

        //OUTPUT : To remove
        if (r_stdout != nullptr) {
            delete r_stdout;
        }
        if (r_videoplayer != nullptr) {
            delete r_videoplayer;
        }

        //CONSTELLATION
        if (r_scope_symbols != nullptr) {
            delete r_scope_symbols;
        }

        // INPUT
        //if(p_rawiq!=nullptr) delete p_rawiq;
        //if(p_rawiq_writer!=nullptr) delete p_rawiq_writer;
        //if(p_preprocessed!=nullptr) delete p_preprocessed;
    }

    m_objScheduler=nullptr;

    // INPUT

    p_rawiq = nullptr;
    p_rawiq_writer = nullptr;

    p_preprocessed = nullptr;

    // NOTCH FILTER
    r_auto_notch = nullptr;
    p_autonotched = nullptr;

    // FREQUENCY CORRECTION : DEROTATOR
    p_derot = nullptr;
    r_derot=nullptr;

    // CNR ESTIMATION
    p_cnr = nullptr;
    r_cnr = nullptr;

    //FILTERING
    r_resample = nullptr;
    p_resampled = nullptr;
    coeffs = nullptr;
    ncoeffs=0;

    // OUTPUT PREPROCESSED DATA
    sampler = nullptr;
    coeffs_sampler=nullptr;
    ncoeffs_sampler=0;

    p_symbols = nullptr;
    p_freq = nullptr;
    p_ss = nullptr;
    p_mer = nullptr;
    p_sampled = nullptr;

    //DECIMATION
    p_decimated = nullptr;
    p_decim = nullptr;
    r_ppout = nullptr;

    //GENERIC CONSTELLATION RECEIVER
    m_objDemodulator = nullptr;

    //DECONVOLUTION AND SYNCHRONIZATION
    p_bytes=nullptr;
    r_deconv=nullptr;
    r = nullptr;

    p_descrambled = nullptr;
    p_frames = nullptr;
    r_etr192_descrambler = nullptr;
    r_sync = nullptr;

    p_mpegbytes = nullptr;
    p_lock = nullptr;
    p_locktime = nullptr;
    r_sync_mpeg = nullptr;


    // DEINTERLEAVING
    p_rspackets = nullptr;
    r_deinter = nullptr;

    p_vbitcount = nullptr;
    p_verrcount = nullptr;
    p_rtspackets = nullptr;
    r_rsdec = nullptr;


    //BER ESTIMATION
    p_vber = nullptr;
    r_vber  = nullptr;


    // DERANDOMIZATION
    p_tspackets = nullptr;
    r_derand = nullptr;


    //OUTPUT : To remove
    r_stdout = nullptr;
    r_videoplayer = nullptr;


    //CONSTELLATION
    r_scope_symbols = nullptr;
}

void DATVDemod::InitDATVFramework()
{
    m_blnDVBInitialized = false;
    m_lngReadIQ = 0;
    CleanUpDATVFramework(false);

    qDebug()  << "DATVDemod::InitDATVFramework:"
        <<  " Symbol Rate: " << m_settings.m_symbolRate
        <<  " Modulation: " << m_settings.m_modulation
        <<  " Notch Filters: " << m_settings.m_notchFilters
        <<  " Allow Drift: " << m_settings.m_allowDrift
        <<  " Fast Lock: " << m_settings.m_fastLock
        <<  " Filter: " << m_settings.m_filter
        <<  " HARD METRIC: " << m_settings.m_hardMetric
        <<  " RollOff: " << m_settings.m_rollOff
        <<  " Viterbi: " << m_settings.m_viterbi
        <<  " Excursion: " << m_settings.m_excursion;

    m_objCfg.standard = m_settings.m_standard;

    m_objCfg.fec = m_settings.m_fec;
    m_objCfg.Fs = (float) m_sampleRate;
    m_objCfg.Fm = (float) m_settings.m_symbolRate;
    m_objCfg.fastlock = m_settings.m_fastLock;

    m_objCfg.sampler = m_settings.m_filter;
    m_objCfg.rolloff = m_settings.m_rollOff;  //0...1
    m_objCfg.rrc_rej = (float) m_settings.m_excursion;  //dB
    m_objCfg.rrc_steps = 0; //auto

    switch(m_settings.m_modulation)
    {
        case DATVDemodSettings::BPSK:
           m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::BPSK;
           break;
        case DATVDemodSettings::QPSK:
            m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::QPSK;
            break;
        case DATVDemodSettings::PSK8:
            m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::PSK8;
            break;
        case DATVDemodSettings::APSK16:
            m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::APSK16;
            break;
        case DATVDemodSettings::APSK32:
           m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::APSK32;
           break;
        case DATVDemodSettings::APSK64E:
           m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::APSK64E;
           break;
        case DATVDemodSettings::QAM16:
           m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::QAM16;
           break;
        case DATVDemodSettings::QAM64:
           m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::QAM64;
           break;
        case DATVDemodSettings::QAM256:
           m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::QAM256;
           break;
        default:
           m_objCfg.constellation = leansdr::cstln_lut<leansdr::eucl_ss, 256>::BPSK;
           break;
    }

    m_objCfg.allow_drift = m_settings.m_allowDrift;
    m_objCfg.anf = m_settings.m_notchFilters;
    m_objCfg.hard_metric = m_settings.m_hardMetric;
    m_objCfg.sampler = m_settings.m_filter;
    m_objCfg.viterbi = m_settings.m_viterbi;

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

    if (m_objCfg.anf>0)
    {
        p_autonotched = new leansdr::pipebuf<leansdr::cf32>(m_objScheduler, "autonotched", BUF_BASEBAND);
        r_auto_notch = new leansdr::auto_notch<leansdr::f32>(m_objScheduler, *p_preprocessed, *p_autonotched, m_objCfg.anf, 0);
        p_preprocessed = p_autonotched;
    }


    // FREQUENCY CORRECTION

    //******** -> if ( m_objCfg.Fderot>0 )

    // CNR ESTIMATION

    p_cnr = new leansdr::pipebuf<leansdr::f32>(m_objScheduler, "cnr", BUF_SLOW);

    if (m_objCfg.cnr == true)
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

    p_symbols = new leansdr::pipebuf<leansdr::eucl_ss>(m_objScheduler, "PSK soft-symbols", BUF_SYMBOLS);
    p_freq = new leansdr::pipebuf<leansdr::f32> (m_objScheduler, "freq", BUF_SLOW);
    p_ss = new leansdr::pipebuf<leansdr::f32> (m_objScheduler, "SS", BUF_SLOW);
    p_mer = new leansdr::pipebuf<leansdr::f32> (m_objScheduler, "MER", BUF_SLOW);
    p_sampled = new leansdr::pipebuf<leansdr::cf32> (m_objScheduler, "PSK symbols", BUF_BASEBAND);

    switch (m_objCfg.sampler)
    {
        case DATVDemodSettings::SAMP_NEAREST:
          sampler = new leansdr::nearest_sampler<float>();
          break;
        case DATVDemodSettings::SAMP_LINEAR:
          sampler = new leansdr::linear_sampler<float>();
          break;
        case DATVDemodSettings::SAMP_RRC:
        {
          if (m_objCfg.rrc_steps == 0)
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

    m_objDemodulator = new leansdr::cstln_receiver<leansdr::f32, leansdr::eucl_ss>(
            m_objScheduler,
            sampler,
            *p_preprocessed,
            *p_symbols,
            p_freq,
            p_ss,
            p_mer,
            p_sampled);

    if (m_objCfg.standard == DATVDemodSettings::DVB_S)
    {
        if ( m_objCfg.constellation != leansdr::cstln_lut<leansdr::eucl_ss, 256>::QPSK
            && m_objCfg.constellation != leansdr::cstln_lut<leansdr::eucl_ss, 256>::BPSK )
        {
            qWarning("DATVDemod::InitDATVFramework: non-standard constellation for DVB-S");
        }
    }

    if (m_objCfg.standard == DATVDemodSettings::DVB_S2)
    {
        // For DVB-S2 testing only.
        // Constellation should be determined from PL signalling.
        qDebug("DATVDemod::InitDATVFramework: DVB-S2: Testing symbol sampler only.");
    }

    m_objDemodulator->cstln = make_dvbs2_constellation(m_objCfg.constellation, m_objCfg.fec);

    if (m_objCfg.hard_metric) {
        m_objDemodulator->cstln->harden();
    }

    m_objDemodulator->set_omega(m_objCfg.Fs/m_objCfg.Fm);

    //******** if ( m_objCfg.Ftune )
    //{
    //  m_objDemodulator->set_freq(m_objCfg.Ftune/m_objCfg.Fs);
    //}

    if (m_objCfg.allow_drift) {
        m_objDemodulator->set_allow_drift(true);
    }

    //******** -> if ( m_objCfg.viterbi )
    if (m_objCfg.viterbi) {
        m_objDemodulator->pll_adjustment /= 6;
    }

    m_objDemodulator->meas_decimation = decimation(m_objCfg.Fs, m_objCfg.Finfo);

    // TRACKING FILTERS

    if (r_cnr)
    {
        r_cnr->freq_tap = &m_objDemodulator->freq_tap;
        r_cnr->tap_multiplier = 1.0 / decim;
    }

    //constellation

    if (m_objRegisteredTVScreen)
    {
        m_objRegisteredTVScreen->resizeTVScreen(256,256);
        r_scope_symbols = new leansdr::datvconstellation<leansdr::f32>(m_objScheduler, *p_sampled, -128,128, nullptr, m_objRegisteredTVScreen);
        r_scope_symbols->decimation = 1;
        r_scope_symbols->cstln = &m_objDemodulator->cstln;
        r_scope_symbols->calculate_cstln_points();
    }

    // DECONVOLUTION AND SYNCHRONIZATION

    p_bytes = new leansdr::pipebuf<leansdr::u8>(m_objScheduler, "bytes", BUF_BYTES);

    r_deconv = nullptr;

    //******** -> if ( m_objCfg.viterbi )

    if (m_objCfg.viterbi)
    {
        if (m_objCfg.fec == leansdr::FEC23 && (m_objDemodulator->cstln->nsymbols == 4 || m_objDemodulator->cstln->nsymbols == 64)) {
            m_objCfg.fec = leansdr::FEC46;
        }

        //To uncomment -> Linking Problem : undefined symbol: _ZN7leansdr21viterbi_dec_interfaceIhhiiE6updateEPiS2_
        r = new leansdr::viterbi_sync(m_objScheduler, (*p_symbols), (*p_bytes), m_objDemodulator->cstln, m_objCfg.fec);

        if (m_objCfg.fastlock) {
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

    p_rspackets = new leansdr::pipebuf<leansdr::rspacket<leansdr::u8> >(m_objScheduler, "RS-enc packets", BUF_PACKETS);
    r_deinter = new leansdr::deinterleaver<leansdr::u8>(m_objScheduler, *p_mpegbytes, *p_rspackets);

    // REED-SOLOMON

    p_vbitcount = new leansdr::pipebuf<int>(m_objScheduler, "Bits processed", BUF_PACKETS);
    p_verrcount = new leansdr::pipebuf<int>(m_objScheduler, "Bits corrected", BUF_PACKETS);
    p_rtspackets = new leansdr::pipebuf<leansdr::tspacket>(m_objScheduler, "rand TS packets", BUF_PACKETS);
    r_rsdec = new leansdr::rs_decoder<leansdr::u8, 0>(m_objScheduler, *p_rspackets, *p_rtspackets, p_vbitcount, p_verrcount);

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
    r_videoplayer = new leansdr::datvvideoplayer<leansdr::tspacket>(m_objScheduler, *p_tspackets, m_objVideoStream);

    m_blnDVBInitialized = true;
}

void DATVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
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
               && (p_rawiq_writer!=nullptr)
               && (m_objScheduler!=nullptr))
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
    m_audioFifo.clear();
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
        applyChannelSettings(objNotif.getSampleRate(), objNotif.getFrequencyOffset());

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
                m_channelizer->getInputSampleRate(), // do not change sample rate
                cfg.getCenterFrequency());

        qDebug() << "DATVDemod::handleMessage: MsgConfigureChannelizer: sampleRate: " << m_channelizer->getInputSampleRate()
            << " centerFrequency: " << cfg.getCenterFrequency();

        return true;
    }
    else if (MsgConfigureDATVDemod::match(cmd))
	{
        MsgConfigureDATVDemod& objCfg = (MsgConfigureDATVDemod&) cmd;
        qDebug() << "DATVDemod::handleMessage: MsgConfigureDATVDemod";
        applySettings(objCfg.getSettings(), objCfg.getForce());

        return true;
	}
	else
	{
		return false;
	}
}

void DATVDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "DATVDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((m_settings.m_centerFrequency != inputFrequencyOffset) ||
        (m_sampleRate != inputSampleRate) || force)
    {
        m_objNCO.setFreq(-(float) inputFrequencyOffset, (float) inputSampleRate);
    }

    if ((m_sampleRate != inputSampleRate) || force)
    {
        m_objSettingsMutex.lock();
        //Bandpass filter shaping
        Real fltLowCut = -((float) m_settings.m_rfBandwidth / 2.0) / (float) inputSampleRate;
        Real fltHiCut  = ((float) m_settings.m_rfBandwidth / 2.0) / (float) inputSampleRate;
        m_objRFFilter->create_filter(fltLowCut, fltHiCut);
        m_blnNeedConfigUpdate = true;
        m_objSettingsMutex.unlock();
    }

    m_sampleRate = inputSampleRate;
    m_settings.m_centerFrequency = inputFrequencyOffset;
}

void DATVDemod::applySettings(const DATVDemodSettings& settings, bool force)
{
    QString msg = tr("DATVDemod::applySettings: force: %1").arg(force);
    settings.debug(msg);

    if (m_sampleRate == 0) {
        return;
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->addAudioSink(&m_audioFifo, getInputMessageQueue(), audioDeviceIndex); // removes from current if necessary
        // uint32_t audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

        // if (m_audioSampleRate != audioSampleRate) {
        //     applyAudioSampleRate(audioSampleRate);
        // }
    }

    if ((settings.m_audioVolume) != (m_settings.m_audioVolume) || force)
    {
        if (m_objRegisteredVideoRender) {
            m_objRegisteredVideoRender->setAudioVolume(settings.m_audioVolume);
        }
    }

    if ((settings.m_audioMute) != (m_settings.m_audioMute) || force)
    {
        if (m_objRegisteredVideoRender) {
            m_objRegisteredVideoRender->setAudioMute(settings.m_audioMute);
        }
    }

    if ((settings.m_videoMute) != (m_settings.m_videoMute) || force)
    {
        if (m_objRegisteredVideoRender) {
            m_objRegisteredVideoRender->setVideoMute(settings.m_videoMute);
        }
    }

    if (m_settings.isDifferent(settings) || force)
    {
        m_objSettingsMutex.lock();

        if ((m_settings.m_rfBandwidth != settings.m_rfBandwidth)
            || force)
        {

            //Bandpass filter shaping
            Real fltLowCut = -((float) settings.m_rfBandwidth / 2.0) / (float) m_sampleRate;
            Real fltHiCut  = ((float) settings.m_rfBandwidth / 2.0) / (float) m_sampleRate;
            m_objRFFilter->create_filter(fltLowCut, fltHiCut);
        }

        if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
            || force)
        {
            m_objNCO.setFreq(-(float) settings.m_centerFrequency, (float) m_sampleRate);
        }

        m_objSettingsMutex.unlock();
        m_blnNeedConfigUpdate = true;
    }

    m_settings = settings;
}

int DATVDemod::GetSampleRate()
{
    return m_sampleRate;
}
