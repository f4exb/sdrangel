#include "Correlator.h"

namespace modemm17 {

// IIR with Nyquist of 1/240.
const std::array<float,3> Correlator::b = {4.24433681e-05, 8.48867363e-05, 4.24433681e-05};
const std::array<float,3> Correlator::a = {1.0, -1.98148851,  0.98165828};

} // namespace modemm17
