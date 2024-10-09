///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef PLUGINS_CHANNELRX_REMOTETCPSINK_REMOTETCPPROTOCOL_H_
#define PLUGINS_CHANNELRX_REMOTETCPSINK_REMOTETCPPROTOCOL_H_

#include <QString>

// Remote TCP protocol based on rtl_tcp (for compatibility) with a few extensions (as SDRangel supports more SDRs)
class RemoteTCPProtocol
{
public:

    enum Device {
        // These are compatible with rtl_tcp
        UNKNOWN = 0,
        RTLSDR_E4000,
        RTLSDR_FC0012,
        RTLSDR_FC0013,
        RTLSDR_FC2580,
        RTLSDR_R820T,       // Used by rsp_tcp
        RTLSDR_R828D,
        // SDRangel extensions that correspond to sample source devices
        AIRSPY = 0x80,
        AIRSPY_HF,
        AUDIO_INPUT,
        BLADE_RF1,
        BLADE_RF2,
        FCD_PRO,
        FCD_PRO_PLUS,
        FILE_INPUT,
        HACK_RF,
        KIWI_SDR,
        LIME_SDR,
        LOCAL_INPUT,
        PERSEUS,
        PLUTO_SDR,
        REMOTE_INPUT,
        REMOTE_TCP_INPUT,
        SDRPLAY_1,
        SDRPLAY_V3_RSP1,
        SDRPLAY_V3_RSP1A,
        SDRPLAY_V3_RSP2,
        SDRPLAY_V3_RSPDUO,
        SDRPLAY_V3_RSPDX,
        SIGMF_FILE_INPUT,
        SOAPY_SDR,
        TEST_SOURCE,
        USRP,
        XTRX,
        SDRPLAY_V3_RSP1B
    };

    enum Command {
        // These are compatible with osmocom rtl_tcp: https://github.com/osmocom/rtl-sdr/blob/master/src/rtl_tcp.c
        // and Android https://github.com/signalwareltd/rtl_tcp_andro-/blob/master/rtlsdr/src/main/cpp/src/tcp_commands.h
        setCenterFrequency = 0x1,           // rtlsdr_set_center_freq
        setSampleRate = 0x2,                // rtlsdr_set_sample_rate
        setTunerGainMode = 0x3,             // rtlsdr_set_tuner_gain_mode
        setTunerGain = 0x4,                 // rtlsdr_set_tuner_gain
        setFrequencyCorrection = 0x5,       // rtlsdr_set_freq_correction
        setTunerIFGain = 0x6,               // rtlsdr_set_tuner_if_gain - Used by SDRangel to set LNA/VGA/IF gain individually
        setTestMode = 0x7,                  // Not supported by SDRangel
        setAGCMode = 0x8,                   // rtlsdr_set_agc_mode
        setDirectSampling = 0x9,            // rtlsdr_set_direct_sampling
        setOffsetTuning = 0xa,
        setXtalFrequency = 0xb,             // Not supported by SDRangel
        setXtalFrequency2 = 0xc,            // Not supported by SDRangel
        setGainByIndex = 0xd,               // Not supported by SDRangel
        setBiasTee = 0xe,                   // rtlsdr_set_bias_tee (Not supported on Android)
        // These extensions are from rsp_tcp: https://github.com/SDRplay/RSPTCPServer/blob/master/rsp_tcp_api.h
        rspSetAntenna = 0x1f,
        rspSetLNAState = 0x20,
        rspSetIfGainR = 0x21,
        rspSetAGC = 0x22,
        rspSetAGCSetPoint = 0x23,
        rspSetNotch = 0x24,
        rspSetBiasT = 0x25,
        rspSetRefOut = 0x26,
         // These extensions are from librtlsdr rtl_tcp: https://github.com/librtlsdr/librtlsdr/blob/development/include/rtl_tcp.h
        setTunerBandwidth = 0x40,
        // Android extensions https://github.com/signalwareltd/rtl_tcp_andro-/blob/master/rtlsdr/src/main/cpp/src/tcp_commands.h
        androidExit = 0x7e,
        androidGainByPercentage = 0x7f,
        androidEnable16BitSigned = 0x80,    // SDRplay, not RTL SDR
        // These are SDRangel extensions
        setDCOffsetRemoval = 0xc0,
        setIQCorrection = 0xc1,
        setDecimation = 0xc2,               // Device to baseband decimation
        setChannelSampleRate = 0xc3,
        setChannelFreqOffset = 0xc4,
        setChannelGain = 0xc5,
        setSampleBitDepth = 0xc6,           // Bit depth for samples sent over network
        setIQSquelchEnabled = 0xc7,
        setIQSquelch = 0xc8,
        setIQSquelchGate = 0xc9,
        //setAntenna?
        //setLOOffset?
        sendMessage = 0xd0,
        sendBlacklistedMessage = 0xd1,
        dataIQ = 0xf0,                      // Uncompressed IQ data
        dataIQFLAC = 0xf1,                  // IQ data compressed with FLAC
        dataIQzlib = 0xf2,                  // IQ data compressed with zlib
        dataPosition = 0xf3,                // Lat, Long, Alt of anntenna
        dataDirection = 0xf4                // Az/El of antenna
    };

    static const int m_rtl0MetaDataSize = 12;
    static const int m_rsp0MetaDataSize = 45;
    static const int m_sdraMetaDataSize = 128;

    static void encodeInt16(quint8 *p, qint16 data)
    {
        p[0] = (data >> 8) & 0xff;
        p[1] = data & 0xff;
    }

    static void encodeUInt32(quint8 *p, quint32 data)
    {
        p[0] = (data >> 24) & 0xff;
        p[1] = (data >> 16) & 0xff;
        p[2] = (data >> 8) & 0xff;
        p[3] = data & 0xff;
    }

    static void encodeInt32(quint8 *p, qint32 data)
    {
        encodeUInt32(p, (quint32)data);
    }

    static qint16 extractInt16(const quint8 *p)
    {
        qint16 data;
        data = (p[1] & 0xff)
            | ((p[0] & 0xff) << 8);
        return data;
    }

    static quint32 extractUInt32(const quint8 *p)
    {
        quint32 data;
        data = (p[3] & 0xff)
            | ((p[2] & 0xff) << 8)
            | ((p[1] & 0xff) << 16)
            | ((p[0] & 0xff) << 24);
        return data;
    }

    static qint32 extractInt32(const quint8 *p)
    {
        return (qint32)extractUInt32(p);
    }

    static void encodeUInt64(quint8 *p, quint64 data)
    {
        p[0] = (data >> 56) & 0xff;
        p[1] = (data >> 48) & 0xff;
        p[2] = (data >> 40) & 0xff;
        p[3] = (data >> 32) & 0xff;
        p[4] = (data >> 24) & 0xff;
        p[5] = (data >> 16) & 0xff;
        p[6] = (data >> 8) & 0xff;
        p[7] = data & 0xff;
    }

    static quint64 extractUInt64(const quint8 *p)
    {
        quint64 data;
        data = (p[7] & 0xff)
            | ((p[6] & 0xff) << 8)
            | ((p[5] & 0xff) << 16)
            | ((p[4] & 0xff) << 24)
            | (((quint64)(p[3] & 0xff)) << 32)
            | (((quint64)(p[2] & 0xff)) << 40)
            | (((quint64)(p[1] & 0xff)) << 48)
            | (((quint64)(p[0] & 0xff)) << 56);
        return data;
    }

    static void encodeFloat(quint8 *p, float data)
    {
        quint32 t;

        memcpy(&t, &data, 4);

        encodeUInt32(p, t);
    }

    static float extractFloat(const quint8 *p)
    {
        quint32 t;
        float f;

        t = extractUInt32(p);

        memcpy(&f, &t, 4);

        return f;
    }

};

#endif /* PLUGINS_CHANNELRX_REMOTETCPSINK_REMOTETCPPROTOCOL_H_ */
