///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_M17DEMODPROCESSOR_H
#define INCLUDE_M17DEMODPROCESSOR_H

#include <QObject>

#include "dsp/dsptypes.h"
#include "audio/audiocompressor.h"
#include "M17Demodulator.h"
#include "m17demodfilters.h"

class AudioFifo;
class MessageQueue;

class M17DemodProcessor : public QObject
{
    Q_OBJECT
public:
    enum StdPacketProtocol
    {
        StdPacketRaw,
        StdPacketAX25,
        StdPacketAPRS,
        StdPacket6LoWPAN,
        StdPacketIPv4,
        StdPacketSMS,
        StdPacketWinlink,
        StdPacketUnknown
    };

    M17DemodProcessor();
    ~M17DemodProcessor();

    void setDemodInputMessageQueue(MessageQueue *messageQueue) { m_demodInputMessageQueue = messageQueue; }
    void pushSample(qint16 sample);
    void setDisplayLSF(bool displayLSF) { m_displayLSF = displayLSF; }
    void setNoiseBlanker(bool noiseBlanker) { m_noiseBlanker = noiseBlanker; }
    void setAudioFifo(AudioFifo *fifo) { m_audioFifo = fifo; }
    void setAudioMute(bool mute) { m_audioMute = mute; }
    void setUpsampling(int upsampling);
    void setVolume(float volume);
    void setHP(bool useHP) { m_upsamplingFilter.useHP(useHP); }
    void resetInfo();
    void setDCDOff();
    uint32_t getLSFCount() const { return m_lsfCount; }
    const QString& getSrcCall() const { return m_srcCall; }
    const QString& getDestcCall() const { return m_destCall; }
    const QString& getTypeInfo() const { return m_typeInfo; }
    const std::array<uint8_t, 14>& getMeta() const { return m_metadata; }
    bool getHasGNSS() const { return m_hasGNSS; }
    bool getStreamElsePacket() const { return m_streamElsePacket; }
    uint16_t getCRC() const { return m_crc; }
    StdPacketProtocol getStdPacketProtocol() const;

    void getDiagnostics(
        bool& dcd,
        float& evm,
        float& deviation,
        float& offset,
        int& status,
        int& sync_word_type,
        float& clock,
        int& sampleIndex,
        int& syncIndex,
        int& clockIndex,
        int& viterbiCost
    ) const
    {
        dcd = m_dcd;
        evm = m_evm;
        deviation = m_deviation;
        offset = m_offset;
        status = m_status;
        sync_word_type = m_syncWordType;
        clock = m_clock;
        sampleIndex = m_sampleIndex;
        syncIndex = m_syncIndex;
        clockIndex = m_clockIndex;
        viterbiCost = m_viterbiCost;
    }

    void getBERT(uint32_t& bertErrors, uint32_t& bertBits)
    {
        bertErrors = m_prbs.errors();
        bertBits = m_prbs.bits();
    }

    void resetPRBS() {
        m_prbs.reset();
    }

private:
    std::vector<uint8_t> m_currentPacket;
    size_t m_packetFrameCounter;
    modemm17::PRBS9 m_prbs;
    bool m_displayLSF;
    bool m_noiseBlanker;
    struct CODEC2 *m_codec2;
    static M17DemodProcessor *m_this;
    modemm17::M17Demodulator m_demod;
    AudioFifo *m_audioFifo;
    bool m_audioMute;
    AudioVector m_audioBuffer;
    std::size_t m_audioBufferFill;
    float m_volume;
    int m_upsampling;            //!< upsampling factor
    float m_upsamplingFactors[7];
    AudioCompressor m_compressor;
    float m_upsamplerLastValue;

    M17DemodAudioInterpolatorFilter m_upsamplingFilter;

    // Diagnostics
    bool m_dcd;         //!< Data Carrier Detect
    float m_evm;        //!< Error Vector Magnitude in percent
    float m_deviation;  //!< Estimated deviation. Ideal = 1.0
    float m_offset;     //!< Estimated frequency offset. Ideal = 0.0 practically limited to ~[-0.18, 0.18]
    int m_status;       //!< Status
    int m_syncWordType; //!< Sync word type
    float m_clock;
    int m_sampleIndex;
    int m_syncIndex;
    int m_clockIndex;
    int m_viterbiCost;  //!< [-1:128] ideally 0

    QString m_srcCall;
    QString m_destCall;
    QString m_typeInfo;
    bool m_streamElsePacket;
    std::array<uint8_t, 14> m_metadata;
    bool m_hasGNSS;
    uint16_t m_crc;
    uint32_t m_lsfCount; // Incremented each time a new LSF is decoded. Reset when lock is lost.
    StdPacketProtocol m_stdPacketProtocol;

    MessageQueue *m_demodInputMessageQueue;

    static bool handle_frame(modemm17::M17FrameDecoder::output_buffer_t const& frame, int viterbi_cost);
    static void diagnostic_callback(
        bool dcd,
        float evm,
        float deviation,
        float offset,
        int status,
        int sync_word_type,
        float clock,
        int sample_index,
        int sync_index,
        int clock_index,
        int viterbi_cost
    );
    bool decode_lsf(modemm17::M17FrameDecoder::lsf_buffer_t const& lsf);
    bool decode_lich(modemm17::M17FrameDecoder::lich_buffer_t const& lich);
    bool decode_packet(modemm17::M17FrameDecoder::packet_buffer_t const& packet_segment);
    bool decode_bert(modemm17::M17FrameDecoder::bert_buffer_t const& bert);
    bool demodulate_audio(modemm17::M17FrameDecoder::audio_buffer_t const& audio, int viterbi_cost);
    void decode_type(uint16_t type);
    void append_packet(std::vector<uint8_t>& result, modemm17::M17FrameDecoder::lsf_buffer_t in);

    void processAudio(const std::array<int16_t, 160>& in);
    void upsample(int upsampling, const int16_t *in, int nbSamplesIn);
    void noUpsample(const int16_t *in, int nbSamplesIn);

    void setVolumeFactors();
};

#endif // INCLUDE_M17PROCESSOR_H
