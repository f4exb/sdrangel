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
        m_objRegisteredDATVScreen(0),
        m_objRegisteredVideoRender(0),
        m_objVideoStream(0),
        m_objRenderThread(0),
        m_blnRenderingVideo(false),
        m_enmModulation(BPSK /*DATV_FM1*/),
        m_objSettingsMutex(QMutex::NonRecursive)
{
    setObjectName("DATVDemod");
    qDebug("DATVDemod::DATVDemod: sizeof FixReal: %lu: SDR_RX_SAMP_SZ: %u", sizeof(FixReal), (unsigned int) SDR_RX_SAMP_SZ);

    //*************** DATV PARAMETERS  ***************
    m_blnInitialized = false;
    CleanUpDATVFramework();

    m_objVideoStream = new DATVideostream();

    m_objRFFilter = new fftfilt(-256000.0 / 1024000.0, 256000.0 / 1024000.0, rfFilterFftLength);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    connect(m_channelizer, SIGNAL(inputSampleRateChanged()), this, SLOT(channelSampleRateChanged()));
}

DATVDemod::~DATVDemod()
{
    m_blnInitialized = false;

    if (m_objRenderThread != NULL)
    {
        if (m_objRenderThread->isRunning())
        {
            m_objRenderThread->stopRendering();
        }
    }

    //CleanUpDATVFramework(true);

    if (m_objRFFilter != NULL)
    {
        //delete m_objRFFilter;
    }

    if (m_objVideoStream != NULL)
    {
        //m_objVideoStream->close();
        //delete m_objVideoStream;
    }

    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

bool DATVDemod::SetDATVScreen(DATVScreen *objScreen)
{
    m_objRegisteredDATVScreen = objScreen;
    return true;
}

DATVideostream * DATVDemod::SetVideoRender(DATVideoRender *objScreen)
{
    m_objRegisteredVideoRender = objScreen;

    m_objRenderThread = new DATVideoRenderThread(m_objRegisteredVideoRender, m_objVideoStream);

    return m_objVideoStream;
}

bool DATVDemod::PlayVideo(bool blnStartStop)
{

    if (m_objVideoStream == NULL)
    {
        return false;
    }

    if (m_objRegisteredVideoRender == NULL)
    {
        return false;
    }

    if (m_objRenderThread == NULL)
    {
        return false;
    }

    if (m_objRenderThread->isRunning())
    {
        if (blnStartStop == true)
        {
            m_objRenderThread->stopRendering();
        }

        return true;
    }

    m_objRenderThread->setStreamAndRenderer(m_objRegisteredVideoRender, m_objVideoStream);
    m_objVideoStream->MultiThreaded = true;
    m_objRenderThread->start();

    //m_objVideoStream->MultiThreaded=false;
    //m_objRenderThread->run();

    return true;
}

void DATVDemod::configure(
        MessageQueue* objMessageQueue,
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
        bool blnViterbi)
{
    Message* msgCmd = MsgConfigureDATVDemod::create(
            intRFBandwidth,
            intCenterFrequency,
            enmStandard,
            enmModulation,
            enmFEC,
            intSymbolRate,
            intNotchFilters,
            blnAllowDrift,
            blnFastLock,
            blnHDLC,
            blnHardMetric,
            blnResample,
            blnViterbi);
    objMessageQueue->push(msgCmd);
}

void DATVDemod::InitDATVParameters(
        int intMsps,
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
        bool blnViterbi)
{
    Real fltLowCut;
    Real fltHiCut;

    m_blnInitialized = false;

    m_objSettingsMutex.lock();

    //Recalibrage du filtre passe bande

    fltLowCut = -((float) intRFBandwidth / 2.0) / (float) intMsps;
    fltHiCut = ((float) intRFBandwidth / 2.0) / (float) intMsps;
    m_objRFFilter->create_filter(fltLowCut, fltHiCut);
    m_objNCO.setFreq(-(float) intCenterFrequency, (float) intMsps);

    //Mise à jour de la config

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
    m_objRunning.blnHDLC = blnHDLC;
    m_objRunning.blnHardMetric = blnHardMetric;
    m_objRunning.blnResample = blnResample;
    m_objRunning.blnViterbi = blnViterbi;

    qDebug() << "DATVDemod::InitDATVParameters:"
            << " - Msps: " << intMsps
            << " - Sample Rate: " << intSampleRate
            << " - Symbol Rate: " << intSymbolRate
            << " - Modulation: " << enmModulation
            << " - Notch Filters: " << intNotchFilters
            << " - Allow Drift: " << blnAllowDrift
            << " - Fast Lock: " << blnFastLock
            << " - HDLC: " << blnHDLC
            << " - HARD METRIC: " << blnHardMetric
            << " - Resample: " << blnResample
            << " - Viterbi: " << blnViterbi;

    m_objSettingsMutex.unlock();

    m_blnNeedConfigUpdate = true;

    m_blnInitialized = true;
}

void DATVDemod::CleanUpDATVFramework()
{
    //if(blnRelease==true)
//    if (false)
//    {
//        if(m_objScheduler!=NULL)
//        {
//            m_objScheduler->shutdown();
//            delete m_objScheduler;
//        }
//
//        // INPUT
//        if(p_rawiq!=NULL) delete p_rawiq;
//        if(p_rawiq_writer!=NULL) delete p_rawiq_writer;
//        if(p_preprocessed!=NULL) delete p_preprocessed;
//
//        // NOTCH FILTER
//        if(r_auto_notch!=NULL) delete r_auto_notch;
//        if(p_autonotched!=NULL) delete p_autonotched;
//
//        // FREQUENCY CORRECTION : DEROTATOR
//        if(p_derot!=NULL) delete p_derot;
//        if(r_derot!=NULL) delete r_derot;
//
//        // CNR ESTIMATION
//        if(p_cnr!=NULL) delete p_cnr;
//        if(r_cnr!=NULL) delete r_cnr;
//
//        //FILTERING
//        if(r_resample!=NULL) delete r_resample;
//        if(p_resampled!=NULL) delete p_resampled;
//        if(coeffs!=NULL) delete coeffs;
//
//        // OUTPUT PREPROCESSED DATA
//        if(sampler!=NULL) delete sampler;
//        if(coeffs_sampler!=NULL) delete coeffs_sampler;
//        if(p_symbols!=NULL) delete p_symbols;
//        if(p_freq!=NULL) delete p_freq;
//        if(p_ss!=NULL) delete p_ss;
//        if(p_mer!=NULL) delete p_mer;
//        if(p_sampled!=NULL) delete p_sampled;
//
//        //DECIMATION
//        if(p_decimated!=NULL) delete p_decimated;
//        if(p_decim!=NULL) delete p_decim;
//        if(r_ppout!=NULL) delete r_ppout;
//
//        //GENERIC CONSTELLATION RECEIVER
//        if(m_objDemodulator!=NULL) delete m_objDemodulator;
//
//        //DECONVOLUTION AND SYNCHRONIZATION
//        if(p_bytes!=NULL) delete p_bytes;
//        if(r_deconv!=NULL) delete r_deconv;
//        if(r!=NULL) delete r;
//        if(p_descrambled!=NULL) delete p_descrambled;
//        if(p_frames!=NULL) delete p_frames;
//        if(r_etr192_descrambler!=NULL) delete r_etr192_descrambler;
//        if(r_sync!=NULL) delete r_sync;
//        if(p_mpegbytes!=NULL) delete p_mpegbytes;
//        if(p_lock!=NULL) delete p_lock;
//        if(p_locktime!=NULL) delete p_locktime;
//        if(r_sync_mpeg!=NULL) delete r_sync_mpeg;
//
//
//        // DEINTERLEAVING
//        if(p_rspackets!=NULL) delete p_rspackets;
//        if(r_deinter!=NULL) delete r_deinter;
//        if(p_vbitcount!=NULL) delete p_vbitcount;
//        if(p_verrcount!=NULL) delete p_verrcount;
//        if(p_rtspackets!=NULL) delete p_rtspackets;
//        if(r_rsdec!=NULL) delete r_rsdec;
//
//        //BER ESTIMATION
//        if(p_vber!=NULL) delete p_vber;
//        if(r_vber!=NULL) delete r_vber;
//
//        // DERANDOMIZATION
//        if(p_tspackets!=NULL) delete p_tspackets;
//        if(r_derand!=NULL) delete r_derand;
//
//
//        //OUTPUT : To remove
//        if(r_stdout!=NULL) delete r_stdout;
//        if(r_videoplayer!=NULL) delete r_videoplayer;
//
//        //CONSTELLATION
//        if(r_scope_symbols!=NULL) delete r_scope_symbols;
//
//    }

    m_objScheduler = 0;

    // INPUT

    p_rawiq = 0;
    p_rawiq_writer = 0;

    p_preprocessed = 0;

    // NOTCH FILTER
    r_auto_notch = 0;
    p_autonotched = 0;

    // FREQUENCY CORRECTION : DEROTATOR
    p_derot = 0;
    r_derot = 0;

    // CNR ESTIMATION
    p_cnr = 0;
    r_cnr = 0;

    //FILTERING
    r_resample = 0;
    p_resampled = 0;
    coeffs = 0;
    ncoeffs = 0;

    // OUTPUT PREPROCESSED DATA
    sampler = 0;
    coeffs_sampler = 0;
    ncoeffs_sampler = 0;

    p_symbols = 0;
    p_freq = 0;
    p_ss = 0;
    p_mer = 0;
    p_sampled = 0;

    //DECIMATION
    p_decimated = 0;
    p_decim = 0;
    r_ppout = 0;

    //GENERIC CONSTELLATION RECEIVER
    m_objDemodulator = 0;

    //DECONVOLUTION AND SYNCHRONIZATION
    p_bytes = 0;
    r_deconv = 0;
    r = 0;

    p_descrambled = 0;
    p_frames = 0;
    r_etr192_descrambler = 0;
    r_sync = 0;

    p_mpegbytes = 0;
    p_lock = 0;
    p_locktime = 0;
    r_sync_mpeg = 0;

    // DEINTERLEAVING
    p_rspackets = 0;
    r_deinter = 0;

    p_vbitcount = 0;
    p_verrcount = 0;
    p_rtspackets = 0;
    r_rsdec = 0;

    //BER ESTIMATION
    p_vber = 0;
    r_vber = 0;

    // DERANDOMIZATION
    p_tspackets = 0;
    r_derand = 0;

    //OUTPUT : To remove
    r_stdout = 0;
    r_videoplayer = 0;

    //CONSTELLATION
    r_scope_symbols = 0;
}

void DATVDemod::InitDATVFramework()
{
    m_blnDVBInitialized = false;
    m_lngReadIQ = 0;

    m_objCfg.standard = m_objRunning.enmStandard;

    m_objCfg.fec = m_objRunning.enmFEC;
    m_objCfg.Fs = (float) m_objRunning.intSampleRate;
    m_objCfg.Fm = (float) m_objRunning.intSymbolRate;
    m_objCfg.fastlock = m_objRunning.blnFastLock;

    switch (m_objRunning.enmModulation)
    {
    case BPSK:
        m_objCfg.constellation = cstln_lut<256>::BPSK;
        break;

    case QPSK:
        m_objCfg.constellation = cstln_lut<256>::QPSK;
        break;

    case PSK8:
        m_objCfg.constellation = cstln_lut<256>::PSK8;
        break;

    case APSK16:
        m_objCfg.constellation = cstln_lut<256>::APSK16;
        break;

    case APSK32:
        m_objCfg.constellation = cstln_lut<256>::APSK32;
        break;

    case APSK64E:
        m_objCfg.constellation = cstln_lut<256>::APSK64E;
        break;

    case QAM16:
        m_objCfg.constellation = cstln_lut<256>::QAM16;
        break;

    case QAM64:
        m_objCfg.constellation = cstln_lut<256>::QAM64;
        break;

    case QAM256:
        m_objCfg.constellation = cstln_lut<256>::QAM256;
        break;

    default:
        m_objCfg.constellation = cstln_lut<256>::BPSK;
        break;
    }

    m_objCfg.allow_drift = m_objRunning.blnAllowDrift;
    m_objCfg.anf = m_objRunning.intNotchFilters;
    m_objCfg.hard_metric = m_objRunning.blnHardMetric;
    m_objCfg.hdlc = m_objRunning.blnHDLC;
    m_objCfg.resample = m_objRunning.blnResample;
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

    m_lngExpectedReadIQ = BUF_BASEBAND;

    CleanUpDATVFramework();

    m_objScheduler = new scheduler();

    //***************
    p_rawiq = new pipebuf<cf32>(m_objScheduler, "rawiq", BUF_BASEBAND);
    p_rawiq_writer = new pipewriter<cf32>(*p_rawiq);
    p_preprocessed = p_rawiq;

    // NOTCH FILTER

    if (m_objCfg.anf)
    {
        p_autonotched = new pipebuf<cf32>(m_objScheduler, "autonotched", BUF_BASEBAND);
        r_auto_notch = new auto_notch<f32>(m_objScheduler, *p_preprocessed, *p_autonotched, m_objCfg.anf, 0);
        p_preprocessed = p_autonotched;
    }

    // FREQUENCY CORRECTION

    if (m_objCfg.Fderot)
    {
        p_derot = new pipebuf<cf32>(m_objScheduler, "derotated", BUF_BASEBAND);
        r_derot = new rotator<f32>(m_objScheduler, *p_preprocessed, *p_derot, -m_objCfg.Fderot / m_objCfg.Fs);
        p_preprocessed = p_derot;
    }

    // CNR ESTIMATION

    p_cnr = new pipebuf<f32>(m_objScheduler, "cnr", BUF_SLOW);

    if (m_objCfg.cnr)
    {
        r_cnr = new cnr_fft<f32>(m_objScheduler, *p_preprocessed, *p_cnr, m_objCfg.Fm / m_objCfg.Fs);
        r_cnr->decimation = decimation(m_objCfg.Fs, 1);  // 1 Hz
    }

    // FILTERING

    int decim = 1;

    if (m_objCfg.resample)
    {
        // Lowpass-filter and decimate.
        if (m_objCfg.decim)
        {
            decim = m_objCfg.decim;
        }
        else
        {
            // Decimate to just above 4 samples per symbol
            float target_Fs = m_objCfg.Fm * 4;
            decim = m_objCfg.Fs / target_Fs;
            if (decim < 1)
            {
                decim = 1;
            }
        }

        float transition = (m_objCfg.Fm / 2) * m_objCfg.rolloff;
        int order = m_objCfg.resample_rej * m_objCfg.Fs / (22 * transition);
        order = ((order + 1) / 2) * 2;  // Make even

        p_resampled = new pipebuf<cf32>(m_objScheduler, "resampled", BUF_BASEBAND);

#if 1  // Cut in middle of roll-off region
        float Fcut = (m_objCfg.Fm / 2) * (1 + m_objCfg.rolloff / 2) / m_objCfg.Fs;
#else  // Cut at beginning of roll-off region
        float Fcut = (m_objCfg.Fm/2) / cfg.Fs;
#endif

        ncoeffs = filtergen::lowpass(order, Fcut, &coeffs);

        filtergen::normalize_dcgain(ncoeffs, coeffs, 1);

        r_resample = new fir_filter<cf32, float>(m_objScheduler, ncoeffs, coeffs, *p_preprocessed, *p_resampled, decim);
        p_preprocessed = p_resampled;
        m_objCfg.Fs /= decim;
    }

    // DECIMATION
    // (Unless already done in resampler)

    if (!m_objCfg.resample && m_objCfg.decim > 1)
    {
        decim = m_objCfg.decim;

        p_decimated = new pipebuf<cf32>(m_objScheduler, "decimated", BUF_BASEBAND);
        p_decim = new decimator<cf32>(m_objScheduler, decim, *p_preprocessed, *p_decimated);
        p_preprocessed = p_decimated;
        m_objCfg.Fs /= decim;
    }

    //Resampling FS

    // Generic constellation receiver

    p_symbols = new pipebuf<softsymbol>(m_objScheduler, "PSK soft-symbols", BUF_SYMBOLS);
    p_freq = new pipebuf<f32>(m_objScheduler, "freq", BUF_SLOW);
    p_ss = new pipebuf<f32>(m_objScheduler, "SS", BUF_SLOW);
    p_mer = new pipebuf<f32>(m_objScheduler, "MER", BUF_SLOW);
    p_sampled = new pipebuf<cf32>(m_objScheduler, "PSK symbols", BUF_BASEBAND);

    switch (m_objCfg.sampler)
    {
    case SAMP_NEAREST:
        sampler = new nearest_sampler<float>();
        break;

    case SAMP_LINEAR:
        sampler = new linear_sampler<float>();
        break;

    case SAMP_RRC:
    {

        if (m_objCfg.rrc_steps == 0)
        {
            // At least 64 discrete sampling points between symbols
            m_objCfg.rrc_steps = max(1, (int) (64 * m_objCfg.Fm / m_objCfg.Fs));
        }

        float Frrc = m_objCfg.Fs * m_objCfg.rrc_steps; // Sample freq of the RRC filter
        float transition = (m_objCfg.Fm / 2) * m_objCfg.rolloff;
        int order = m_objCfg.rrc_rej * Frrc / (22 * transition);
        ncoeffs_sampler = filtergen::root_raised_cosine(order, m_objCfg.Fm / Frrc, m_objCfg.rolloff, &coeffs_sampler);

        sampler = new fir_sampler<float, float>(ncoeffs_sampler, coeffs_sampler, m_objCfg.rrc_steps);
        break;
    }

    default:
        fatal("Interpolator not implemented");
    }

    m_objDemodulator = new cstln_receiver<f32>(m_objScheduler, sampler, *p_preprocessed, *p_symbols, p_freq, p_ss, p_mer, p_sampled);

    if (m_objCfg.standard == DVB_S)
    {
        if (m_objCfg.constellation != cstln_lut<256>::QPSK && m_objCfg.constellation != cstln_lut<256>::BPSK)
        {
            qWarning("DATVDemod::InitDATVFramework: non-standard constellation for DVB-S");
        }
    }

    if (m_objCfg.standard == DVB_S2)
    {
        // For DVB-S2 testing only.
        // Constellation should be determined from PL signalling.
        qWarning("DATVDemod::InitDATVFramework: DVB-S2: Testing symbol sampler only.");
    }

    m_objDemodulator->cstln = make_dvbs2_constellation(m_objCfg.constellation, m_objCfg.fec);

    if (m_objCfg.hard_metric)
    {
        m_objDemodulator->cstln->harden();
    }

    m_objDemodulator->set_omega(m_objCfg.Fs / m_objCfg.Fm);

    if (m_objCfg.Ftune)
    {

        m_objDemodulator->set_freq(m_objCfg.Ftune / m_objCfg.Fs);
    }

    if (m_objCfg.allow_drift)
    {
        m_objDemodulator->set_allow_drift(true);
    }

    if (m_objCfg.viterbi)
    {
        m_objDemodulator->pll_adjustment /= 6;
    }

    m_objDemodulator->meas_decimation = decimation(m_objCfg.Fs, m_objCfg.Finfo);

    // TRACKING FILTERS

    if (r_resample)
    {
        r_resample->freq_tap = &m_objDemodulator->freq_tap;
        r_resample->tap_multiplier = 1.0 / decim;
        r_resample->freq_tol = m_objCfg.Fm / (m_objCfg.Fs * decim) * 0.1;
    }

    if (r_cnr)
    {
        r_cnr->freq_tap = &m_objDemodulator->freq_tap;
        r_cnr->tap_multiplier = 1.0 / decim;
    }

    //constellation

    m_objRegisteredDATVScreen->resizeDATVScreen(256, 256);

    r_scope_symbols = new datvconstellation<f32>(m_objScheduler, *p_sampled, -128, 128, NULL, m_objRegisteredDATVScreen);
    r_scope_symbols->decimation = 1;
    r_scope_symbols->cstln = &m_objDemodulator->cstln;

    // DECONVOLUTION AND SYNCHRONIZATION

    p_bytes = new pipebuf<u8>(m_objScheduler, "bytes", BUF_BYTES);

    r_deconv = NULL;

    if (m_objCfg.viterbi)
    {
        if (m_objCfg.fec == FEC23 && (m_objDemodulator->cstln->nsymbols == 4 || m_objDemodulator->cstln->nsymbols == 64))
        {
            m_objCfg.fec = FEC46;
        }

        //To uncomment -> Linking Problem : undefined symbol: _ZN7leansdr21viterbi_dec_interfaceIhhiiE6updateEPiS2_
        r = new viterbi_sync(m_objScheduler, (*p_symbols), (*p_bytes), m_objDemodulator->cstln, m_objCfg.fec);

        if (m_objCfg.fastlock)
        {
            r->resync_period = 1;
        }
    }
    else
    {
        r_deconv = make_deconvol_sync_simple(m_objScheduler, (*p_symbols), (*p_bytes), m_objCfg.fec);
        r_deconv->fastlock = m_objCfg.fastlock;
    }

    if (m_objCfg.hdlc)
    {
        p_descrambled = new pipebuf<u8>(m_objScheduler, "descrambled", BUF_MPEGBYTES);
        r_etr192_descrambler = new etr192_descrambler(m_objScheduler, (*p_bytes), *p_descrambled);
        p_frames = new pipebuf<u8>(m_objScheduler, "frames", BUF_MPEGBYTES);
        r_sync = new hdlc_sync(m_objScheduler, *p_descrambled, *p_frames, 2, 278);

        if (m_objCfg.fastlock)
        {
            r_sync->resync_period = 1;
        }

        if (m_objCfg.packetized)
        {
            r_sync->header16 = true;
        }

    }

    p_mpegbytes = new pipebuf<u8>(m_objScheduler, "mpegbytes", BUF_MPEGBYTES);
    p_lock = new pipebuf<int>(m_objScheduler, "lock", BUF_SLOW);
    p_locktime = new pipebuf<u32>(m_objScheduler, "locktime", BUF_PACKETS);

    if (!m_objCfg.hdlc)
    {
        r_sync_mpeg = new mpeg_sync<u8, 0>(m_objScheduler, *p_bytes, *p_mpegbytes, r_deconv, p_lock, p_locktime);
        r_sync_mpeg->fastlock = m_objCfg.fastlock;
    }

    // DEINTERLEAVING

    p_rspackets = new pipebuf<rspacket<u8> >(m_objScheduler, "RS-enc packets", BUF_PACKETS);
    r_deinter = new deinterleaver<u8>(m_objScheduler, *p_mpegbytes, *p_rspackets);

    // REED-SOLOMON

    p_vbitcount = new pipebuf<int>(m_objScheduler, "Bits processed", BUF_PACKETS);
    p_verrcount = new pipebuf<int>(m_objScheduler, "Bits corrected", BUF_PACKETS);
    p_rtspackets = new pipebuf<tspacket>(m_objScheduler, "rand TS packets", BUF_PACKETS);
    r_rsdec = new rs_decoder<u8, 0>(m_objScheduler, *p_rspackets, *p_rtspackets, p_vbitcount, p_verrcount);

    // BER ESTIMATION

    p_vber = new pipebuf<float>(m_objScheduler, "VBER", BUF_SLOW);
    r_vber = new rate_estimator<float>(m_objScheduler, *p_verrcount, *p_vbitcount, *p_vber);
    r_vber->sample_size = m_objCfg.Fm / 2; // About twice per second, depending on CR
    // Require resolution better than 2E-5
    if (r_vber->sample_size < 50000)
    {
        r_vber->sample_size = 50000;
    }

    // DERANDOMIZATION

    p_tspackets = new pipebuf<tspacket>(m_objScheduler, "TS packets", BUF_PACKETS);
    r_derand = new derandomizer(m_objScheduler, *p_rtspackets, *p_tspackets);

    // OUTPUT
    r_videoplayer = new datvvideoplayer<tspacket>(m_objScheduler, *p_tspackets, m_objVideoStream);

    m_blnDVBInitialized = true;
}

void DATVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
    float fltI;
    float fltQ;
    cf32 objIQ;
    //Complex objC;
    fftfilt::cmplx *objRF;
    int intRFOut;

#ifdef EXTENDED_DIRECT_SAMPLE

    qint16 * ptrBufferToRelease=NULL;
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

        if ((m_blnDVBInitialized == false) || (m_blnNeedConfigUpdate == true))
        {
            m_blnNeedConfigUpdate = false;
            InitDATVFramework();
        }

        //********** iq stream ****************

        if (m_lngReadIQ > p_rawiq_writer->writable())
        {
            m_objScheduler->step();

            m_objRegisteredDATVScreen->renderImage(NULL);

            m_lngReadIQ = 0;
            p_rawiq_writer = new pipewriter<cf32>(*p_rawiq);
        }

        if (false)
        {
            objIQ.re = fltI;
            objIQ.im = fltQ;

            p_rawiq_writer->write(objIQ);

            m_lngReadIQ++;
        }
        else
        {

            Complex objC(fltI, fltQ);

            objC *= m_objNCO.nextIQ();

            intRFOut = m_objRFFilter->runFilt(objC, &objRF); // filter RF before demod

            for (int intI = 0; intI < intRFOut; intI++)
            {
                objIQ.re = objRF->real();
                objIQ.im = objRF->imag();

                p_rawiq_writer->write(objIQ);

                objRF++;
                m_lngReadIQ++;
            }
        }

        //********** demodulation **********

    }

#ifdef EXTENDED_DIRECT_SAMPLE
    if(ptrBufferToRelease!=NULL)
    {
        delete ptrBufferToRelease;
    }
#endif

    //m_objSettingsMutex.unlock();

}

void DATVDemod::start()
{
    //m_objTimer.start();
}

void DATVDemod::stop()
{

}

bool DATVDemod::handleMessage(const Message& cmd)
{
    if (DownChannelizer::MsgChannelizerNotification::match(cmd))
    {
        DownChannelizer::MsgChannelizerNotification& objNotif = (DownChannelizer::MsgChannelizerNotification&) cmd;

        qDebug() << "DATVDemod::handleMessage: MsgChannelizerNotification:" << " intMsps: " << objNotif.getSampleRate();

        if (m_objRunning.intMsps != objNotif.getSampleRate())
        {
            m_objRunning.intMsps = objNotif.getSampleRate();
            m_objRunning.intSampleRate = m_objRunning.intMsps;

            qDebug("DATVDemod::handleMessage: Sample Rate: %d", m_objRunning.intSampleRate);
            ApplySettings();
        }

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {

        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        qDebug() << "DATVDemod::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << m_channelizer->getInputSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(), m_channelizer->getInputSampleRate(), cfg.getCenterFrequency());
        //m_objRunning.intCenterFrequency);
        //<< " centerFrequency: " << m_objRunning.intCenterFrequency;

        return true;
    }
    else if (MsgConfigureDATVDemod::match(cmd))
    {
        qDebug() << "DATVDemod::handleMessage: MsgConfigureDATVDemod";

        MsgConfigureDATVDemod& objCfg = (MsgConfigureDATVDemod&) cmd;

        if ((objCfg.m_objMsgConfig.blnAllowDrift != m_objRunning.blnAllowDrift) || (objCfg.m_objMsgConfig.intRFBandwidth != m_objRunning.intRFBandwidth)
                || (objCfg.m_objMsgConfig.intCenterFrequency != m_objRunning.intCenterFrequency) || (objCfg.m_objMsgConfig.blnFastLock != m_objRunning.blnFastLock)
                || (objCfg.m_objMsgConfig.blnHardMetric != m_objRunning.blnHardMetric) || (objCfg.m_objMsgConfig.blnHDLC != m_objRunning.blnHDLC)
                || (objCfg.m_objMsgConfig.blnResample != m_objRunning.blnResample) || (objCfg.m_objMsgConfig.blnViterbi != m_objRunning.blnViterbi)
                || (objCfg.m_objMsgConfig.enmFEC != m_objRunning.enmFEC) || (objCfg.m_objMsgConfig.enmModulation != m_objRunning.enmModulation)
                || (objCfg.m_objMsgConfig.enmStandard != m_objRunning.enmStandard) || (objCfg.m_objMsgConfig.intNotchFilters != m_objRunning.intNotchFilters)
                || (objCfg.m_objMsgConfig.intSymbolRate != m_objRunning.intSymbolRate))
        {
            m_objRunning.blnAllowDrift = objCfg.m_objMsgConfig.blnAllowDrift;
            m_objRunning.blnFastLock = objCfg.m_objMsgConfig.blnFastLock;
            m_objRunning.blnHardMetric = objCfg.m_objMsgConfig.blnHardMetric;
            m_objRunning.blnHDLC = objCfg.m_objMsgConfig.blnHDLC;
            m_objRunning.blnResample = objCfg.m_objMsgConfig.blnResample;
            m_objRunning.blnViterbi = objCfg.m_objMsgConfig.blnViterbi;
            m_objRunning.enmFEC = objCfg.m_objMsgConfig.enmFEC;
            m_objRunning.enmModulation = objCfg.m_objMsgConfig.enmModulation;
            m_objRunning.enmStandard = objCfg.m_objMsgConfig.enmStandard;
            m_objRunning.intNotchFilters = objCfg.m_objMsgConfig.intNotchFilters;
            m_objRunning.intSymbolRate = objCfg.m_objMsgConfig.intSymbolRate;
            m_objRunning.intRFBandwidth = objCfg.m_objMsgConfig.intRFBandwidth;
            m_objRunning.intCenterFrequency = objCfg.m_objMsgConfig.intCenterFrequency;

            ApplySettings();
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        qDebug() << "DATVDemod::handleMessage: DSPSignalNotification";
        return true;
    }
    else
    {
        return false;
    }
}

void DATVDemod::ApplySettings()
{

    if (m_objRunning.intMsps == 0)
    {
        return;
    }

    //m_objSettingsMutex.lock();

    InitDATVParameters(
            m_objRunning.intMsps,
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
            m_objRunning.blnHDLC,
            m_objRunning.blnHardMetric,
            m_objRunning.blnResample,
            m_objRunning.blnViterbi);

}

int DATVDemod::GetSampleRate()
{
    return m_objRunning.intMsps;
}

void DATVDemod::channelSampleRateChanged()
{
    qDebug("DATVDemod::channelSampleRateChanged");
    // reconfigure to get full available bandwidth
    m_channelizer->configure(m_channelizer->getInputMessageQueue(), m_channelizer->getInputSampleRate(), m_channelizer->getRequestedCenterFrequency());
    // TODO: forward to GUI if necessary
}
