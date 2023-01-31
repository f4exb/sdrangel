///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
// reformatted and adapted to Qt and SDRangel context                            //
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
#ifndef ft8_h
#define ft8_h

#include <vector>

#include <QObject>
#include <QMutex>
#include <QString>

#include "fft.h"
#include "export.h"

class QThread;

namespace FT8 {
// Callback interface to get the results
class FT8_API CallbackInterface
{
public:
    virtual int hcb(
        int *a91,
        float hz0,
        float off,
        const char *,
        float snr,
        int pass,
        int correct_bits
    ) = 0; //!< virtual nathod called each time there is a result
    virtual QString get_name() = 0;
};

//
// manage statistics for soft decoding, to help
// decide how likely each symbol is to be correct,
// to drive LDPC decoding.
//
// meaning of the how (problt_how) parameter:
// 0: gaussian
// 1: index into the actual distribution
// 2: do something complex for the tails.
// 3: index into the actual distribution plus gaussian for tails.
// 4: similar to 3.
// 5: laplace
//
class FT8_API Stats
{
public:
    std::vector<float> a_;
    float sum_;
    bool finalized_;
    float mean_;   // cached
    float stddev_; // cached
    float b_;      // cached
    int how_;

public:
    Stats(int how, float log_tail, float log_rate);
    void add(float x);
    void finalize();
    float mean();
    float stddev();

    // fraction of distribution that's less than x.
    // assumes normal distribution.
    // this is PHI(x), or the CDF at x,
    // or the integral from -infinity
    // to x of the PDF.
    float gaussian_problt(float x);
    // https://en.wikipedia.org/wiki/Laplace_distribution
    // m and b from page 116 of Mark Owen's Practical Signal Processing.
    float laplace_problt(float x);
    // look into the actual distribution.
    float problt(float x);

private:
    float log_tail_;
    float log_rate_;
};

class FT8_API Strength
{
public:
    float hz_;
    int off_;
    float strength_; // higher is better
};

// same as Python class CDECODE
//
struct FT8_API cdecode
{
    float hz0;
    float hz1;
    float off;
    int *bits; // 174
};

// 1920-point FFT at 12000 samples/second
// 6.25 Hz spacing, 0.16 seconds/symbol
// encode chain:
//   77 bits
//   append 14 bits CRC (for 91 bits)
//   LDPC(174,91) yields 174 bits
//   that's 58 3-bit FSK-8 symbols
//   gray code each 3 bits
//   insert three 7-symbol Costas sync arrays
//     at symbol #s 0, 36, 72 of final signal
//   thus: 79 FSK-8 symbols
// total transmission time is 12.64 seconds

// tunable parameters
struct FT8_API FT8Params
{
    int nthreads;            // number of parallel threads, for multi-core
    int npasses_one;         // number of spectral subtraction passes
    int npasses_two;         // number of spectral subtraction passes
    int ldpc_iters;          // how hard LDPC decoding should work
    int snr_win;             // averaging window, in symbols, for SNR conversion
    int snr_how;             // technique to measure "N" for SNR. 0 means median of the 8 tones.
    float shoulder200;       // for 200 sps bandpass filter
    float shoulder200_extra; // for bandpass filter
    float second_hz_win;     // +/- hz
    int second_hz_n;         // divide total window into this many pieces
    float second_off_win;    // +/- search window in symbol-times
    int second_off_n;
    int third_hz_n;
    float third_hz_win;
    int third_off_n;
    float third_off_win;
    float log_tail;
    float log_rate;
    int problt_how_noise;
    int problt_how_sig;
    int use_apriori;
    int use_hints;           // 1 means use all hints, 2 means just CQ hints
    int win_type;
    int use_osd;
    int osd_depth;           // 6; // don't increase beyond 6, produces too much garbage
    int osd_ldpc_thresh;     // demand this many correct LDPC parity bits before OSD
    int ncoarse;             // number of offsets per hz produced by coarse()
    int ncoarse_blocks;
    float tminus;            // start looking at 0.5 - tminus seconds
    float tplus;
    int coarse_off_n;
    int coarse_hz_n;
    float already_hz;
    float overlap;
    int overlap_edges;
    float nyquist;
    int oddrate;
    float pass0_frac;
    int reduce_how;
    float go_extra;
    int do_reduce;
    int pass_threshold;
    int strength_how;
    int known_strength_how;
    int coarse_strength_how;
    float reduce_shoulder;
    float reduce_factor;
    float reduce_extra;
    float coarse_all;
    int second_count;
    int soft_phase_win;
    float subtract_ramp;
    int soft_ones;
    int soft_pairs;
    int soft_triples;
    int do_second;
    int do_fine_hz;
    int do_fine_off;
    int do_third;
    float fine_thresh;
    int fine_max_off;
    int fine_max_tone;
    int known_sparse;
    float c_soft_weight;
    int c_soft_win;
    int bayes_how;

    FT8Params()
    {
        nthreads = 8;            // number of parallel threads, for multi-core
        npasses_one = 3;         // number of spectral subtraction passes
        npasses_two = 3;         // number of spectral subtraction passes
        ldpc_iters = 25;         // how hard LDPC decoding should work
        snr_win = 7;             // averaging window, in symbols, for SNR conversion
        snr_how = 3;             // technique to measure "N" for SNR. 0 means median of the 8 tones.
        shoulder200 = 10;        // for 200 sps bandpass filter
        shoulder200_extra = 0.0; // for bandpass filter
        second_hz_win = 3.5;     // +/- hz
        second_hz_n = 8;         // divide total window into this many pieces
        second_off_win = 0.5;    // +/- search window in symbol-times
        second_off_n = 10;
        third_hz_n = 3;
        third_hz_win = 0.25;
        third_off_n = 4;
        third_off_win = 0.075;
        log_tail = 0.1;
        log_rate = 8.0;
        problt_how_noise = 0;
        problt_how_sig = 0;
        use_apriori = 1;
        use_hints = 2;           // 1 means use all hints, 2 means just CQ hints
        win_type = 1;
        use_osd = 1;
        osd_depth = 0;           // 6; // don't increase beyond 6, produces too much garbage
        osd_ldpc_thresh = 70;    // demand this many correct LDPC parity bits before OSD
        ncoarse = 1;             // number of offsets per hz produced by coarse()
        ncoarse_blocks = 1;
        tminus = 2.2;            // start looking at 0.5 - tminus seconds
        tplus = 2.4;
        coarse_off_n = 4;
        coarse_hz_n = 4;
        already_hz = 27;
        overlap = 20;
        overlap_edges = 0;
        nyquist = 0.925;
        oddrate = 1;
        pass0_frac = 1.0;
        reduce_how = 2;
        go_extra = 3.5;
        do_reduce = 1;
        pass_threshold = 1;
        strength_how = 4;
        known_strength_how = 7;
        coarse_strength_how = 6;
        reduce_shoulder = -1;
        reduce_factor = 0.25;
        reduce_extra = 0;
        coarse_all = -1;
        second_count = 3;
        soft_phase_win = 2;
        subtract_ramp = 0.11;
        soft_ones = 2;
        soft_pairs = 1;
        soft_triples = 1;
        do_second = 1;
        do_fine_hz = 1;
        do_fine_off = 1;
        do_third = 2;
        fine_thresh = 0.19;
        fine_max_off = 2;
        fine_max_tone = 4;
        known_sparse = 1;
        c_soft_weight = 7;
        c_soft_win = 2;
        bayes_how = 1;
    }
}; // class FT8Params

// The FT8 worker
class FT8_API FT8 : public QObject
{
    Q_OBJECT
public:
    FT8(
        const std::vector<float> &samples,
        float min_hz,
        float max_hz,
        int start,
        int rate,
        int hints1[],
        int hints2[],
        double deadline,
        double final_deadline,
        CallbackInterface *cb,
        std::vector<cdecode> prevdecs,
        FFTEngine *fftEngine
    );
    ~FT8();
    // Number of passes
    void set_npasses(int npasses) { npasses_ = npasses; }
    // Start the worker
    void start_work();
    // strength of costas block of signal with tone 0 at bi0,
    // and symbol zero at si0.
    float one_coarse_strength(const FFTEngine::ffts_t &bins, int bi0, int si0);
    // return symbol length in samples at the given rate.
    // insist on integer symbol lengths so that we can
    // use whole FFT bins.
    int blocksize(int rate);
    //
    // look for potential signals by searching FFT bins for Costas symbol
    // blocks. returns a vector of candidate positions.
    //
    std::vector<Strength> coarse(const FFTEngine::ffts_t &bins, int si0, int si1);

    FT8Params& getParams() { return params; }

private:
    //
    // reduce the sample rate from arate to brate.
    // center hz0..hz1 in the new nyquist range.
    // but first filter to that range.
    // sets delta_hz to hz moved down.
    //
    std::vector<float> reduce_rate(
        const std::vector<float> &a,
        float hz0,
        float hz1,
        int arate,
        int brate,
        float &delta_hz
    );
    // The actual main process
    void go(int npasses);
    //
    // what's the strength of the Costas sync blocks of
    // the signal starting at hz and off?
    //
    float one_strength(const std::vector<float> &samples200, float hz, int off);
    //
    // given a complete known signal's symbols in syms,
    // how strong is it? used to look for the best
    // offset and frequency at which to subtract a
    // decoded signal.
    //
    float one_strength_known(
        const std::vector<float> &samples,
        int rate,
        const std::vector<int> &syms,
        float hz,
        int off
    );
    int search_time_fine(
        const std::vector<float> &samples200,
        int off0,
        int offN,
        float hz,
        int gran,
        float &str
    );
    int search_time_fine_known(
        const std::vector<std::complex<float>> &bins,
        int rate,
        const std::vector<int> &syms,
        int off0,
        int offN,
        float hz,
        int gran,
        float &str
    );
    //
    // search for costas blocks in an MxN time/frequency grid.
    // hz0 +/- hz_win in hz_inc increments. hz0 should be near 25.
    // off0 +/- off_win in off_inc incremenents.
    //
    std::vector<Strength> search_both(
        const std::vector<float> &samples200,
        float hz0,
        int hz_n,
        float hz_win,
        int off0,
        int off_n,
        int off_win
    );
    void search_both_known(
        const std::vector<float> &samples,
        int rate,
        const std::vector<int> &syms,
        float hz0,
        float off_secs0, // seconds
        float &hz_out,
        float &off_out
    );
    //
    // shift frequency by shifting the bins of one giant FFT.
    // so no problem with phase mismatch &c at block boundaries.
    // surprisingly fast at 200 samples/second.
    // shifts *down* by hz.
    //
    std::vector<float> fft_shift(
        const std::vector<float> &samples,
        int off,
        int len,
        int rate,
        float hz
    );
    //
    // shift down by hz.
    //
    std::vector<float> fft_shift_f(
        const std::vector<std::complex<float>> &bins,
        int rate,
        float hz
    );
    // shift the frequency by a fraction of 6.25,
    // to center hz on bin 4 (25 hz).
    std::vector<float> shift200(
        const std::vector<float> &samples200,
        int off,
        int len,
        float hz
    );
    // returns a mini-FFT of 79 8-tone symbols.
    FFTEngine::ffts_t extract(const std::vector<float> &samples200, float, int off);
    //
    // m79 is a 79x8 array of complex.
    //
    FFTEngine::ffts_t un_gray_code_c(const FFTEngine::ffts_t &m79);
    //
    // m79 is a 79x8 array of float.
    //
    std::vector<std::vector<float>> un_gray_code_r(const std::vector<std::vector<float>> &m79);
    //
    // normalize levels by windowed median.
    // this helps, but why?
    //
    std::vector<std::vector<float>> convert_to_snr(const std::vector<std::vector<float>> &m79);
    //
    // normalize levels by windowed median.
    // this helps, but why?
    //
    std::vector<std::vector<std::complex<float>>> c_convert_to_snr(
        const std::vector<std::vector<std::complex<float>>> &m79
    );
    //
    // statistics to decide soft probabilities,
    // to drive LDPC decoder.
    // distribution of strongest tones, and
    // distribution of noise.
    //
    void make_stats(
        const std::vector<std::vector<float>> &m79,
        Stats &bests,
        Stats &all
    );
    //
    // convert 79x8 complex FFT bins to magnitudes.
    //
    // exploits local phase coherence by decreasing magnitudes of bins
    // whose phase is far from the phases of nearby strongest tones.
    //
    // relies on each tone being reasonably well centered in its FFT bin
    // (in time and frequency) so that each tone completes an integer
    // number of cycles and thus preserves phase from one symbol to the
    // next.
    //
    std::vector<std::vector<float>> soft_c2m(const FFTEngine::ffts_t &c79);
    //
    // guess the probability that a bit is zero vs one,
    // based on strengths of strongest tones that would
    // give it those values. for soft LDPC decoding.
    //
    // returns log-likelihood, zero is positive, one is negative.
    //
    float bayes(
        float best_zero,
        float best_one,
        int lli,
        Stats &bests,
        Stats &all
    );
    //
    // c79 is 79x8 complex tones, before un-gray-coding.
    //
    void soft_decode(const FFTEngine::ffts_t &c79, float ll174[]);
    //
    // c79 is 79x8 complex tones, before un-gray-coding.
    //
    void c_soft_decode(const FFTEngine::ffts_t &c79x, float ll174[]);
    //
    // turn 79 symbol numbers into 174 bits.
    // strip out the three Costas sync blocks,
    // leaving 58 symbol numbers.
    // each represents three bits.
    // (all post-un-gray-code).
    // str is per-symbol strength; must be positive.
    // each returned element is < 0 for 1, > 0 for zero,
    // scaled by str.
    //
    std::vector<float> extract_bits(const std::vector<int> &syms, const std::vector<float> str);
    // decode successive pairs of symbols. exploits the likelyhood
    // that they have the same phase, by summing the complex
    // correlations for each possible pair and using the max.
    void soft_decode_pairs(
        const FFTEngine::ffts_t &m79x,
        float ll174[]
    );
    void soft_decode_triples(
        const FFTEngine::ffts_t &m79x,
        float ll174[]
    );
    //
    // given log likelyhood for each bit, try LDPC and OSD decoders.
    // on success, puts corrected 174 bits into a174[].
    //
    int decode(const float ll174[], int a174[], int use_osd, std::string &comment);
    //
    // bandpass filter some FFT bins.
    // smooth transition from stop-band to pass-band,
    // so that it's not a brick-wall filter, so that it
    // doesn't ring.
    //
    std::vector<std::complex<float>> fbandpass(
        const std::vector<std::complex<float>> &bins0,
        float bin_hz,
        float low_outer,  // start of transition
        float low_inner,  // start of flat area
        float high_inner, // end of flat area
        float high_outer  // end of transition
    );
    //
    // move hz down to 25, filter+convert to 200 samples/second.
    //
    // like fft_shift(). one big FFT, move bins down and
    // zero out those outside the band, then IFFT,
    // then re-sample.
    //
    // XXX maybe merge w/ fft_shift() / shift200().
    //
    std::vector<float> down_v7(const std::vector<float> &samples, float hz);
    std::vector<float> down_v7_f(const std::vector<std::complex<float>> &bins, int len, float hz);
    //
    // putative start of signal is at hz and symbol si0.
    //
    // return 2 if it decodes to a brand-new message.
    // return 1 if it decodes but we've already seen it,
    //   perhaps in a different pass.
    // return 0 if we could not decode.
    //
    // XXX merge with one_iter().
    //
    int one_merge(const std::vector<std::complex<float>> &bins, int len, float hz, int off);
    // return 2 if it decodes to a brand-new message.
    // return 1 if it decodes but we've already seen it,
    //   perhaps in a different pass.
    // return 0 if we could not decode.
    int one_iter(const std::vector<float> &samples200, int best_off, float hz_for_cb);
    //
    // estimate SNR, yielding numbers vaguely similar to WSJT-X.
    // m79 is a 79x8 complex FFT output.
    //
    float guess_snr(const FFTEngine::ffts_t &m79);
    //
    // compare phases of successive symbols to guess whether
    // the starting offset is a little too high or low.
    // we expect each symbol to have the same phase.
    // an error in causes the phase to advance at a steady rate.
    // so if hz is wrong, we expect the phase to advance
    // or retard at a steady pace.
    // an error in offset causes each symbol to start at
    // a phase that depends on the symbol's frequency;
    // a particular offset error causes a phase error
    // that depends on frequency.
    // hz0 is actual FFT bin number of m79[...][0] (always 4).
    //
    // the output adj_hz is relative to the FFT bin center;
    // a positive number means the real signal seems to be
    // a bit higher in frequency that the bin center.
    //
    // adj_off is the amount to change the offset, in samples.
    // should be subtracted from offset.
    //
    void fine(const FFTEngine::ffts_t &m79, int, float &adj_hz, float &adj_off);
    //
    // subtract a corrected decoded signal from nsamples_,
    // perhaps revealing a weaker signal underneath,
    // to be decoded in a subsequent pass.
    //
    // re79[] holds the error-corrected symbol numbers.
    //
    void subtract(
        const std::vector<int> re79,
        float hz0,
        float hz1,
        float off_sec
    );
    //
    // decode, give to callback, and subtract.
    //
    // return 2 if it decodes to a brand-new message.
    // return 1 if it decodes but we've already seen it,
    //   perhaps in a different pass.
    // return 0 if we could not decode.
    //
    int try_decode(
        const std::vector<float> &samples200,
        float ll174[174],
        float best_hz,
        int best_off_samples,
        float hz0_for_cb,
        float,
        int use_osd,
        const char *comment1,
        const FFTEngine::ffts_t &m79
    );
    //
    // given 174 bits corrected by LDPC, work
    // backwards to the symbols that must have
    // been sent.
    // used to help ensure that subtraction subtracts
    // at the right place.
    //
    std::vector<int> recode(int a174[]);
    //
    // the signal is at roughly 25 hz in samples200.
    //
    // return 2 if it decodes to a brand-new message.
    // return 1 if it decodes but we've already seen it,
    //   perhaps in a different pass.
    // return 0 if we could not decode.
    //
    int one_iter1(
        const std::vector<float> &samples200x,
        int best_off,
        float best_hz,
        float hz0_for_cb,
        float hz1_for_cb
    );

signals:
    void finished();
private:
    FT8Params params;
    FFTEngine *fftEngine_;
    int npasses_;
    static const double apriori174[];

    float min_hz_;
    float max_hz_;
    std::vector<float> samples_;  // input to each pass
    std::vector<float> nsamples_; // subtract from here

    int start_;             // sample number of 0.5 seconds into samples[]
    int rate_;              // samples/second
    double deadline_;       // start time + budget
    double final_deadline_; // keep going this long if no decodes
    std::vector<int> hints1_;
    std::vector<int> hints2_;
    int pass_;
    float down_hz_;

    QMutex cb_mu_;
    CallbackInterface *cb_; // call-back interface

    QMutex hack_mu_;
    int hack_size_;
    int hack_off_;
    int hack_len_;
    float hack_0_;
    float hack_1_;
    const float *hack_data_;
    std::vector<std::complex<float>> hack_bins_;
    std::vector<cdecode> prevdecs_;
}; // class FT8

class FT8_API FT8Decoder : public QObject {
    Q_OBJECT
public:
    ~FT8Decoder();
    void entry(
        float xsamples[],
        int nsamples,
        int start,
        int rate,
        float min_hz,
        float max_hz,
        int hints1[],
        int hints2[],
        double time_left,
        double total_time_left,
        CallbackInterface *cb,
        int,
        struct cdecode *
    );
    void wait(double time_left); //!< wait for all threads to finish
    void forceQuit(); //!< force quit all threads
    FT8Params& getParams() { return params; }
private:
    FT8Params params;
    std::vector<QThread*> threads;
    std::vector<FFTEngine*> fftEngines;
}; // FT8Decoder

} // namespace FT8

#endif
