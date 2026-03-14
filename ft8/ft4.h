///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                           //
//                                                                               //
// Experimental FT4 decoder scaffold derived from FT8 decoder architecture.      //
///////////////////////////////////////////////////////////////////////////////////
#ifndef ft4_h
#define ft4_h

#include <vector>

#include <QObject>

#include "ft8.h"
#include "export.h"

class QThread;

namespace FT8 {

// FT4 characteristics:
// 576-point FFT at 12000 samples/second
// 23.4 Hz spacing, 0.048 seconds/symbol
// encode chain:
//   77 bits
//   append 14 bits CRC (for 91 bits)
//   LDPC(174,91) yields 174 bits
//   that's 87 2-bit FSK-4 symbols
//   gray code each 2 bits
//   insert four 4-symbol Costas sync arrays
//     at positions: 0-3, 33-36, 66-69, 99-102
//   thus: 103 FSK-4 symbols
//   add 2 ramp symbols at start and end to make 105 symbols
// total transmission time is 5.04 seconds

// The FT4 worker
class FT8_API FT4 : public QObject
{
    Q_OBJECT
public:
    FT4(
        const std::vector<float> &samples,
        float min_hz,
        float max_hz,
        int start,
        int rate,
        const int hints1[],
        const int hints2[],
        double deadline,
        double final_deadline,
        CallbackInterface *cb,
        const std::vector<cdecode>& prevdecs,
        FFTEngine *fftEngine
    );
    ~FT4() override = default;
    // Number of passes
    void set_npasses(int npasses) { npasses_ = npasses; }
    // Start the worker
    void start_work();
    // strength of costas block of signal with tone 0 at bi0,
    // and symbol zero at si0.
    float one_coarse_strength(const FFTEngine::ffts_t &bins, int bi0, int si0) const;
    // return symbol length in samples at the given rate.
    // insist on integer symbol lengths so that we can
    // use whole FFT bins.
    int blocksize(int rate) const;
    //
    // look for potential signals by searching FFT bins for Costas symbol
    // blocks. returns a vector of candidate positions.
    //
    std::vector<Strength> coarse(const FFTEngine::ffts_t &bins, int si0, int si1);

    FT8Params& getParams() { return params; }
    //
    // given log likelihood for each bit, try LDPC and OSD decoders.
    // on success, puts corrected 174 bits into a174[].
    //
    static int decode(const float ll174[], int a174[], const FT8Params& params, int use_osd, std::string &comment);
    // encode a 77 bit message into a 174 bit payload
    // adds the 14 bit CRC to obtain 91 bits
    // apply (174, 91) generator mastrix to obtain the 83 parity bits
    // append the 83 bits to the 91 bits message e+ crc to obtain the 174 bit payload
    static void encode(int a174[], const int s77[]);

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
    FFTEngine::ffts_t un_gray_code_c(const FFTEngine::ffts_t &m79) const;
    //
    // m79 is a 79x8 array of float.
    //
    std::vector<std::vector<float>> un_gray_code_r(const std::vector<std::vector<float>> &m79) const;
    //
    // normalize levels by windowed median.
    // this helps, but why?
    //
    std::vector<std::vector<float>> convert_to_snr(const std::vector<std::vector<float>> &m79) const;
    // normalize levels by windowed median.
    // this helps, but why?
    //
    std::vector<std::vector<std::complex<float>>> c_convert_to_snr(
        const std::vector<std::vector<std::complex<float>>> &m79
    ) const;
    //
    // statistics to decide soft probabilities,
    // to drive LDPC decoder.
    // distribution of strongest tones, and
    // distribution of noise.
    //
    static void make_stats(
        const std::vector<std::vector<float>> &m79,
        Stats &bests,
        Stats &all
    );
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
    std::vector<std::vector<float>> soft_c2m(const FFTEngine::ffts_t &c79) const;
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
    std::vector<float> extract_bits(const std::vector<int> &syms, const std::vector<float>& str) const;
    // decode successive pairs of symbols. exploits the likelihood
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
    ) const;
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
    // m103 is a 103x4 complex FFT output.
    //
    float guess_snr(const FFTEngine::ffts_t &m103) const;
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
    // hz0 is actual FFT bin number of m103[...][0].
    //
    // the output adj_hz is relative to the FFT bin center;
    // a positive number means the real signal seems to be
    // a bit higher in frequency that the bin center.
    //
    // adj_off is the amount to change the offset, in samples.
    // should be subtracted from offset.
    //
    void fine(const FFTEngine::ffts_t &m103, int, float &adj_hz, float &adj_off) const;
    //
    // subtract a corrected decoded signal from nsamples_,
    // perhaps revealing a weaker signal underneath,
    // to be decoded in a subsequent pass.
    //
    // re103[] holds the error-corrected symbol numbers.
    //
    void subtract(
        const std::vector<int>& re103,
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
        const FFTEngine::ffts_t &m103
    );
    //
    // given 174 bits corrected by LDPC, work
    // backwards to the symbols that must have
    // been sent.
    // used to help ensure that subtraction subtracts
    // at the right place.
    //
    std::vector<int> recode(const int a174[]) const;
    //
    // the signal is centered around 46.8 Hz in samples200.
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
}; // class FT4


class FT8_API FT4Decoder : public QObject {
    Q_OBJECT
public:
    ~FT4Decoder();
    void entry(
        const float xsamples[],
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
        const struct cdecode*
    );
    void wait(double time_left); //!< wait for all threads to finish
    void forceQuit(); //!< force quit all threads
    FT8Params& getParams() { return params; }
private:
    FT8Params params;
    std::vector<QThread*> threads;
    std::vector<FFTEngine*> fftEngines;
}; // FT4Decoder


} // namespace FT8

#endif
