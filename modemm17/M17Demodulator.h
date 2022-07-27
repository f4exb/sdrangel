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
#include <tuple>

#include "export.h"

namespace modemm17 {

struct MODEMM17_API M17Demodulator
{
	static const uint16_t SAMPLE_RATE = 48000;
	static const uint16_t SYMBOL_RATE = 4800;
	static const uint16_t SAMPLES_PER_SYMBOL = SAMPLE_RATE / SYMBOL_RATE;
	static const uint16_t BLOCK_SIZE = 192;
	static const uint8_t MAX_MISSING_SYNC = 8;

	using callback_t = M17FrameDecoder::callback_t;
	using diagnostic_callback_t = std::function<void(bool, float, float, float, int, int, float, int, int, int, int)>;

	enum class DemodState {
        UNLOCKED,
        LSF_SYNC,
        STREAM_SYNC,
        PACKET_SYNC,
        BERT_SYNC,
        FRAME
    };

	DataCarrierDetect<SAMPLE_RATE, 500> dcd{2500, 4000, 1.0, 4.0};
	ClockRecovery clock_recovery;

	SyncWord preamble_sync{{+3, -3, +3, -3, +3, -3, +3, -3}, 29.f};
	SyncWord lsf_sync{     {+3, +3, +3, +3, -3, -3, +3, -3}, 32.f, -31.f};	// LSF or STREAM (inverted)
	SyncWord packet_sync{  {+3, -3, +3, +3, -3, -3, -3, -3}, 31.f, -31.f};	// PACKET or BERT (inverted)

	FreqDevEstimator dev;
	float idev;
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
        clock_recovery(SAMPLE_RATE, SYMBOL_RATE),
        decoder(callback),
        initializing_count_(1920)
	{
        demodState = DemodState::UNLOCKED;
    }

	virtual ~M17Demodulator() {}

	void dcd_on();
	void dcd_off();
	void initialize(const float input);
	void update_dcd();
	void do_unlocked();
	void do_lsf_sync();
	void do_packet_sync();
	void do_stream_sync();
	void do_bert_sync();
	void do_frame(float filtered_sample);

	bool locked() const {
		return dcd_;
	}

	void passall(bool enabled) {
	    passall_ = enabled;
		// decoder.passall(enabled);
	}

	void diagnostics(diagnostic_callback_t callback)
	{
		diagnostic_callback = callback;
	}

	void update_values(uint8_t index);
	void operator()(const float input);

private:
    static const std::array<float, 150> rrc_taps;
    BaseFirFilter<rrc_taps.size()> demod_filter{rrc_taps};
  	Correlator correlator;
    int16_t initializing_count_;
};

} // modemm17
