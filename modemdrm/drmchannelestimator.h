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

#ifndef MODEMDRM_DRMCHANNELESTIMATOR_H_
#define MODEMDRM_DRMCHANNELESTIMATOR_H_

#include <complex>
#include <map>
#include <vector>

#include "export.h"
#include "drmparameters.h"

namespace modemdrm
{

struct MODEMDRM_API EqualizedSymbol
{
    int minimumCarrier = 0;
    int maximumCarrier = -1;
    std::vector<std::complex<float>> channel;
    std::vector<std::complex<float>> cells;
    float pilotResidualPower = 0.0f;
    // The symbol's raw gain-pilot observations against the estimate at their
    // carriers: the error power y = |observation - estimate|^2 against
    // x = |estimate|^2 (from the time/frequency estimator: A-D, and mode E
    // with long echoes; see DRMReceiver::decodeMSC).
    struct PilotError
    {
        int carrier = 0;
        double x = 0.0;
        double y = 0.0;
    };
    std::vector<PilotError> pilotErrors;
    // Regression sums of y on x.
    struct PilotErrorSums
    {
        double count = 0.0, x = 0.0, y = 0.0, xx = 0.0, xy = 0.0;
        void add(const PilotError& error)
        {
            count += 1.0;
            x += error.x;
            y += error.y;
            xx += error.x * error.x;
            xy += error.x * error.y;
        }

        void add(const std::vector<PilotError>& errors)
        {
            for (const PilotError& error : errors) {
                add(error);
            }
        }
    };
    bool contains(int carrier) const;
    const std::complex<float>& cell(int carrier) const;
};

struct MODEMDRM_API PilotTimingEstimate
{
    bool valid = false;
    double timingShiftSamples = 0.0;
    double commonPhaseRadians = 0.0;
    float coherence = 0.0f;
};

struct MODEMDRM_API ChannelWienerStatistics
{
    float temporalCorrelation = 0.0f;
    float delayNoiseRatio = 0.0f;
};

class MODEMDRM_API ChannelEstimator
{
public:
    static bool estimate(
        Mode mode,
        int occupancy,
        int symbol,
        int frameInSuperframe,
        int integerCarrierOffset,
        const std::vector<std::complex<float>>& fftBins,
        EqualizedSymbol& result
    );
};

struct MODEMDRM_API ChannelPilotObservation
{
    int sequenceSymbol;
    std::complex<float> channel;
};

// The per-symbol pilot estimates of a chronological frame window, computed
// once and shared by every frame equalized from that window (in mode E by
// frequency Wiener interpolation of each symbol's reference cells, otherwise
// linearly). The window's spectra must outlive the context.
class MODEMDRM_API ChannelEstimationContext
{
public:
    bool prepare(
        Mode mode,
        int occupancy,
        int firstFrameInSuperframe,
        int integerCarrierOffset,
        const std::vector<std::vector<std::vector<std::complex<float>>>>& fftFrames
    );
    // Equalizes one frame using the window's pilot estimates; targetFrame
    // indexes fftFrames.
    bool estimateFrame(int targetFrame, std::vector<EqualizedSymbol>& result, int temporalRadius = 3) const;
    // Interpolates the repeated gain-pilot lattice to the target symbol in
    // time (A-D: a Wiener filter on a Gaussian correlation fitted to the
    // pilots; mode E: a regression weighted by their measured correlation),
    // then applies delay-domain scalar Wiener shrinkage before DFT
    // interpolation in frequency. In mode E it declines (returns false)
    // unless the channel has echoes beyond the delay span each symbol's
    // pilots resolve.
    bool estimateFrameWiener(
        int targetFrame,
        std::vector<EqualizedSymbol>& result,
        ChannelWienerStatistics* statistics = nullptr
    );

    // Mode E: the delay-profile energy of the time-interpolated pilot grid
    // beyond the window each symbol's own pilots are interpolated over, and
    // away from that window's images at the pilot pattern period, relative
    // to the energy in it. Negative when it cannot be measured.
    double longEchoFraction();

private:
    bool prepareWiener();

    Mode m_mode = Mode::A;
    int m_occupancy = 0;
    int m_firstFrameInSuperframe = 0;
    int m_integerCarrierOffset = 0;
    const std::vector<std::vector<std::vector<std::complex<float>>>>* m_frames = nullptr;
    bool m_valid = false;
    std::vector<EqualizedSymbol> m_initial;
    int m_wienerState = 0; // 0 unprepared, 1 prepared, -1 unusable
    std::vector<int> m_pilotGrid;
    int m_pilotSpacing = 0;
    std::vector<std::vector<ChannelPilotObservation>> m_histories; // by pilotGrid position
    float m_temporalCorrelation = 0.8f;
    int m_pilotPeriod = 0;           // symbols between observations of a pilot carrier
    double m_phaseRate = 0.0;        // common phase advance per symbol
    double m_pilotCorrelation = 1.0; // pilots' lag-two to lag-one correlation ratio
    double m_pilotLag1 = 1.0;        // measured correlation one pilot period apart, with noise
    bool m_haveDelayWindow = false;  // mode E: the per-symbol interpolation window
    double m_delayCentre = 0.0;      // samples of the useful period
    double m_delayWidth = 0.0;
    int m_timeInterpolation = 0;     // mode E: 0 undecided, 1 time/frequency Wiener, -1 per symbol
    // A-D time-Wiener weights by the pattern of observation times; the model
    // is the context's, so every frame equalized from it shares them.
    std::map<std::vector<int>, std::vector<std::complex<double>>> m_timeWienerWeights;
};

class MODEMDRM_API PilotTimingTracker
{
public:
    // A change in FFT-window position appears as a linear phase ramp across
    // the pilot-derived channel response. The common phase term absorbs
    // residual carrier rotation, leaving the slope as the timing change.
    static PilotTimingEstimate compare(
        const EqualizedSymbol& previous,
        const EqualizedSymbol& current,
        int usefulSamples
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMCHANNELESTIMATOR_H_
