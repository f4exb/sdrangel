#include "FreqDevEstimator.h"

namespace mobilinkd {

template<>
const std::array<double, 3> FreqDevEstimator<double>::dc_b = { 0.09763107,  0.19526215,  0.09763107 };

template<>
const std::array<double, 3> FreqDevEstimator<double>::dc_a = { 1.        , -0.94280904,  0.33333333 };

template<>
const std::array<float, 3> FreqDevEstimator<float>::dc_b = { 0.09763107,  0.19526215,  0.09763107 };

template<>
const std::array<float, 3> FreqDevEstimator<float>::dc_a = { 1.        , -0.94280904,  0.33333333 };

} // namespace mobilinkd
