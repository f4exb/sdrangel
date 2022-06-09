#pragma once

#include "queue.h"
#include "FirFilter.h"
#include "LinkSetupFrame.h"
#include "CRC16.h"
#include "Convolution.h"
#include "PolynomialInterleaver.h"
#include "M17Randomizer.h"
#include "Util.h"
#include "Golay24.h"
#include "Trellis.h"

#include <codec2/codec2.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>

namespace mobilinkd
{

/**
 * Asynchronous M17 modulator.  This modulator is initialized with the source and
 * destination callsigns.  It is then run by attaching an input queue and an output
 * queue.  The modulator reads 16-bit, 8ksps, 1-channel audio samples from the input
 * queue and an M17 bitstream (in 8-bit bytes, 4 symbols per byte) to the output queue.
 *
 * The call to run(), which is used to attach the queues, returns immediately, starting
 * a new thread in a detached state.  run() returns a future, which may contain error
 * information if an exception is thrown.
 *
 * The modulator stops when the input queue is closed.
 *
 * The modulator starts in a paused state, discarding all input.
 *
 * The modulator is started by calling ptt_on().  This causes the preamble and link
 * setup frame to be sent.  The modulator then starts reading from the input queue
 * and writing the data stream to the output queue.
 *
 * The modulator can be paused by calling ptt_off().  This will cause any audio
 * samples remaining in the input queue to be discarded.  The final frame will
 * be sent with the EOS bit set.  The output queue should always be completely
 * drained and all symbols output should be transmitted to ensure proper EOS
 * signalling.
 *
 * Output will be bursty -- their is no throttling of the symbol stream.  As soon
 * as enough input samples are received to fill the M17 payload field, the frame
 * will be constructed and the symbol stream output on the queue.
 *
 * @invariant The state of the modulator is one of INACTIVE, IDLE, PREAMBLE,
 *  LINK_SETUP, ACTIVE, or END_OF_STREAM.
 *
 * The modulator transitions from INACTIVE to IDLE when run() is called.
 *
 * The modulator transitions from IDLE to PREAMBLE when ptt_on() is called.
 *
 * The modulator will transition from PREAMBLE to LINK_SETUP to ACTIVE automatically.
 *
 * The modulator transitions from ACTIVE to END_OF_STREAM when ptt_off() is called.
 *
 * The modulator transitions from END_OF_STREAM to IDLE after the last audio
 * frame is emitted.
 *
 * The modulator will transition from IDLE to INACTIVE when the input or output
 * queue is closed.
 *
 * The modulator will emit at least 3 frames when ptt_on() is called: the preamble,
 * the link setup frame, and one audio frame with the EOS flag set.
 *
 * It is an error to close the input or output stream when the modulator is not IDLE.
 *
 * @section Thread Safety
 *
 * Internally, the modulator is thread-safe.  It is running with a background thread
 * reading from and writing to thread-safe queues.  Externally, the modulator expects
 * that all API calls made synchronously as if from a single thread of control.
 *
 * @section Convertion Functions
 *
 * There are two public static conversion functions provided to support conversion of
 * the output bitstream into either a symbol stream or into a 48ksps baseband stream.
 */
struct M17Modulator
{
public:
    using bitstream_queue_t = queue<uint8_t, 96>;   // 1 frame's worth of data, 48 bytes, 192 symbols, 384 bits.
    using audio_queue_t = queue<int16_t, 320>;      // 1 frame's worth of data.
    using symbols_t = std::array<int8_t, 192>;      // One frame of symbols.
    using baseband_t = std::array<int16_t, 1920>;   // One frame of baseband data @ 48ksps
    using bitstream_t = std::array<uint8_t, 48>;    // M17 frame of bits (in bytes).

    enum class State {INACTIVE, IDLE, PREAMBLE, LINK_SETUP, ACTIVE, END_OF_STREAM};

private:
    using lsf_t = std::array<uint8_t, 30>;          // Link setup frame bytes.

    using lich_segment_t = std::array<uint8_t, 12>; // Golay-encoded LICH.
    using lich_t = std::array<lich_segment_t, 6>;   // All LICH segments.
    using audio_frame_t = std::array<int16_t, 320>;
    using codec_frame_t = std::array<uint8_t, 16>;
    using payload_t = std::array<uint8_t, 34>;      // Bytes in the payload of a data frame.
    using frame_t = std::array<uint8_t, 46>;        // M17 frame (without sync word).

    static constexpr std::array<uint8_t, 2> SYNC_WORD = {0x32, 0x43};
    static constexpr std::array<uint8_t, 2> LSF_SYNC_WORD = {0x55, 0xF7};
    static constexpr std::array<uint8_t, 2> DATA_SYNC_WORD = {0xFF, 0x5D};


    std::shared_ptr<audio_queue_t> audio_queue_;        // Input queue.
    std::shared_ptr<bitstream_queue_t> bitstream_queue_;  // Output queue.
    std::atomic<State> state_;
    struct CODEC2* codec2_ = nullptr;
    M17ByteRandomizer<46> randomizer_;
    PolynomialInterleaver<45, 92, 368> interleaver_;
    CRC16<0x5935, 0xFFFF> crc_;
    LinkSetupFrame::encoded_call_t source_;
    LinkSetupFrame::encoded_call_t dest_;

    static LinkSetupFrame::encoded_call_t encode_callsign(std::string callsign)
    {
        LinkSetupFrame::encoded_call_t encoded_call = {0xff,0xff,0xff,0xff,0xff,0xff};

        if (callsign.empty() || callsign.size() > 9) return encoded_call;

        mobilinkd::LinkSetupFrame::call_t call;
        call.fill(0);
        std::copy(callsign.begin(), callsign.end(), call.begin());
        encoded_call = LinkSetupFrame::encode_callsign(call);
        return encoded_call;
    }

    static constexpr int8_t bits_to_symbol(uint8_t bits)
    {
        switch (bits)
        {
        case 0: return 1;
        case 1: return 3;
        case 2: return -1;
        case 3: return -3;
        }
        return 0;
    }

    template <typename T, size_t N>
    static std::array<int8_t, N / 2> bits_to_symbols(const std::array<T, N>& bits)
    {
        std::array<int8_t, N / 2> result;
        size_t index = 0;
        for (size_t i = 0; i != N; i += 2)
        {
            result[index++] = bits_to_symbol((bits[i] << 1) | bits[i + 1]);
        }
        return result;
    }

    void output_frame(std::array<uint8_t, 2> sync_word, const frame_t& frame)
    {
        for (auto c : sync_word) bitstream_queue_->put(c);
        for (auto c : frame) bitstream_queue_->put(c);
    }

    void send_preamble()
    {
        // Preamble is simple... bytes -> symbols.
        std::array<uint8_t, 48> preamble_bytes;
        preamble_bytes.fill(0x77);
        for (auto c : preamble_bytes) bitstream_queue_->put(c);
    }

    template <typename T, size_t N>
    static std::array<T, N * 2 + 1> conv_encode(std::array<T, N> data)
    {
        std::array<T, N * 2 + 1> result;

        uint8_t bit_index = 0;
        uint8_t byte_index = 0;
        uint8_t tmp = 0;

        uint32_t memory = 0;
        for (auto b : data)
        {
            for (size_t i = 0; i != 8; ++i)
            {
                uint32_t x = (b & 0x80) >> 7;
                b <<= 1;
                memory = update_memory<4>(memory, x);
                tmp = (tmp << 1) | convolve_bit(031, memory);
                tmp = (tmp << 1) | convolve_bit(027, memory);
                bit_index += 2;
                if (bit_index == 8)
                {
                    bit_index = 0;
                    result[byte_index++] = tmp;
                    tmp = 0;
                }
            }
        }

        // Flush the encoder.
        for (size_t i = 0; i != 4; ++i)
        {
            memory = update_memory<4>(memory, 0);
            tmp = (tmp << 1) | convolve_bit(031, memory);
            tmp = (tmp << 1) | convolve_bit(027, memory);
            bit_index += 2;
            if (bit_index == 8)
            {
                bit_index = 0;
                result[byte_index++] = tmp;
                tmp = 0;
            }
        }

        // Frame may not end on a byte boundary.
        if (bit_index != 0)
        {
            while (bit_index++ != 8) tmp <<= 1;
            result[byte_index] = tmp;
        }

        return result;
    }

    /**
     * Encode each LSF segment into a Golay-encoded LICH segment bitstream.
     */
    lich_segment_t make_lich_segment(std::array<uint8_t, 5> segment, uint8_t segment_number)
    {
        lich_segment_t result;
        uint16_t tmp;
        uint32_t encoded;

        tmp = segment[0] << 4 | ((segment[1] >> 4) & 0x0F);
        encoded = mobilinkd::Golay24::encode24(tmp);
        for (size_t i = 0; i != 24; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        tmp = ((segment[1] & 0x0F) << 8) | segment[2];
        encoded = mobilinkd::Golay24::encode24(tmp);
        for (size_t i = 24; i != 48; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        tmp = segment[3] << 4 | ((segment[4] >> 4) & 0x0F);
        encoded = mobilinkd::Golay24::encode24(tmp);
        for (size_t i = 48; i != 72; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        tmp = ((segment[4] & 0x0F) << 8) | (segment_number << 5);
        encoded = mobilinkd::Golay24::encode24(tmp);
        for (size_t i = 72; i != 96; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        return result;
    }

    /**
     * Construct the link setup frame and split into LICH segments.  Output the
     * link setup frame and return the LICH segments to the caller.
     */
    void send_link_setup(lich_t& lich)
    {
        using namespace mobilinkd;

        lsf_t lsf;
        lsf.fill(0);

        auto rit = std::copy(source_.begin(), source_.end(), lsf.begin());
        std::copy(dest_.begin(), dest_.end(), rit);
        lsf[12] = 0;
        lsf[13] = 5;

        crc_.reset();
        for (size_t i = 0; i != 28; ++i)
        {
            crc_(lsf[i]);
        }
        auto checksum = crc_.get_bytes();
        lsf[28] = checksum[0];
        lsf[29] = checksum[1];

        // Build LICH segments
        for (size_t i = 0; i != lich.size(); ++i)
        {
            std::array<uint8_t, 5> segment;
            std::copy(lsf.begin() + i * 5, lsf.begin() + (i + 1) * 5, segment.begin());
            auto lich_segment = make_lich_segment(segment, i);
            std::copy(lich_segment.begin(), lich_segment.end(), lich[i].begin());
        }

        auto encoded = conv_encode(lsf);

        std::array<uint8_t, 46> punctured;
        auto size = puncture_bytes(encoded, punctured, P1);
        assert(size == 368);

        interleaver_.interleave(punctured);
        randomizer_(punctured);
        output_frame(LSF_SYNC_WORD, punctured);
    }

    /**
     * Append the LICH and Convolutionally encoded payload, interleave and randomize
     * the frame bits, and output the frame.
     */
    void send_audio_frame(const lich_segment_t& lich, const payload_t& data)
    {
        using namespace mobilinkd;

        std::array<uint8_t, 46> temp;
        auto it = std::copy(lich.begin(), lich.end(), temp.begin());
        std::copy(data.begin(), data.end(), it);

        interleaver_.interleave(temp);
        randomizer_(temp);
        output_frame(DATA_SYNC_WORD, temp);
    }

    /**
     * Assemble the audio frame payload by appending the frame number, encoded audio,
     * and CRC, then convolutionally coding and puncturing the data.
     */
    payload_t make_payload(uint16_t frame_number, const codec_frame_t& payload)
    {
        std::array<uint8_t, 20> data;   // FN, Audio, CRC = 2 + 16 + 2;
        data[0] = uint8_t((frame_number >> 8) & 0xFF);
        data[1] = uint8_t(frame_number & 0xFF);
        std::copy(payload.begin(), payload.end(), data.begin() + 2);

        crc_.reset();
        for (size_t i = 0; i != 18; ++i) crc_(data[i]);
        auto checksum = crc_.get_bytes();
        data[18] = checksum[0];
        data[19] = checksum[1];

        auto encoded = conv_encode(data);

        payload_t punctured;
        auto size = puncture_bytes(encoded, punctured, mobilinkd::P2);
        assert(size == 272);
        return punctured;
    }

    /**
     * Encode 2 frames of data.  Caller must ensure that the audio is
     * padded with 0s if the incoming data is incomplete.
     */
    codec_frame_t encode_audio(const audio_frame_t& audio)
    {
        codec_frame_t result;
        codec2_encode(codec2_, &result[0], const_cast<int16_t*>(&audio[0]));
        codec2_encode(codec2_, &result[8], const_cast<int16_t*>(&audio[160]));
        return result;
    }

    /**
     * Send the audio frame.  Encodes the audio, assembles the audio frame, and
     * outputs the frame on the queue.
     */
    void send_audio(const lich_segment_t& lich, uint16_t frame_number, const audio_frame_t& audio)
    {
        auto encoded_audio = encode_audio(audio);
        auto payload = make_payload(frame_number, encoded_audio);
        send_audio_frame(lich, payload);
    }

    /**
     * Modulator state machine.  Controls state transitions, ensuring that the
     * M17 stream is sent and terminated appropriately.
     */
    void modulate()
    {
        using namespace std::chrono_literals;
        using clock = std::chrono::steady_clock;

        state_ = State::IDLE;
        codec2_ = ::codec2_create(CODEC2_MODE_3200);

        lich_t lich;
        size_t index = 0;
        uint16_t frame_number = 0;
        uint8_t lich_segment = 0;
        audio_frame_t audio;
        auto current = clock::now();

        audio.fill(0);

        while (audio_queue_->is_open() && bitstream_queue_->is_open())
        {
            int16_t sample;
            if (!(audio_queue_->get(sample, 5s))) sample = 0;    // May be closed.
            if (!(audio_queue_->is_open()))
            {
                std::clog << "audio output queue closed" << std::endl;
                break;
            }
            switch (state_)
            {
            case State::IDLE:
                break;
            case State::PREAMBLE:
                send_preamble();
                state_ = State::LINK_SETUP;
                break;
            case State::LINK_SETUP:
                send_link_setup(lich);
                index = 0;
                frame_number = 0;
                lich_segment = 0;
                state_ = State::ACTIVE;
                current = clock::now();
                break;
            case State::ACTIVE:
                audio[index++] = sample;
                if (index == audio.size())
                {
                    auto now = clock::now();
                    if (now - current > 40ms)
                    {
                        std::clog << "WARNING: packet time exceeded" << std::endl;
                    }
                    current = now;
                    index = 0;
                    send_audio(lich[lich_segment++], frame_number++, audio);
                    if (frame_number == 0x8000) frame_number = 0;
                    if (lich_segment == lich.size()) lich_segment = 0;
                    audio.fill(0);
                }
                break;
            case State::END_OF_STREAM:
                audio[index++] = sample;
                send_audio(lich[lich_segment++], frame_number++, audio);
                audio.fill(0);
                state_ = State::IDLE;
                break;
            default:
                assert(false && "Invalid state");
            }
        }

        ::codec2_destroy(codec2_);
        codec2_ = nullptr;

        if (state_ != State::IDLE) throw std::logic_error("queue closed when not IDLE");

        state_ = State::INACTIVE;
    }

public:

    M17Modulator(const std::string& source, const std::string& dest = "") :
        source_(encode_callsign(source)),
        dest_(encode_callsign(dest))
    {
        state_.store(State::INACTIVE);
    }

    /**
     * Set the source identifier (callsign) for the transmitter.
     */
    void source(const std::string& callsign)
    {
        source_ = encode_callsign(callsign);
    }

    /**
     * Set the destination identifier for the transmitter.  A blank value is
     * interpreted as the broadcast address.  This is the default.
     */
    void dest(const std::string& callsign)
    {
        dest_ = encode_callsign(callsign);
    }

    /**
     * Start the modulator.  This starts a background thread and returns once the thread
     * has started and changed the state to IDLE.
     *
     * @pre state is INACTIVE.
     *
     * @param input is a shared pointer to the audio input queue.
     * @param output is a shared pointer to the symbol output queue.
     * @return a future which is used to return error information to the caller.
     */
    std::future<void> run(const std::shared_ptr<audio_queue_t>& input, const std::shared_ptr<bitstream_queue_t>& output)
    {
        using namespace std::chrono_literals;

        assert(state_ == State::INACTIVE);

        audio_queue_ = input;
        bitstream_queue_ = output;

        auto result = std::async(std::launch::async, [this](){
            this->modulate();
        });

        // Wait until thread is active.
        while (state_ != State::IDLE) std::this_thread::yield();

        return result;
    }

    /**
     * Activate the modulator.  This causes the modulator to transition from IDLE to
     * ACTIVE.  If the modulator is already ACTIVE, no action is taken.  If the modulator
     * is not IDLE, return is delayed until the modulator becomes IDLE (which may take
     * up to 120ms), at which time the modulator is returned to the ACTIVE state.
     * Otherwise the modulator immediately transistions from IDLE to ACTIVE. This will
     * cause the preamble and link setup frames to be emitted.
     *
     * @pre run must have been called.
     * @pre the input queue must be open.
     * @pre the output queue must be open.
     */
    void ptt_on()
    {
        using namespace std::chrono_literals;

        assert(state_ != State::INACTIVE);
        assert(audio_queue_ && audio_queue_->is_open());
        assert(bitstream_queue_ && bitstream_queue_->is_open());

        if (state_ == State::ACTIVE) return;
        while (state_ != State::IDLE && state_ != State::INACTIVE) std::this_thread::sleep_for(1ms);
        assert(state_ == State::IDLE);  // Precondition violated -- one of the queues was closed.
        state_ = State::PREAMBLE;
    }

    /**
     * Stop the modulator.
     *
     * @pre ptt_on() was called and the modulator is in PREAMBLE, LINK_SETUP, or ACTIVE state.
     */
    void ptt_off()
    {
        using namespace std::chrono_literals;

        assert(state_ == State::PREAMBLE | state_ == State::LINK_SETUP | state_ == State::ACTIVE);

        // State must become active before we release PTT to ensure preamble and LSF are sent.
        while (state_ != State::ACTIVE && state_ != State::INACTIVE) std::this_thread::sleep_for(1ms);
        assert(state_ == State::ACTIVE);  // Precondition violated -- one of the queues was closed.
        state_ = State::END_OF_STREAM;
    }

    void wait_until_idle()
    {
        using namespace std::chrono_literals;

        while (state_ != State::IDLE && state_ != State::INACTIVE) std::this_thread::sleep_for(1ms);
    }

    void wait_until_inactive()
    {
        using namespace std::chrono_literals;

        while (state_ != State::INACTIVE) std::this_thread::sleep_for(1ms);
    }

    State state() const { return state_; }

    template <typename T, size_t N>
    static std::array<int8_t, N * 4> bytes_to_symbols(const std::array<T, N>& bytes)
    {
        std::array<int8_t, N * 4> result;
        size_t index = 0;
        for (auto b : bytes)
        {
            for (size_t i = 0; i != 4; ++i)
            {
                result[index++] = bits_to_symbol(b >> 6);
                b <<= 2;
            }
        }
        return result;
    }

    static baseband_t symbols_to_baseband(const symbols_t& symbols)
    {
        // Generated using scikit-commpy
        static const auto rrc_taps = std::array<float, 79>{
            -0.009265784007800534, -0.006136551625729697, -0.001125978562075172, 0.004891777252042491,
            0.01071805138282269, 0.01505751553351295, 0.01679337935001369, 0.015256245142156299,
            0.01042830577908502, 0.003031522725559901,  -0.0055333532968188165, -0.013403099825723372,
            -0.018598682349642525, -0.01944761739590459, -0.015005271935951746, -0.0053887880354343935,
            0.008056525910253532, 0.022816244158307273, 0.035513467692208076, 0.04244131815783876,
            0.04025481153629372, 0.02671818654865632, 0.0013810216516704976, -0.03394615682795165,
            -0.07502635967975885, -0.11540977897637611, -0.14703962203941534, -0.16119995609538576,
            -0.14969512896336504, -0.10610329539459686, -0.026921412469634916, 0.08757875030779196,
            0.23293327870303457, 0.4006012210123992, 0.5786324696325503, 0.7528286479934068,
            0.908262741447522, 1.0309661131633199, 1.1095611856548013, 1.1366197723675815,
            1.1095611856548013, 1.0309661131633199, 0.908262741447522, 0.7528286479934068,
            0.5786324696325503,  0.4006012210123992, 0.23293327870303457, 0.08757875030779196,
            -0.026921412469634916, -0.10610329539459686, -0.14969512896336504, -0.16119995609538576,
            -0.14703962203941534, -0.11540977897637611, -0.07502635967975885, -0.03394615682795165,
            0.0013810216516704976, 0.02671818654865632, 0.04025481153629372,  0.04244131815783876,
            0.035513467692208076, 0.022816244158307273, 0.008056525910253532, -0.0053887880354343935,
            -0.015005271935951746, -0.01944761739590459, -0.018598682349642525, -0.013403099825723372,
            -0.0055333532968188165, 0.003031522725559901, 0.01042830577908502, 0.015256245142156299,
            0.01679337935001369, 0.01505751553351295, 0.01071805138282269, 0.004891777252042491,
            -0.001125978562075172, -0.006136551625729697, -0.009265784007800534
        };
        static BaseFirFilter<std::tuple_size<decltype(rrc_taps)>::value> rrc = makeFirFilter(rrc_taps);

        std::array<int16_t, 1920> baseband;
        baseband.fill(0);
        for (size_t i = 0; i != symbols.size(); ++i)
        {
            baseband[i * 10] = symbols[i];
        }

        for (auto& b : baseband)
        {
            b = rrc(b) * 25;
        }
        return baseband;
    }
};

} // mobilinkd
