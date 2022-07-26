#include <QDebug>
#include <QString>

#include "M17Demodulator.h"

namespace modemm17 {

// const std::array<float, 150> M17Demodulator::rrc_taps = std::array<float, 150>{
//      0.0029364388513841593,  0.0031468394550958484,  0.002699564567597445,   0.001661182944400927,   0.00023319405581230247,
//     -0.0012851320781224025, -0.0025577136087664687, -0.0032843366522956313, -0.0032697038088887226, -0.0024733964729590865,
//     -0.0010285696910973807,  0.0007766690889758685,  0.002553421969211845,   0.0038920145144327816,  0.004451886520053017,
//      0.00404219185231544,    0.002674727068399207,   0.0005756567993179152, -0.0018493784971116507, -0.004092346891623224,
//     -0.005648131453822014,  -0.006126925416243605,  -0.005349511529163396,  -0.003403189203405097,  -0.0006430502751187517,
//      0.002365929161655135,   0.004957956568090113,   0.006506845894531803,   0.006569574194782443,   0.0050017573119839134,
//      0.002017321931508163,  -0.0018256054303579805, -0.00571615173291049,   -0.008746639552588416,  -0.010105075751866371,
//     -0.009265784007800534,  -0.006136551625729697,  -0.001125978562075172,   0.004891777252042491,   0.01071805138282269,
//      0.01505751553351295,    0.01679337935001369,    0.015256245142156299,   0.01042830577908502,    0.003031522725559901,
//     -0.0055333532968188165, -0.013403099825723372,  -0.018598682349642525,  -0.01944761739590459,   -0.015005271935951746,
//     -0.0053887880354343935,  0.008056525910253532,   0.022816244158307273,   0.035513467692208076,   0.04244131815783876,
//      0.04025481153629372,    0.02671818654865632,    0.0013810216516704976, -0.03394615682795165,   -0.07502635967975885,
//     -0.11540977897637611,   -0.14703962203941534,   -0.16119995609538576,   -0.14969512896336504,   -0.10610329539459686,
//     -0.026921412469634916,   0.08757875030779196,    0.23293327870303457,    0.4006012210123992,     0.5786324696325503,
//      0.7528286479934068,     0.908262741447522,      1.0309661131633199,     1.1095611856548013,     1.1366197723675815,
//      1.1095611856548013,     1.0309661131633199,     0.908262741447522,      0.7528286479934068,     0.5786324696325503,
//      0.4006012210123992,     0.23293327870303457,    0.08757875030779196,   -0.026921412469634916,  -0.10610329539459686,
//     -0.14969512896336504,   -0.16119995609538576,   -0.14703962203941534,   -0.11540977897637611,   -0.07502635967975885,
//     -0.03394615682795165,    0.0013810216516704976,  0.02671818654865632,    0.04025481153629372,    0.04244131815783876,
//      0.035513467692208076,   0.022816244158307273,   0.008056525910253532,  -0.0053887880354343935, -0.015005271935951746,
//     -0.01944761739590459,   -0.018598682349642525,  -0.013403099825723372,  -0.0055333532968188165,  0.003031522725559901,
//      0.01042830577908502,    0.015256245142156299,   0.01679337935001369,    0.01505751553351295,    0.01071805138282269,
//      0.004891777252042491,  -0.001125978562075172,  -0.006136551625729697,  -0.009265784007800534,  -0.010105075751866371,
//     -0.008746639552588416,  -0.00571615173291049,   -0.0018256054303579805,  0.002017321931508163,   0.0050017573119839134,
//      0.006569574194782443,   0.006506845894531803,   0.004957956568090113,   0.002365929161655135,  -0.0006430502751187517,
//     -0.003403189203405097,  -0.005349511529163396,  -0.006126925416243605,  -0.005648131453822014,  -0.004092346891623224,
//     -0.0018493784971116507,  0.0005756567993179152,  0.002674727068399207,   0.00404219185231544,    0.004451886520053017,
//      0.0038920145144327816,  0.002553421969211845,   0.0007766690889758685, -0.0010285696910973807, -0.0024733964729590865,
//     -0.0032697038088887226, -0.0032843366522956313, -0.0025577136087664687, -0.0012851320781224025,  0.00023319405581230247,
//     0.001661182944400927,    0.002699564567597445,   0.0031468394550958484,  0.0029364388513841593,  0.0
// };

// Generated using scikit-commpy N = 150, aplha = 0.5, Ts = 1/4800 s, Fs = 48000 Hz
const std::array<float, 150> M17Demodulator::rrc_taps = std::array<float, 150>{
    -8.434122e-04, +3.898184e-04, +1.628891e-03, +2.576674e-03, +2.987740e-03,
    +2.729505e-03, +1.820181e-03, +4.333001e-04, -1.134215e-03, -2.525029e-03,
    -3.402452e-03, -3.531573e-03, -2.841363e-03, -1.451929e-03, +3.417005e-04,
    +2.128236e-03, +3.470212e-03, +4.006361e-03, +3.543024e-03, +2.112321e-03,
    -1.893023e-05, -2.395144e-03, -4.465932e-03, -5.709548e-03, -5.759027e-03,
    -4.501582e-03, -2.125844e-03, +8.982825e-04, +3.907892e-03, +6.188389e-03,
    +7.139194e-03, +6.427125e-03, +4.090469e-03, +5.649447e-04, -3.381677e-03,
    -6.799652e-03, -8.765190e-03, -8.603529e-03, -6.076811e-03, -1.489520e-03,
    +4.321674e-03, +1.012385e-02, +1.453219e-02, +1.631886e-02, +1.472302e-02,
    +9.691259e-03, +1.984723e-03, -6.888307e-03, -1.492227e-02, -2.001531e-02,
    -2.045303e-02, -1.538011e-02, -5.154591e-03, +8.509368e-03, +2.267330e-02,
    +3.359618e-02, +3.740502e-02, +3.091849e-02, +1.248579e-02, -1.731807e-02,
    -5.529141e-02, -9.561492e-02, -1.303248e-01, -1.502279e-01, -1.461577e-01,
    -1.104008e-01, -3.808012e-02, +7.173159e-02, +2.153664e-01, +3.845237e-01,
    +5.668902e-01, +7.473693e-01, +9.097718e-01, +1.038746e+00, +1.121677e+00,
    +1.150282e+00, +1.121677e+00, +1.038746e+00, +9.097718e-01, +7.473693e-01,
    +5.668902e-01, +3.845237e-01, +2.153664e-01, +7.173159e-02, -3.808012e-02,
    -1.104008e-01, -1.461577e-01, -1.502279e-01, -1.303248e-01, -9.561492e-02,
    -5.529141e-02, -1.731807e-02, +1.248579e-02, +3.091849e-02, +3.740502e-02,
    +3.359618e-02, +2.267330e-02, +8.509368e-03, -5.154591e-03, -1.538011e-02,
    -2.045303e-02, -2.001531e-02, -1.492227e-02, -6.888307e-03, +1.984723e-03,
    +9.691259e-03, +1.472302e-02, +1.631886e-02, +1.453219e-02, +1.012385e-02,
    +4.321674e-03, -1.489520e-03, -6.076811e-03, -8.603529e-03, -8.765190e-03,
    -6.799652e-03, -3.381677e-03, +5.649447e-04, +4.090469e-03, +6.427125e-03,
    +7.139194e-03, +6.188389e-03, +3.907892e-03, +8.982825e-04, -2.125844e-03,
    -4.501582e-03, -5.759027e-03, -5.709548e-03, -4.465932e-03, -2.395144e-03,
    -1.893023e-05, +2.112321e-03, +3.543024e-03, +4.006361e-03, +3.470212e-03,
    +2.128236e-03, +3.417005e-04, -1.451929e-03, -2.841363e-03, -3.531573e-03,
    -3.402452e-03, -2.525029e-03, -1.134215e-03, +4.333001e-04, +1.820181e-03,
    +2.729505e-03, +2.987740e-03, +2.576674e-03, +1.628891e-03, +3.898184e-04,
};

void M17Demodulator::update_values(uint8_t index)
{
	correlator.apply([this](float t){dev.sample(t);}, index);
	dev.update();
	sync_sample_index = index;
}

void M17Demodulator::dcd_on()
{
	// Data carrier newly detected.
	dcd_ = true;
	sync_count = 0;
	missing_sync_count = 0;

	dev.reset();
	framer.reset();
	decoder.reset();
}

void M17Demodulator::dcd_off()
{
	// Just lost data carrier.
	dcd_ = false;
	demodState = DemodState::UNLOCKED;
    decoder.reset();

    if (diagnostic_callback)
    {
        diagnostic_callback(
            dcd_,
            dev.error(),
            dev.deviation(),
            dev.offset(),
            (int) demodState,
            (int) sync_word_type,
            clock_recovery.clock_estimate(),
            sample_index,
            sync_sample_index,
            clock_recovery.sample_index(),
            -1
        );
    }
}

void M17Demodulator::initialize(const float input)
{
	auto filtered_sample = demod_filter(input);
	correlator.sample(filtered_sample);
}

void M17Demodulator::update_dcd()
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

void M17Demodulator::do_unlocked()
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

		if (sync_updated < 0) {
			sync_word_type = M17FrameDecoder::SyncWordType::STREAM;
		} else {
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
void M17Demodulator::do_lsf_sync()
{
	float sync_triggered = 0.;
	float bert_triggered = 0.;

	if (correlator.index() == sample_index)
	{
		sync_triggered = preamble_sync.triggered(correlator);

		if (sync_triggered > 0.1) {
			return;
		}

		sync_triggered = lsf_sync.triggered(correlator);
		bert_triggered = packet_sync.triggered(correlator);

        if (sync_triggered != 0) {
            qDebug() << "modemm17::M17Demodulator::do_lsf_sync: sync_triggered:" << sync_triggered;
        }

        if (bert_triggered != 0) {
            qDebug() << "modemm17::M17Demodulator::do_lsf_sync: bert_triggered:" << bert_triggered;
        }

        if (bert_triggered < 0)
		{
			missing_sync_count = 0;
			need_clock_update_ = true;
			update_values(sample_index);
			demodState = DemodState::FRAME;
			sync_word_type = M17FrameDecoder::SyncWordType::BERT;
            qDebug() << "modemm17::M17Demodulator::do_lsf_sync: BERT:" << (int) sync_word_type;
		}
        else if (bert_triggered > 0)
        {
			missing_sync_count = 0;
			need_clock_update_ = true;
			update_values(sample_index);
			demodState = DemodState::FRAME;
			sync_word_type = M17FrameDecoder::SyncWordType::PACKET;
            qDebug() << "modemm17::M17Demodulator::do_lsf_sync: PACKET:" << (int) sync_word_type;
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
                qDebug() << "modemm17::M17Demodulator::do_lsf_sync: LSF:" << (int) sync_word_type;
			}
			else
			{
				demodState = DemodState::FRAME;
				sync_word_type = M17FrameDecoder::SyncWordType::STREAM;
                qDebug() << "modemm17::M17Demodulator::do_lsf_sync: STREAM:" << (int) sync_word_type;
			}
		}
		else if (++missing_sync_count > 192)
		{
			demodState = DemodState::UNLOCKED;
            decoder.reset();
			missing_sync_count = 0;
            qDebug() << "modemm17::M17Demodulator::do_lsf_sync: UNLOCKED:" << (int) sync_word_type;
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
void M17Demodulator::do_stream_sync()
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
void M17Demodulator::do_packet_sync()
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
void M17Demodulator::do_bert_sync()
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


void M17Demodulator::do_frame(float filtered_sample)
{
	if (correlator.index() != sample_index) {
        return;
    }

	static uint8_t cost_count = 0;
	auto sample = filtered_sample - dev.offset();
	sample *= dev.idev();
	sample *= polarity;
	auto n = llr<4>(sample);
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
        // qDebug() << "modemm17::M17Demodulator::do_frame: "
        //     << "sync_word_type:" << (int) sync_word_type
        //     << " len:" << len
        //     << " viterbi_cost: " << viterbi_cost
        //     << " cost_count" << cost_count;

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

void M17Demodulator::operator()(const float input)
{
	count_++;
	dcd(input);

	// We need to pump a few ms of data through on startup to initialize
	// the demodulator.
	if (initializing_count_)
	{
		--initializing_count_;
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
				diagnostic_callback(
                    dcd_,
                    dev.error(),
                    dev.deviation(),
                    dev.offset(),
                    (int) demodState,
                    (int) sync_word_type,
					clock_recovery.clock_estimate(),
                    sample_index,
                    sync_sample_index,
                    clock_recovery.sample_index(),
                    viterbi_cost
                );
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

            if (clock_diff_ok) {
                sample_index = clock_index;
            } else if (sync_diff_ok) {
                sample_index = sync_sample_index;
            }

            // else unchanged.
			need_clock_update_ = false;
		}
	}

	clock_recovery(filtered_sample);

	if (demodState != DemodState::UNLOCKED && correlator.index() == sample_index) {
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
			diagnostic_callback(
                dcd_,
                dev.error(),
                dev.deviation(),
                dev.offset(),
                (int) demodState,
                (int) sync_word_type,
				clock_recovery.clock_estimate(),
                sample_index,
                sync_sample_index,
                clock_recovery.sample_index(),
                viterbi_cost
            );
		}

        dcd.update();
	}
}

} // modemm17
