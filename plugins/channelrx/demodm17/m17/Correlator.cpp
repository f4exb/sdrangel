#include "Correlator.h"

namespace mobilinkd {

// IIR with Nyquist of 1/240.
template<>
const std::array<double,3> Correlator<double>::b = {4.24433681e-05, 8.48867363e-05, 4.24433681e-05};

template<>
const std::array<double,3> Correlator<double>::a = {1.0, -1.98148851,  0.98165828};

template<>
const std::array<float,3> Correlator<float>::b = {4.24433681e-05, 8.48867363e-05, 4.24433681e-05};

template<>
const std::array<float,3> Correlator<float>::a = {1.0, -1.98148851,  0.98165828};

} // namespace mobilinkd
