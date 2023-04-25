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

// #include <boost/crc.hpp>
// #include <boost/program_options.hpp>
// #include <boost/optional.hpp>
#include <codec2/codec2.h>

#include <QDebug>

#include "audio/audiofifo.h"
#include "util/ax25.h"

#include "ax25_frame.h"
#include "m17demod.h"
#include "m17demodprocessor.h"

M17DemodProcessor* M17DemodProcessor::m_this = nullptr;

M17DemodProcessor::M17DemodProcessor() :
    m_packetFrameCounter(0),
    m_displayLSF(true),
    m_noiseBlanker(true),
    m_demod(handle_frame),
    m_audioFifo(nullptr),
    m_audioMute(false),
    m_volume(1.0f),
    m_demodInputMessageQueue(nullptr)
{
    m_this = this;
    m_codec2 = ::codec2_create(CODEC2_MODE_3200);
    m_audioBuffer.resize(12000);
    m_audioBufferFill = 0;
    m_srcCall = "";
    m_destCall = "";
    m_typeInfo = "";
    m_metadata.fill(0);
    m_hasGNSS = false;
    m_crc = 0;
    m_lsfCount = 0;
    setUpsampling(6); // force upsampling of audio to 48k
    m_demod.diagnostics(diagnostic_callback);
}

M17DemodProcessor::~M17DemodProcessor()
{
    codec2_destroy(m_codec2);
}

void M17DemodProcessor::pushSample(qint16 sample)
{
    m_demod(sample / 22000.0f);
}

bool M17DemodProcessor::handle_frame(modemm17::M17FrameDecoder::output_buffer_t const& frame, int viterbi_cost)
{
    using FrameType = modemm17::M17FrameDecoder::FrameType;

    bool result = true;

    switch (frame.type)
    {
        case FrameType::LSF:
            result = m_this->decode_lsf(frame.lsf);
            break;
        case FrameType::LICH:
            result = m_this->decode_lich(frame.lich);
            break;
        case FrameType::STREAM:
            result = m_this->demodulate_audio(frame.stream, viterbi_cost);
            break;
        case FrameType::BASIC_PACKET:
            result = m_this->decode_packet(frame.packet);
            break;
        case FrameType::FULL_PACKET:
            result = m_this->decode_packet(frame.packet);
            break;
        case FrameType::BERT:
            result = m_this->decode_bert(frame.bert);
            break;
    }

    return result;
}

void M17DemodProcessor::diagnostic_callback(
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
    int viterbi_cost)
{
    bool debug = false;
    bool quiet = true;

    m_this->m_dcd = dcd;
    m_this->m_evm = evm;
    m_this->m_deviation = deviation;
    m_this->m_offset = offset;
    m_this->m_status = status;
    m_this->m_syncWordType = sync_word_type;
    m_this->m_clock = clock;
    m_this->m_sampleIndex = sample_index;
    m_this->m_syncIndex = sync_index;
    m_this->m_clockIndex = clock_index;
    m_this->m_viterbiCost = viterbi_cost;

    if (debug)
    {
        std::ostringstream oss;
        oss << "dcd: " << std::setw(1) << int(dcd)
            << ", evm: " << std::setfill(' ') << std::setprecision(4) << std::setw(8) << evm * 100 <<"%"
            << ", deviation: " << std::setprecision(4) << std::setw(8) << deviation
            << ", freq offset: " << std::setprecision(4) << std::setw(8) << offset
            << ", locked: " << std::boolalpha << std::setw(6) << (status != 0) << std::dec
            << ", clock: " << std::setprecision(7) << std::setw(8) << clock
            << ", sample: " << std::setw(1) << sample_index << ", "  << sync_index << ", " << clock_index
            << ", cost: " << viterbi_cost;
        qDebug() << "M17DemodProcessor::diagnostic_callback: " << oss.str().c_str();
    }

    if (m_this->m_prbs.sync() && !quiet)
    {
        std::ostringstream oss;
        auto ber = double(m_this->m_prbs.errors()) / double(m_this->m_prbs.bits());
        char buffer[40];
        snprintf(buffer, 40, "BER: %-1.6lf (%u bits)", ber, m_this->m_prbs.bits());
        oss << buffer;
        qDebug() << "M17DemodProcessor::diagnostic_callback: " << oss.str().c_str();
    }
}

bool M17DemodProcessor::decode_lich(modemm17::M17FrameDecoder::lich_buffer_t const& lich)
{
    uint8_t fragment_number = lich[5];   // Get fragment number.
    fragment_number = (fragment_number >> 5) & 7;
    qDebug("M17DemodProcessor::handle_frame: LICH: %d", (int) fragment_number);
    return true;
}

bool M17DemodProcessor::decode_lsf(modemm17::M17FrameDecoder::lsf_buffer_t const& lsf)
{
    modemm17::LinkSetupFrame::encoded_call_t encoded_call;
    std::ostringstream oss;

    std::copy(lsf.begin() + 6, lsf.begin() + 12, encoded_call.begin());
    modemm17::LinkSetupFrame::call_t src = modemm17::LinkSetupFrame::decode_callsign(encoded_call);
    m_srcCall = QString(src.data());

    std::copy(lsf.begin(), lsf.begin() + 6, encoded_call.begin());
    modemm17::LinkSetupFrame::call_t dest = modemm17::LinkSetupFrame::decode_callsign(encoded_call);
    m_destCall = QString(dest.data());

    uint16_t type = (lsf[12] << 8) | lsf[13];
    decode_type(type);

    m_hasGNSS = ((lsf[13] >> 5) & 3) == 1;

    std::copy(lsf.begin()+14, lsf.begin()+28, m_metadata.begin());
    m_crc = (lsf[28] << 8) | lsf[29];

    if (m_displayLSF)
    {
        oss << "SRC: " << m_srcCall.toStdString().c_str();
        oss << ", DEST: " << m_destCall.toStdString().c_str();
        oss << ", " << m_typeInfo.toStdString().c_str();
        oss << ", META: ";
        for (size_t i = 0; i != 14; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << int(m_metadata[i]);
        }
        oss << ", CRC: " << std::hex << std::setw(4) << std::setfill('0') << m_crc;
        oss << std::dec;
    }

    m_currentPacket.clear();
    m_packetFrameCounter = 0;

    if (!lsf[111]) // LSF type bit 0
    {
        uint8_t packet_type = (lsf[109] << 1) | lsf[110];

        switch (packet_type)
        {
        case 1: // RAW -- ignore LSF.
             break;
        case 2: // ENCAPSULATED
            append_packet(m_currentPacket, lsf);
            break;
        default:
            oss << " LSF for reserved packet type";
            append_packet(m_currentPacket, lsf);
        }
    }

    m_lsfCount++;
    qDebug() << "M17DemodProcessor::decode_lsf: " << m_lsfCount << ":" << oss.str().c_str();
    return true;
}

void M17DemodProcessor::decode_type(uint16_t type)
{
    m_streamElsePacket = type & 1; // bit 0

    if (m_streamElsePacket)
    {
        m_typeInfo = "STR:"; // Stream mode

        switch ((type & 6) >> 1) // bits 1..2
        {
            case 0:
                m_typeInfo += "UNK";
                break;
            case 1:
                m_typeInfo += "D/D";
                break;
            case 2:
                m_typeInfo += "V/V";
                break;
            case 3:
                m_typeInfo += "V/D";
                break;
        }
    }
    else
    {
        m_typeInfo = "PKT:"; // Packet mode

        switch ((type & 6) >> 1) // bits 1..2
        {
            case 0:
                m_typeInfo += "UNK";
                break;
            case 1:
                m_typeInfo += "RAW";
                break;
            case 2:
                m_typeInfo += "ENC";
                break;
            case 3:
                m_typeInfo += "UNK";
                break;
        }
    }

    m_typeInfo += QString(" CAN:%1").arg(int((type & 0x780) >> 7), 2, 10, QChar('0')); // Channel Access number (bits 7..10)
}

void M17DemodProcessor::resetInfo()
{
    m_srcCall = "";
    m_destCall = "";
    m_typeInfo = "";
    m_streamElsePacket = true;
    m_metadata.fill(0);
    m_crc = 0;
    m_lsfCount = 0;
}

void M17DemodProcessor::setDCDOff()
{
    m_demod.dcd_off();
}

void M17DemodProcessor::append_packet(std::vector<uint8_t>& result, modemm17::M17FrameDecoder::lsf_buffer_t in)
{
    uint8_t out = 0;
    size_t b = 0;

    for (auto c : in)
    {
        out = (out << 1) | c;
        if (++b == 8)
        {
            result.push_back(out);
            out = 0;
            b = 0;
        }
    }
}

bool M17DemodProcessor::decode_packet(modemm17::M17FrameDecoder::packet_buffer_t const& packet_segment)
{
    // qDebug() << tr("M17DemodProcessor::decode_packet: 0x%1").arg((int) packet_segment[25], 2, 16, QChar('0'));
    if (packet_segment[25] & 0x80) // last frame of packet.
    {
        size_t packet_size = (packet_segment[25] & 0x7F) >> 2;
        packet_size = std::min(packet_size, size_t(25)); // on last frame this is the remainder byte count
        m_packetFrameCounter = 0;

        for (size_t i = 0; i < packet_size; ++i) {
            m_currentPacket.push_back(packet_segment[i]);
        }

        if (m_currentPacket.size() < 3)
        {
            qDebug() << "M17DemodProcessor::decode_packet: too small:" << m_currentPacket.size();
            return false;
        }
        else
        {
            qDebug() << "M17DemodProcessor::decode_packet: last chunk size:" << packet_size << " packet size:" << m_currentPacket.size();
        }

        modemm17::CRC16 crc16(0x5935, 0xFFFF);
        crc16.reset();

        for (std::vector<uint8_t>::const_iterator it = m_currentPacket.begin(); it != m_currentPacket.end() - 2; ++it) {
            crc16(*it);
        }

        uint16_t calcChecksum = crc16.get_bytes()[0] + (crc16.get_bytes()[1]<<8);
        uint16_t xmitChecksum = m_currentPacket.back() + (m_currentPacket.end()[-2]<<8);

        if (calcChecksum == xmitChecksum) // (checksum == 0x0f47)
        {
            uint8_t protocol = m_currentPacket.front();
            m_stdPacketProtocol = protocol < (int) StdPacketUnknown ? (StdPacketProtocol) protocol : StdPacketUnknown;
            qDebug() << "M17DemodProcessor::decode_packet: protocol: " << m_stdPacketProtocol;

            if (m_stdPacketProtocol == StdPacketAX25)
            {
                std::string ax25;
                ax25.reserve(m_currentPacket.size());

                for (auto c : m_currentPacket) {
                    ax25.push_back(char(c));
                }

                modemm17::ax25_frame frame(ax25);
                std::ostringstream oss;
                modemm17::write(oss, frame); // TODO: get details
                qDebug() << "M17DemodProcessor::decode_packet: AX25:" << oss.str().c_str();
            }
            else if (m_stdPacketProtocol == StdPacketSMS)
            {
                std::ostringstream oss;

                for (std::vector<uint8_t>::const_iterator it = m_currentPacket.begin()+1; it < m_currentPacket.end()-2; ++it) {
                    oss << *it;
                }

                qDebug() << "M17DemodProcessor::decode SMS_packet: "
                    << " Src:" << getSrcCall()
                    << " Dest:" << getDestcCall()
                    << " SMS:" << oss.str().c_str();

                if (m_demodInputMessageQueue)
                {
                    M17Demod::MsgReportSMS *msg = M17Demod::MsgReportSMS::create(
                        getSrcCall(),
                        getDestcCall(),
                        QString(oss.str().c_str())
                    );
                    m_demodInputMessageQueue->push(msg);
                }
            }
            else if (m_stdPacketProtocol == StdPacketAPRS)
            {
                AX25Packet ax25;
                QByteArray packet = QByteArray(reinterpret_cast<const char*>(&m_currentPacket[1]), m_currentPacket.size()-3);

                if (ax25.decode(packet))
                {
                    qDebug() << "M17DemodProcessor::decode APRS_packet: "
                        << " Src:" << getSrcCall()
                        << " Dest:" << getDestcCall()
                        << " From:" << ax25.m_from
                        << " To: " << ax25.m_to
                        << " Via: " << ax25.m_via
                        << " Type: " << ax25.m_type
                        << " PID: " << ax25.m_pid
                        << " Data: " << ax25.m_dataASCII;

                    if (m_demodInputMessageQueue)
                    {
                        M17Demod::MsgReportAPRS *msg = M17Demod::MsgReportAPRS::create(
                            getSrcCall(),
                            getDestcCall(),
                            ax25.m_from,
                            ax25.m_to,
                            ax25.m_via,
                            ax25.m_type,
                            ax25.m_pid,
                            ax25.m_dataASCII
                        );
                        msg->getPacket() = packet;
                        m_demodInputMessageQueue->push(msg);
                    }
                }
            }

            return true;
        }

        QString ccrc = tr("0x%1").arg(calcChecksum, 4, 16, QChar('0'));
        QString xcrc = tr("0x%1").arg(xmitChecksum, 4, 16, QChar('0'));
        qWarning() << "M17DemodProcessor::decode_packet: Packet checksum error: " << ccrc << "vs" << xcrc;
        return false;
    }

    size_t frame_number = (packet_segment[25] & 0x7F) >> 2;

    if (frame_number != m_packetFrameCounter)
    {
        qWarning() << "M17DemodProcessor::decode_packet: Packet frame sequence error. Got "
            << frame_number << ", expected " << m_packetFrameCounter;
        return false;
    }

    if (m_packetFrameCounter == 0) {
        m_currentPacket.clear();
    }

    for (size_t i = 0; i < 25; ++i) {
        m_currentPacket.push_back(packet_segment[i]);
    }

    m_packetFrameCounter++;

    return true;
}

bool M17DemodProcessor::decode_bert(modemm17::M17FrameDecoder::bert_buffer_t const& bert)
{
    for (int j = 0; j != 24; ++j)
    {
        auto b = bert[j];

        for (int i = 0; i != 8; ++i)
        {
            m_prbs.validate(b & 0x80);
            b <<= 1;
        }
    }

    auto b = bert[24];

    for (int i = 0; i != 5; ++i)
    {
        m_prbs.validate(b & 0x80);
        b <<= 1;
    }

    return true;
}

bool M17DemodProcessor::demodulate_audio(modemm17::M17FrameDecoder::audio_buffer_t const& audio, int viterbi_cost)
{
    bool result = true;
    std::array<int16_t, 160> buf; // 8k audio

    // First two bytes are the frame counter + EOS indicator.
    if (viterbi_cost < 70 && (audio[0] & 0x80))
    {
        if (m_displayLSF) {
            qDebug() << "M17DemodProcessor::demodulate_audio: EOS";
        }

        result = false;
    }

    if (m_audioFifo && !m_audioMute)
    {
        if (m_noiseBlanker && viterbi_cost > 80)
        {
            buf.fill(0);
            processAudio(buf); // first block expanded
            processAudio(buf); // second block expanded
        }
        else
        {
            codec2_decode(m_codec2, buf.data(), audio.data() + 2);  // first 8 bytes block input
            processAudio(buf);
            codec2_decode(m_codec2, buf.data(), audio.data() + 10); // second 8 bytes block input
            processAudio(buf);
        }
    }

    return result;
}

void M17DemodProcessor::setUpsampling(int upsampling)
{
    m_upsampling = upsampling < 1 ? 1 : upsampling > 6 ? 6 : upsampling;
}

void M17DemodProcessor::setVolume(float volume)
{
    m_volume = volume;
    setVolumeFactors();
}

void M17DemodProcessor::processAudio(const std::array<int16_t, 160>& in)
{
    if (m_upsampling > 1) {
        upsample(m_upsampling, in.data(), in.size());
    } else {
        noUpsample(in.data(), in.size());
    }

    if (m_audioBufferFill >= m_audioBuffer.size() - 960)
    {
        std::size_t res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

        if (res != m_audioBufferFill) {
            qDebug("M17DemodProcessor::processAudio: %lu/%lu audio samples written", res, m_audioBufferFill);
        }

        m_audioBufferFill = 0;
    }
}

void M17DemodProcessor::upsample(int upsampling, const int16_t *in, int nbSamplesIn)
{
    for (int i = 0; i < nbSamplesIn; i++)
    {
        float cur = m_upsamplingFilter.usesHP() ? m_upsamplingFilter.runHP((float) in[i]) : (float) in[i];
        float prev = m_upsamplerLastValue;
        qint16 upsample;

        for (int j = 1; j <= upsampling; j++)
        {
            upsample = (qint16) m_upsamplingFilter.runLP(cur*m_upsamplingFactors[j] + prev*m_upsamplingFactors[upsampling-j]);
            m_audioBuffer[m_audioBufferFill].l = m_compressor.compress(upsample);
            m_audioBuffer[m_audioBufferFill].r = m_compressor.compress(upsample);

            if (m_audioBufferFill < m_audioBuffer.size() - 1) {
                ++m_audioBufferFill;
            }
        }

        m_upsamplerLastValue = cur;
    }

    if (m_audioBufferFill >= m_audioBuffer.size() - 1) {
        qDebug("M17DemodProcessor::upsample(%d): audio buffer is full check its size", upsampling);
    }
}

void M17DemodProcessor::noUpsample(const int16_t *in, int nbSamplesIn)
{
    for (int i = 0; i < nbSamplesIn; i++)
    {
        float cur = m_upsamplingFilter.usesHP() ? m_upsamplingFilter.runHP((float) in[i]) : (float) in[i];
        m_audioBuffer[m_audioBufferFill].l = cur*m_upsamplingFactors[0];
        m_audioBuffer[m_audioBufferFill].r = cur*m_upsamplingFactors[0];

        if (m_audioBufferFill < m_audioBuffer.size() - 1) {
            ++m_audioBufferFill;
        }
    }

    if (m_audioBufferFill >= m_audioBuffer.size() - 1) {
        qDebug("M17DemodProcessor::noUpsample: audio buffer is full check its size");
    }
}

void M17DemodProcessor::setVolumeFactors()
{
    m_upsamplingFactors[0] = m_volume;

    for (int i = 1; i <= m_upsampling; i++) {
        m_upsamplingFactors[i] = (i*m_volume) / (float) m_upsampling;
    }
}

M17DemodProcessor::StdPacketProtocol M17DemodProcessor::getStdPacketProtocol() const
{
    if (m_streamElsePacket) {
        return StdPacketUnknown;
    } else {
        return m_stdPacketProtocol;
    }
}
