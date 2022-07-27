#include "FreqDevEstimator.h"

namespace modemm17 {

const std::array<float, 3> FreqDevEstimator::dc_b = { 0.09763107,  0.19526215,  0.09763107 };
const std::array<float, 3> FreqDevEstimator::dc_a = { 1.        , -0.94280904,  0.33333333 };
const float FreqDevEstimator::MAX_DC_ERROR = 0.2f;

} // namespace modemm17
