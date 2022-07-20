// Copyright 2020 Mobilinkd LLC.

#pragma once

#include "FirFilter.h"
#include "PhaseEstimator.h"
#include "DeviationError.h"
#include "FrequencyError.h"
#include "SymbolEvm.h"

#include <array>
#include <tuple>

namespace modemm17
{

namespace detail
{
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

static const auto evm_b = std::array<float, 3>{0.02008337, 0.04016673, 0.02008337};
static const auto evm_a = std::array<float, 3>{1.0, -1.56101808, 0.64135154};
} // detail

struct Fsk4Demod
{
    using demod_result_t = std::tuple<double, double, int, double>;
    using result_t = std::tuple<double, double, int, double, double, double, double>;

    BaseFirFilter<std::tuple_size<decltype(detail::rrc_taps)>::value> rrc = makeFirFilter(detail::rrc_taps);
    PhaseEstimator phase = PhaseEstimator(48000, 4800);
    DeviationError<10> deviation;
    FrequencyError<32> frequency;
    SymbolEvm<std::tuple_size<decltype(detail::evm_b)>::value> symbol_evm = makeSymbolEvm(makeIirFilter(detail::evm_b, detail::evm_a));

    double sample_rate = 48000;
    double symbol_rate = 4800;
    double unlock_gain = 0.02;
    double lock_gain = 0.001;
    std::array<float, 3> samples{0};
    double t = 0;
    double dt = symbol_rate / sample_rate;
    double ideal_dt = dt;
    bool sample_now = false;
    double estimated_deviation = 1.0;
    double estimated_frequency_offset = 0.0;
    double evm_average = 0.0;

    Fsk4Demod(
        double sample_rate,
        double symbol_rate,
        double unlock_gain = 0.02,
        double lock_gain = 0.001
    ) :
        sample_rate(sample_rate),
        symbol_rate(symbol_rate),
        unlock_gain(unlock_gain * symbol_rate / sample_rate),
        lock_gain(lock_gain * symbol_rate / sample_rate),
        dt(symbol_rate / sample_rate),
        ideal_dt(dt)
    {
        samples.fill(0.0);
    }

    demod_result_t demod(bool lock)
    {
        estimated_deviation = deviation(samples[1]);
        for (auto& sample : samples) sample *= estimated_deviation;

        estimated_frequency_offset = frequency(samples[1]);
        for (auto& sample : samples) sample -= estimated_frequency_offset;

        auto phase_estimate = phase(samples);
        if (samples[1] < 0) phase_estimate *= -1;

        dt = ideal_dt - (phase_estimate * (lock ? lock_gain : unlock_gain));
        t += dt;

        symbol_evm(samples[1]);
        int symbol;
        float evm;
        std::tie(symbol, evm) = symbol_evm(samples[1]);
        evm_average = symbol_evm.evm();
        samples[0] = samples[2];

        return std::make_tuple(samples[1], phase_estimate, symbol, evm);
    }

    /**
     * Process the sample.  If a symbol is ready, return a tuple
     * containing the sample used, the estimated phase, the decoded
     * symbol, the EVM, the deviation error and the frequency error
     * (sample, phase, symbol, evm, ed, ef), otherwise None.
     */
    result_t operator()(double sample, bool lock)
    {
        auto filtered_sample = rrc(sample);

        if (sample_now)
        {
            samples[2] = filtered_sample;
            sample_now = false;
            double prev_sample;
            double phase_estimate;
            int symbol;
            double evm;
            std::tie(prev_sample, phase_estimate, symbol, evm) = demod(lock);
            return std::make_tuple(
                prev_sample,
                phase_estimate,
                symbol,
                evm,
                estimated_deviation,
                estimated_frequency_offset,
                evm_average
            );
        }

        t += dt;
        if (t < 1.0)
        {
            samples[0] = filtered_sample;
        }
        else
        {
            t -= 1.0;
            samples[1] = filtered_sample;
            sample_now = true;
        }

        return result_t{0, 0, 0, 0, 0, 0, 0};
    }
};

} // modemm17
