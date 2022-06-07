// Copyright 2020-2021 Rob Riggs <rob@mobilinkd.com>
// All rights reserved.

#pragma once

#include "ClockRecovery.h"
#include "Correlator.h"
#include "DataCarrierDetect.h"
#include "FirFilter.h"
#include "FreqDevEstimator.h"
#include "M17FrameDecoder.h"
#include "M17Framer.h"
#include "Util.h"

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <tuple>

namespace mobilinkd {

namespace detail
{


} // detail

template <typename FloatType>
struct M17Demodulator
{
	static const uint16_t SAMPLE_RATE = 48000;
	static const uint16_t SYMBOL_RATE = 4800;
	static const uint16_t SAMPLES_PER_SYMBOL = SAMPLE_RATE / SYMBOL_RATE;
	static const uint16_t BLOCK_SIZE = 192;

	static constexpr FloatType sample_rate = SAMPLE_RATE;
	static constexpr FloatType symbol_rate = SYMBOL_RATE;

	static const uint8_t MAX_MISSING_SYNC = 8;

	using collelator_t = Correlator<FloatType>;
	using sync_word_t = SyncWord<collelator_t>;
	using callback_t = M17FrameDecoder::callback_t;
	using diagnostic_callback_t = std::function<void(bool, FloatType, FloatType, FloatType, int, FloatType, int, int, int, int)>;

	enum class DemodState {
        UNLOCKED,
        LSF_SYNC,
        STREAM_SYNC,
        PACKET_SYNC,
        BERT_SYNC,
        FRAME
    };

	DataCarrierDetect<FloatType, SAMPLE_RATE, 500> dcd{2500, 4000, 1.0, 4.0};
	ClockRecovery<FloatType, SAMPLE_RATE, SYMBOL_RATE> clock_recovery;

	collelator_t correlator;
	sync_word_t preamble_sync{{+3,-3,+3,-3,+3,-3,+3,-3}, 29.f};
	sync_word_t lsf_sync{{+3,+3,+3,+3,-3,-3,+3,-3}, 32.f, -31.f};	// LSF or STREAM (inverted)
	sync_word_t packet_sync{{3,-3,3,3,-3,-3,-3,-3}, 31.f, -31.f};	// PACKET or BERT (inverted)

	FreqDevEstimator<FloatType> dev;
	FloatType idev;
	size_t count_ = 0;

	int8_t polarity = 1;
	M17Framer<368> framer;
	M17FrameDecoder decoder;
	DemodState demodState = DemodState::UNLOCKED;
	M17FrameDecoder::SyncWordType sync_word_type = M17FrameDecoder::SyncWordType::LSF;
	uint8_t sample_index = 0;

	bool dcd_ = false;
	bool need_clock_reset_ = false;
	bool need_clock_update_ = false;

	bool passall_ = false;
	int viterbi_cost = 0;
	int sync_count = 0;
	int missing_sync_count = 0;
	uint8_t sync_sample_index = 0;
	diagnostic_callback_t diagnostic_callback;

	M17Demodulator(callback_t callback)	:
        decoder(callback)
	{}

	virtual ~M17Demodulator() {}

	void dcd_on();
	void dcd_off();
	void initialize(const FloatType input);
	void update_dcd();
	void do_unlocked();
	void do_lsf_sync();
	void do_packet_sync();
	void do_stream_sync();
	void do_bert_sync();
	void do_frame(FloatType filtered_sample);

	bool locked() const
	{
		return dcd_;
	}

	void passall(bool enabled)
	{
	passall_ = enabled;
		// decoder.passall(enabled);
	}

	void diagnostics(diagnostic_callback_t callback)
	{
		diagnostic_callback = callback;
	}

	void update_values(uint8_t index);

	void operator()(const FloatType input);

private:
    static const std::array<FloatType, 150> rrc_taps;
    BaseFirFilter<FloatType, rrc_taps.size()> demod_filter{rrc_taps};
};

template <typename FloatType>
void M17Demodulator<FloatType>::update_values(uint8_t index)
{
	correlator.apply([this,index](FloatType t){dev.sample(t);}, index);
	dev.update();
	sync_sample_index = index;
}

template <typename FloatType>
void M17Demodulator<FloatType>::dcd_on()
{
	// Data carrier newly detected.
	dcd_ = true;
	sync_count = 0;
	missing_sync_count = 0;

	dev.reset();
	framer.reset();
	decoder.reset();
}

template <typename FloatType>
void M17Demodulator<FloatType>::dcd_off()
{
	// Just lost data carrier.
	dcd_ = false;
	demodState = DemodState::UNLOCKED;
    decoder.reset();

    if (diagnostic_callback)
    {
        diagnostic_callback(int(dcd_), dev.error(), dev.deviation(), dev.offset(), (int) demodState,
            clock_recovery.clock_estimate(), sample_index, sync_sample_index, clock_recovery.sample_index(), -1);
    }
}

template <typename FloatType>
void M17Demodulator<FloatType>::initialize(const FloatType input)
{
	auto filtered_sample = demod_filter(input);
	correlator.sample(filtered_sample);
}

template <typename FloatType>
void M17Demodulator<FloatType>::update_dcd()
{
	if (!dcd_ && dcd.dcd())
	{
		// fputs("\nAOS\n", stderr);
		dcd_on();
		need_clock_reset_ = true;
	}
	else if (dcd_ && !dcd.dcd())
	{
		// fputs("\nLOS\n", stderr);
		dcd_off();
	}
}

template <typename FloatType>
void M17Demodulator<FloatType>::do_unlocked()
{
	// We expect to find the preamble immediately after DCD.
	if (missing_sync_count < 1920)
	{
		missing_sync_count += 1;
		auto sync_index = preamble_sync(correlator);
		auto sync_updated = preamble_sync.updated();
		if (sync_updated)
		{
			sync_count = 0;
			missing_sync_count = 0;
			need_clock_reset_ = true;
			dev.reset();
			update_values(sync_index);
			sample_index = sync_index;
			demodState = DemodState::LSF_SYNC;
		}
		return;
	}
	auto sync_index = lsf_sync(correlator);
	auto sync_updated = lsf_sync.updated();
	if (sync_updated)
	{
		sync_count = 0;
		missing_sync_count = 0;
		need_clock_reset_ = true;
		dev.reset();
		update_values(sync_index);
		sample_index = sync_index;
		demodState = DemodState::FRAME;
		if (sync_updated < 0)
		{
			sync_word_type = M17FrameDecoder::SyncWordType::STREAM;
		}
		else
		{
			sync_word_type = M17FrameDecoder::SyncWordType::LSF;
		}
		return;
	}
	sync_index = packet_sync(correlator);
	sync_updated = packet_sync.updated();
	if (sync_updated < 0)
	{
		sync_count = 0;
		missing_sync_count = 0;
		need_clock_reset_ = true;
		dev.reset();
		update_values(sync_index);
		sample_index = sync_index;
		demodState = DemodState::FRAME;
		sync_word_type = M17FrameDecoder::SyncWordType::BERT;
	}
}

/**
 * Check for LSF sync word.  We only enter the DemodState::LSF_SYNC state
 * if a preamble sync has been detected, which also means that sample_index
 * has been initialized to a sane value for the baseband.
 */
template <typename FloatType>
void M17Demodulator<FloatType>::do_lsf_sync()
{
	FloatType sync_triggered = 0.;
	FloatType bert_triggered = 0.;

	if (correlator.index() == sample_index)
	{
		sync_triggered = preamble_sync.triggered(correlator);
		if (sync_triggered > 0.1)
		{
			return;
		}
		sync_triggered = lsf_sync.triggered(correlator);
		bert_triggered = packet_sync.triggered(correlator);
		if (bert_triggered < 0)
		{
			missing_sync_count = 0;
			need_clock_update_ = true;
			update_values(sample_index);
			demodState = DemodState::FRAME;
			sync_word_type = M17FrameDecoder::SyncWordType::BERT;
		}
		else if (std::abs(sync_triggered) > 0.1)
		{
			missing_sync_count = 0;
			need_clock_update_ = true;
			update_values(sample_index);
			if (sync_triggered > 0)
			{
				demodState = DemodState::FRAME;
				sync_word_type = M17FrameDecoder::SyncWordType::LSF;
			}
			else
			{
				demodState = DemodState::FRAME;
				sync_word_type = M17FrameDecoder::SyncWordType::STREAM;
			}
		}
		else if (++missing_sync_count > 192)
		{
			demodState = DemodState::UNLOCKED;
            decoder.reset();
			missing_sync_count = 0;
		}
		else
		{
			update_values(sample_index);
		}
	}
}

/**
 * Check for a stream sync word (LSF sync word that is maximally negative).
 * We can enter DemodState::STREAM_SYNC from either a valid LSF decode for
 * an audio stream, or from a stream frame decode.
 *
 */
template <typename FloatType>
void M17Demodulator<FloatType>::do_stream_sync()
{
	uint8_t sync_index = lsf_sync(correlator);
	int8_t sync_updated = lsf_sync.updated();
	sync_count += 1;
	if (sync_updated < 0)   // Stream sync word
	{
		missing_sync_count = 0;
		if (sync_count > 70)
		{
			update_values(sync_index);
			sync_word_type = M17FrameDecoder::SyncWordType::STREAM;
			demodState = DemodState::FRAME;
		}
		return;
	}
	else if (sync_count > 87)
	{
		update_values(sync_index);
		missing_sync_count += 1;
		if (missing_sync_count < MAX_MISSING_SYNC)
		{
			sync_word_type = M17FrameDecoder::SyncWordType::STREAM;
			demodState = DemodState::FRAME;
		}
		else
		{
			// fputs("\n!SYNC\n", stderr);
			demodState = DemodState::LSF_SYNC;
		}
	}
}

/**
 * Check for a packet sync word.  DemodState::PACKET_SYNC can only be
 * entered from a valid LSF frame decode with the data/packet type bit set.
 */
template <typename FloatType>
void M17Demodulator<FloatType>::do_packet_sync()
{
	auto sync_index = packet_sync(correlator);
	auto sync_updated = packet_sync.updated();
	sync_count += 1;
	if (sync_count > 70 && sync_updated)
	{
		missing_sync_count = 0;
		update_values(sync_index);
		sync_word_type = M17FrameDecoder::SyncWordType::PACKET;
		demodState = DemodState::FRAME;
	}
	else if (sync_count > 87)
	{
		missing_sync_count += 1;
		if (missing_sync_count < MAX_MISSING_SYNC)
		{
			sync_word_type = M17FrameDecoder::SyncWordType::PACKET;
			demodState = DemodState::FRAME;
		}
		else
		{
			demodState = DemodState::UNLOCKED;
            decoder.reset();
		}
	}
}

/**
 * Check for a bert sync word.
 */
template <typename FloatType>
void M17Demodulator<FloatType>::do_bert_sync()
{
	auto sync_index = packet_sync(correlator);
	auto sync_updated = packet_sync.updated();
	sync_count += 1;
	if (sync_count > 70 && sync_updated < 0)
	{
		missing_sync_count = 0;
		update_values(sync_index);
		sync_word_type = M17FrameDecoder::SyncWordType::BERT;
		demodState = DemodState::FRAME;
	}
	else if (sync_count > 87)
	{
		missing_sync_count += 1;
		if (missing_sync_count < MAX_MISSING_SYNC)
		{
			sync_word_type = M17FrameDecoder::SyncWordType::BERT;
			demodState = DemodState::FRAME;
		}
		else
		{
			demodState = DemodState::UNLOCKED;
            decoder.reset();
		}
	}
}


template <typename FloatType>
void M17Demodulator<FloatType>::do_frame(FloatType filtered_sample)
{
	if (correlator.index() != sample_index) return;

	static uint8_t cost_count = 0;

	auto sample = filtered_sample - dev.offset();
	sample *= dev.idev();
	sample *= polarity;

	auto n = llr<FloatType, 4>(sample);
	int8_t* tmp;
	auto len = framer(n, &tmp);
	if (len != 0)
	{
		need_clock_update_ = true;

		M17FrameDecoder::input_buffer_t buffer;
		std::copy(tmp, tmp + len, buffer.begin());
		auto valid = decoder(sync_word_type, buffer, viterbi_cost);

		cost_count = viterbi_cost > 90 ? cost_count + 1 : 0;
		cost_count = viterbi_cost > 100 ? cost_count + 1 : cost_count;
		cost_count = viterbi_cost > 110 ? cost_count + 1 : cost_count;

		if (cost_count > 75)
		{
			cost_count = 0;
			demodState = DemodState::UNLOCKED;
            decoder.reset();
			// fputs("\nCOST\n", stderr);
			return;
		}

		switch (decoder.state())
		{
		case M17FrameDecoder::State::STREAM:
			demodState = DemodState::STREAM_SYNC;
			break;
		case M17FrameDecoder::State::LSF:
			// If state == LSF, we need to recover LSF from LICH.
			demodState = DemodState::STREAM_SYNC;
			break;
		case M17FrameDecoder::State::BERT:
			demodState = DemodState::BERT_SYNC;
			break;
		default:
			demodState = DemodState::PACKET_SYNC;
			break;
		}

		sync_count = 0;

		switch (valid)
		{
		case M17FrameDecoder::DecodeResult::FAIL:
			break;
		case M17FrameDecoder::DecodeResult::EOS:
			demodState = DemodState::LSF_SYNC;
			break;
		case M17FrameDecoder::DecodeResult::OK:
			break;
		case M17FrameDecoder::DecodeResult::INCOMPLETE:
			break;
        case M17FrameDecoder::DecodeResult::PACKET_INCOMPLETE:
            break;
		}
	}
}

template <typename FloatType>
void M17Demodulator<FloatType>::operator()(const FloatType input)
{
	static int16_t initializing = 1920;

	count_++;

	dcd(input);

	// We need to pump a few ms of data through on startup to initialize
	// the demodulator.
	if (initializing) [[unlikely]]
	{
		--initializing;
		initialize(input);
		count_ = 0;
		return;
	}

	if (!dcd_)
	{
		if (count_ % (BLOCK_SIZE * 2) == 0)
		{
			update_dcd();
			dcd.update();

			if (diagnostic_callback)
			{
				diagnostic_callback(int(dcd_), dev.error(), dev.deviation(), dev.offset(), (int) demodState,
					clock_recovery.clock_estimate(), sample_index, sync_sample_index, clock_recovery.sample_index(), viterbi_cost);
			}

            count_ = 0;
		}

		return;
	}

	auto filtered_sample = demod_filter(input);

	correlator.sample(filtered_sample);

	if (correlator.index() == 0)
	{
		if (need_clock_reset_)
		{
			clock_recovery.reset();
			need_clock_reset_ = false;
		}
		else if (need_clock_update_) // must avoid update immediately after reset.
		{
			clock_recovery.update();
			uint8_t clock_index = clock_recovery.sample_index();
			uint8_t clock_diff = std::abs(sample_index - clock_index);
			uint8_t sync_diff = std::abs(sample_index - sync_sample_index);
			bool clock_diff_ok = clock_diff <= 1 || clock_diff == 9;
			bool sync_diff_ok = sync_diff <= 1 || sync_diff == 9;
			if (clock_diff_ok) sample_index = clock_index;
			else if (sync_diff_ok) sample_index = sync_sample_index;
			// else unchanged.
			need_clock_update_ = false;
		}
	}

	clock_recovery(filtered_sample);

	if (demodState != DemodState::UNLOCKED && correlator.index() == sample_index)
	{
		dev.sample(filtered_sample);
	}

	switch (demodState)
	{
	case DemodState::UNLOCKED:
		// In this state, the sample_index is unknown.  We need to find
		// a sync word to find the proper sample_index.  We only leave
		// this state if we believe that we have a valid sample_index.
		do_unlocked();
		break;
	case DemodState::LSF_SYNC:
		do_lsf_sync();
		break;
	case DemodState::STREAM_SYNC:
		do_stream_sync();
		break;
	case DemodState::PACKET_SYNC:
		do_packet_sync();
		break;
	case DemodState::BERT_SYNC:
		do_bert_sync();
		break;
	case DemodState::FRAME:
		do_frame(filtered_sample);
		break;
	}

	if (count_ % (BLOCK_SIZE * 5) == 0)
	{
		update_dcd();
		count_ = 0;

		if (diagnostic_callback)
		{
			diagnostic_callback(int(dcd_), dev.error(), dev.deviation(), dev.offset(), (int) demodState,
				clock_recovery.clock_estimate(), sample_index, sync_sample_index, clock_recovery.sample_index(), viterbi_cost);
		}

        dcd.update();
	}
}

} // mobilinkd
