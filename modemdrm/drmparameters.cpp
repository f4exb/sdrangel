///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "drmparameters.h"

#include <stdexcept>

namespace modemdrm
{

namespace
{

constexpr std::array<ModeParameters, 5> g_modes {{
    {Mode::A, 288, 32, 15, 3, 48000},
    {Mode::B, 256, 64, 15, 3, 48000},
    {Mode::C, 176, 64, 20, 3, 48000},
    {Mode::D, 112, 88, 24, 3, 48000},
    {Mode::E,  27,  3, 40, 4, 192000}
}};

// ETSI ES 201 980 V4.3.1, tables 48 and 49. Invalid mode/occupancy
// combinations are intentionally absent.
const std::vector<SpectrumParameters> g_spectra {
    {Mode::A, 0,   4500,    2, 102},
    {Mode::A, 1,   5000,    2, 114},
    {Mode::A, 2,   9000, -102, 102},
    {Mode::A, 3,  10000, -114, 114},
    {Mode::A, 4,  18000,  -98, 314},
    {Mode::A, 5,  20000, -110, 350},
    {Mode::B, 0,   4500,    1,  91},
    {Mode::B, 1,   5000,    1, 103},
    {Mode::B, 2,   9000,  -91,  91},
    {Mode::B, 3,  10000, -103, 103},
    {Mode::B, 4,  18000,  -87, 279},
    {Mode::B, 5,  20000,  -99, 311},
    {Mode::C, 3,  10000,  -69,  69},
    {Mode::C, 5,  20000,  -67, 213},
    {Mode::D, 3,  10000,  -44,  44},
    {Mode::D, 5,  20000,  -43, 135},
    {Mode::E, 0, 100000, -106, 106}
};

} // namespace

double ModeParameters::usefulDurationSeconds() const
{
    return static_cast<double>(usefulTimeUnits) / Parameters::TimeUnitsPerSecond;
}

double ModeParameters::guardDurationSeconds() const
{
    return static_cast<double>(guardTimeUnits) / Parameters::TimeUnitsPerSecond;
}

double ModeParameters::carrierSpacingHz() const
{
    return static_cast<double>(Parameters::TimeUnitsPerSecond) / usefulTimeUnits;
}

int ModeParameters::usefulSamples(int sampleRate) const
{
    const std::int64_t numerator = static_cast<std::int64_t>(sampleRate) * usefulTimeUnits;
    return numerator % Parameters::TimeUnitsPerSecond == 0
        ? static_cast<int>(numerator / Parameters::TimeUnitsPerSecond)
        : 0;
}

int ModeParameters::guardSamples(int sampleRate) const
{
    const std::int64_t numerator = static_cast<std::int64_t>(sampleRate) * guardTimeUnits;
    return numerator % Parameters::TimeUnitsPerSecond == 0
        ? static_cast<int>(numerator / Parameters::TimeUnitsPerSecond)
        : 0;
}

int ModeParameters::symbolSamples(int sampleRate) const
{
    const int useful = usefulSamples(sampleRate);
    const int guard = guardSamples(sampleRate);
    return useful > 0 && guard > 0 ? useful + guard : 0;
}

bool Parameters::acquisitionBand(Mode mode, int occupancy, int configuredRangeHz,
    bool spectralInversion, double& lowerHz, double& upperHz)
{
    const auto *spectrum = forSpectrum(mode, occupancy);
    if (!spectrum || !isSearchRangeValid(configuredRangeHz)) {
        return false;
    }

    const double spacing = forMode(mode).carrierSpacingHz();
    const double lower = (spectralInversion ? -spectrum->maximumCarrier : spectrum->minimumCarrier) * spacing;
    const double upper = (spectralInversion ? -spectrum->minimumCarrier : spectrum->maximumCarrier) * spacing;
    const double margin = effectiveSearchRangeHz(mode, configuredRangeHz) + spacing / 2.0;
    lowerHz = lower - margin;
    upperHz = upper + margin;
    return true;
}

bool Parameters::spectrumWithinBand(Mode mode, int occupancy, double lowHz, double highHz,
    int configuredRangeHz, bool spectralInversion)
{
    double lower = 0.0;
    double upper = 0.0;
    return acquisitionBand(mode, occupancy, configuredRangeHz, spectralInversion, lower, upper)
        && lower > lowHz && upper < highHz;
}

const ModeParameters& Parameters::forMode(Mode mode)
{
    for (const ModeParameters& parameters : g_modes)
    {
        if (parameters.mode == mode) {
            return parameters;
        }
    }

    throw std::invalid_argument("Invalid DRM robustness mode");
}

const SpectrumParameters* Parameters::forSpectrum(Mode mode, int occupancy)
{
    for (const SpectrumParameters& parameters : g_spectra)
    {
        if (parameters.mode == mode && parameters.occupancy == occupancy) {
            return &parameters;
        }
    }

    return nullptr;
}

const std::array<ModeParameters, 5>& Parameters::modes()
{
    return g_modes;
}

const std::vector<SpectrumParameters>& Parameters::spectra()
{
    return g_spectra;
}

int Parameters::sdcDataFieldBytes(Mode mode, int occupancy, int sdcMode)
{
    if ((sdcMode != 0 && sdcMode != 1) || !forSpectrum(mode, occupancy)) {
        return -1;
    }

    // ETSI ES 201 980 V4.3.1, table 21. The columns are spectrum
    // occupancies 0..5; -1 marks combinations which are not defined.
    static constexpr int lengths[5][2][6] = {
        {{37,43,85,97,184,207}, {17,20,41,47,91,102}},
        {{28,33,66,76,143,161}, {13,15,32,37,70,79}},
        {{-1,-1,-1,68,-1,147}, {-1,-1,-1,32,-1,72}},
        {{-1,-1,-1,33,-1,78}, {-1,-1,-1,15,-1,38}},
        {{113,-1,-1,-1,-1,-1}, {55,-1,-1,-1,-1,-1}}
    };
    return lengths[static_cast<int>(mode)][sdcMode][occupancy];
}

int Parameters::effectiveSearchRangeHz(Mode mode, int configuredRangeHz)
{
    if (!isSearchRangeValid(configuredRangeHz)) {
        return -1;
    }

    if (configuredRangeHz != 0) {
        return configuredRangeHz;
    }

    return mode == Mode::E ? DefaultSearchRangeEHz : DefaultSearchRangeADHz;
}

bool Parameters::isSearchRangeValid(int configuredRangeHz)
{
    return configuredRangeHz == 0
        || (configuredRangeHz >= MinimumSearchRangeHz && configuredRangeHz <= MaximumSearchRangeHz);
}

char Parameters::modeName(Mode mode)
{
    return static_cast<char>('A' + static_cast<int>(mode));
}

} // namespace modemdrm
